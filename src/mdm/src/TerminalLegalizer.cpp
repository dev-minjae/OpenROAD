// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "TerminalLegalizer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <unordered_set>
#include <utility>

#include "utl/Logger.h"

namespace mdm {

TerminalLegalizer::TerminalLegalizer(utl::Logger* logger, const Config& cfg)
    : logger_(logger), cfg_(cfg)
{
}

void TerminalLegalizer::addTerminal(std::string name,
                                    int tor_xlo,
                                    int tor_ylo,
                                    int tor_xhi,
                                    int tor_yhi)
{
  Terminal t;
  t.name = std::move(name);
  t.tor_xlo = tor_xlo;
  t.tor_xhi = tor_xhi;
  t.tor_ylo = tor_ylo;
  t.tor_yhi = tor_yhi;
  terminals_.push_back(std::move(t));
}

int64_t TerminalLegalizer::costToTor(int gx, int gy, const Terminal& t) const
{
  int64_t dx = 0;
  if (gx < t.tor_xlo) {
    dx = static_cast<int64_t>(t.tor_xlo) - gx;
  } else if (gx > t.tor_xhi) {
    dx = static_cast<int64_t>(gx) - t.tor_xhi;
  }
  int64_t dy = 0;
  if (gy < t.tor_ylo) {
    dy = static_cast<int64_t>(t.tor_ylo) - gy;
  } else if (gy > t.tor_yhi) {
    dy = static_cast<int64_t>(gy) - t.tor_yhi;
  }
  return dx + dy;
}

int64_t TerminalLegalizer::costOnGrid(const Terminal& t, int gi, int gj) const
{
  return costToTor(gridCenterX(gi), gridCenterY(gj), t);
}

int TerminalLegalizer::torCenterGi(const Terminal& t) const
{
  const int tor_cx = (t.tor_xlo + t.tor_xhi) / 2;
  const int g = (tor_cx - cfg_.origin_x + cfg_.step_x / 2) / cfg_.step_x;
  return std::clamp(g, 0, cfg_.grid_w - 1);
}

int TerminalLegalizer::torCenterGj(const Terminal& t) const
{
  const int tor_cy = (t.tor_ylo + t.tor_yhi) / 2;
  const int g = (tor_cy - cfg_.origin_y + cfg_.step_y / 2) / cfg_.step_y;
  return std::clamp(g, 0, cfg_.grid_h - 1);
}

void TerminalLegalizer::legalize()
{
  if (terminals_.empty()) {
    return;
  }

  for (auto& term : terminals_) {
    term.gi = torCenterGi(term);
    term.gj = torCenterGj(term);
  }

  // Sort terminals by cost-to-TOR for their initial (nearest) grid cell,
  // descending. Strictest-constrained terminals (those whose TOR centre
  // is farthest from any grid cell, so any displacement matters most)
  // get first pick of grid cells; loose ones absorb residual collisions.
  // This is a poor-man's bipartite matching; Phase 1b replaces it with
  // LEMON network simplex.
  std::sort(terminals_.begin(),
            terminals_.end(),
            [&](const Terminal& a, const Terminal& b) {
              return costOnGrid(a, a.gi, a.gj) > costOnGrid(b, b.gi, b.gj);
            });

  // Spiral collision resolution in Chebyshev rings. Within each ring pick
  // the candidate that minimises cost-to-TOR, not the first one we
  // encounter — keeps cost linear in the displacement actually paid.
  auto cell_id = [&](int i, int j) {
    return static_cast<int64_t>(i) * static_cast<int64_t>(cfg_.grid_h) + j;
  };
  std::unordered_set<int64_t> taken;
  taken.reserve(terminals_.size() * 2);
  const int radius_max = std::max(cfg_.grid_w, cfg_.grid_h);
  for (auto& term : terminals_) {
    if (taken.insert(cell_id(term.gi, term.gj)).second) {
      continue;
    }
    bool placed = false;
    int64_t best_cost = std::numeric_limits<int64_t>::max();
    int best_ni = term.gi;
    int best_nj = term.gj;
    for (int radius = 1; radius <= radius_max && !placed; ++radius) {
      for (int di = -radius; di <= radius; ++di) {
        for (int dj = -radius; dj <= radius; ++dj) {
          if (std::max(std::abs(di), std::abs(dj)) != radius) {
            continue;
          }
          const int ni = term.gi + di;
          const int nj = term.gj + dj;
          if (ni < 0 || ni >= cfg_.grid_w || nj < 0 || nj >= cfg_.grid_h) {
            continue;
          }
          if (taken.contains(cell_id(ni, nj))) {
            continue;
          }
          const int64_t c = costOnGrid(term, ni, nj);
          if (c < best_cost) {
            best_cost = c;
            best_ni = ni;
            best_nj = nj;
            placed = true;
          }
        }
      }
    }
    if (placed) {
      taken.insert(cell_id(best_ni, best_nj));
      term.gi = best_ni;
      term.gj = best_nj;
    } else if (logger_) {
      logger_->warn(utl::MDM,
                    33,
                    "TerminalLegalizer: grid full for net {}; leaving on "
                    "conflicting cell.",
                    term.name);
    }
  }

  // Local refinement: pair-swap to lower total cost. Cap passes adaptively
  // so case4 (~43k terminals) does not blow up runtime — the O(m²) inner
  // loop dominates.
  const int swap_pass_cap
      = terminals_.size() < 5000 ? 4 : (terminals_.size() < 15000 ? 2 : 1);
  bool any_swap = true;
  int swap_passes = 0;
  while (any_swap && swap_passes < swap_pass_cap) {
    any_swap = false;
    ++swap_passes;
    for (size_t i = 0; i < terminals_.size(); ++i) {
      for (size_t j = i + 1; j < terminals_.size(); ++j) {
        const int64_t cost_now
            = costOnGrid(terminals_[i], terminals_[i].gi, terminals_[i].gj)
              + costOnGrid(terminals_[j], terminals_[j].gi, terminals_[j].gj);
        const int64_t cost_swap
            = costOnGrid(terminals_[i], terminals_[j].gi, terminals_[j].gj)
              + costOnGrid(terminals_[j], terminals_[i].gi, terminals_[i].gj);
        if (cost_swap < cost_now) {
          std::swap(terminals_[i].gi, terminals_[j].gi);
          std::swap(terminals_[i].gj, terminals_[j].gj);
          any_swap = true;
        }
      }
    }
  }
}

}  // namespace mdm
