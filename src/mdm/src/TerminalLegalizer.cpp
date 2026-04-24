// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "TerminalLegalizer.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "lemon/list_graph.h"
#include "lemon/network_simplex.h"
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

  if (cfg_.use_bipartite_matching && legalizeByMatching()) {
    // Matching returns the grid-restricted optimum within each terminal's
    // candidate window. A pair-swap pass on top can still pick up
    // improvements that require moving a terminal outside its window,
    // and is a no-op when the matching was globally optimal.
    runPairSwap();
    return;
  }

  legalizeGreedy();
}

bool TerminalLegalizer::legalizeByMatching()
{
  using Graph = lemon::ListDigraph;
  const int m = static_cast<int>(terminals_.size());
  const int64_t grid_capacity = static_cast<int64_t>(cfg_.grid_w) * cfg_.grid_h;
  if (grid_capacity < m) {
    if (logger_) {
      logger_->warn(utl::MDM,
                    35,
                    "TerminalLegalizer: grid has {} cells for {} terminals; "
                    "falling back to greedy.",
                    grid_capacity,
                    m);
    }
    return false;
  }

  // Grow the candidate window (TOR centre ± r grid cells) until the
  // matching is feasible. r=2 yields 5x5 = 25 candidates per terminal
  // (paper Table III δ=25). Paper reports this is always sufficient;
  // our fallback only triggers when many TORs pile onto the same grid
  // neighborhood with fewer than m distinct cells within radius r.
  for (int r = std::max(1, cfg_.candidate_radius);
       r <= cfg_.max_candidate_radius;
       ++r) {
    Graph g;
    Graph::Node src = g.addNode();
    Graph::Node sink = g.addNode();

    std::vector<Graph::Node> term_nodes;
    term_nodes.reserve(m);
    for (int j = 0; j < m; ++j) {
      term_nodes.push_back(g.addNode());
    }

    // Grid node cache — created on demand so we only pay for cells that
    // appear in at least one candidate set.
    std::unordered_map<int64_t, Graph::Node> grid_cache;
    grid_cache.reserve(static_cast<size_t>(m) * 4);
    auto grid_key = [this](int gi, int gj) {
      return static_cast<int64_t>(gi) * cfg_.grid_h + gj;
    };
    auto getGridNode = [&](int gi, int gj) {
      const int64_t key = grid_key(gi, gj);
      auto it = grid_cache.find(key);
      if (it != grid_cache.end()) {
        return it->second;
      }
      Graph::Node n = g.addNode();
      grid_cache.emplace(key, n);
      return n;
    };

    Graph::ArcMap<int> caps(g, 0);
    Graph::ArcMap<int64_t> costs(g, 0);
    // Parallel metadata: for a terminal→grid arc, record the destination
    // grid index so we can recover the assignment from the flow.
    Graph::ArcMap<int> arc_term_idx(g, -1);
    Graph::ArcMap<int> arc_gi(g, -1);
    Graph::ArcMap<int> arc_gj(g, -1);

    // src → terminal arcs
    for (int j = 0; j < m; ++j) {
      Graph::Arc a = g.addArc(src, term_nodes[j]);
      caps[a] = 1;
      costs[a] = 0;
    }

    // terminal → candidate grid arcs. The candidate window covers the
    // entire TOR rectangle (projected onto the grid) expanded by r cells
    // on every side. For nets with a large TOR this is wider than a
    // fixed k-window centred on the TOR's midpoint and avoids biasing
    // the matching toward one corner of the region.
    auto gridIndexX = [&](int x) {
      return std::clamp((x - cfg_.origin_x + cfg_.step_x / 2) / cfg_.step_x,
                        0,
                        cfg_.grid_w - 1);
    };
    auto gridIndexY = [&](int y) {
      return std::clamp((y - cfg_.origin_y + cfg_.step_y / 2) / cfg_.step_y,
                        0,
                        cfg_.grid_h - 1);
    };
    for (int j = 0; j < m; ++j) {
      const Terminal& t = terminals_[j];
      const int tor_gi_lo = gridIndexX(t.tor_xlo);
      const int tor_gi_hi = gridIndexX(t.tor_xhi);
      const int tor_gj_lo = gridIndexY(t.tor_ylo);
      const int tor_gj_hi = gridIndexY(t.tor_yhi);
      const int gi_lo = std::max(0, tor_gi_lo - r);
      const int gi_hi = std::min(cfg_.grid_w - 1, tor_gi_hi + r);
      const int gj_lo = std::max(0, tor_gj_lo - r);
      const int gj_hi = std::min(cfg_.grid_h - 1, tor_gj_hi + r);
      for (int gi = gi_lo; gi <= gi_hi; ++gi) {
        for (int gj = gj_lo; gj <= gj_hi; ++gj) {
          Graph::Node gn = getGridNode(gi, gj);
          Graph::Arc a = g.addArc(term_nodes[j], gn);
          caps[a] = 1;
          costs[a] = costOnGrid(t, gi, gj);
          arc_term_idx[a] = j;
          arc_gi[a] = gi;
          arc_gj[a] = gj;
        }
      }
    }

    // grid → sink arcs (cap=1 enforces one terminal per grid cell)
    for (auto& [_, gn] : grid_cache) {
      Graph::Arc a = g.addArc(gn, sink);
      caps[a] = 1;
      costs[a] = 0;
    }

    lemon::NetworkSimplex<Graph, int, int64_t> ns(g);
    ns.costMap(costs).upperMap(caps).stSupply(src, sink, m);
    const auto result = ns.run();
    if (result != lemon::NetworkSimplex<Graph, int, int64_t>::OPTIMAL) {
      if (logger_) {
        logger_->info(utl::MDM,
                      36,
                      "TerminalLegalizer: matching infeasible at radius {} "
                      "(status {}); growing window.",
                      r,
                      static_cast<int>(result));
      }
      continue;
    }

    Graph::ArcMap<int> flow(g);
    ns.flowMap(flow);
    for (Graph::ArcIt it(g); it != lemon::INVALID; ++it) {
      if (flow[it] > 0 && arc_term_idx[it] >= 0) {
        Terminal& t = terminals_[arc_term_idx[it]];
        t.gi = arc_gi[it];
        t.gj = arc_gj[it];
      }
    }
    if (logger_) {
      logger_->info(utl::MDM,
                    37,
                    "TerminalLegalizer: matched {} terminals via LEMON "
                    "network simplex (radius={}, grids touched={}).",
                    m,
                    r,
                    grid_cache.size());
    }
    return true;
  }

  return false;
}

void TerminalLegalizer::legalizeGreedy()
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

  runPairSwap();
}

void TerminalLegalizer::runPairSwap()
{
  // Cap passes adaptively so case4 (~43k terminals) does not blow up
  // runtime — the O(m²) inner loop dominates.
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
