// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "GlobalTierOptimizer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <queue>
#include <unordered_set>
#include <vector>

#include "odb/db.h"
#include "odb/geom.h"
#include "utl/Logger.h"

namespace mdm {

namespace {

// Pin partition for one net under the hypothetical state where moving_cell
// has flipped to to_block AND S has already flipped. "from"-side pins
// remain on from_block.
struct PinPartition
{
  std::vector<int> from_xs;
  std::vector<int> from_ys;
  std::vector<int> to_xs;
  std::vector<int> to_ys;
};

PinPartition classifyPins(odb::dbNet* net,
                          odb::dbInst* moving_cell,
                          const std::unordered_set<odb::dbInst*>& S,
                          bool count_moving_cell_as_to)
{
  PinPartition pp;
  for (auto* iterm : net->getITerms()) {
    odb::dbInst* inst = iterm->getInst();
    if (!inst) {
      continue;
    }
    int x = 0, y = 0;
    iterm->getAvgXY(&x, &y);
    const bool on_to
        = (count_moving_cell_as_to && inst == moving_cell) || S.count(inst) > 0;
    if (on_to) {
      pp.to_xs.push_back(x);
      pp.to_ys.push_back(y);
    } else {
      pp.from_xs.push_back(x);
      pp.from_ys.push_back(y);
    }
  }
  return pp;
}

bool isCrossing(const PinPartition& pp)
{
  return !pp.from_xs.empty() && !pp.to_xs.empty();
}

int64_t bboxHpwl(const std::vector<int>& xs, const std::vector<int>& ys)
{
  if (xs.empty()) {
    return 0;
  }
  auto x_minmax = std::minmax_element(xs.begin(), xs.end());
  auto y_minmax = std::minmax_element(ys.begin(), ys.end());
  return static_cast<int64_t>(*x_minmax.second - *x_minmax.first)
         + static_cast<int64_t>(*y_minmax.second - *y_minmax.first);
}

// HPWL of the net under a given pin partition. If both sides have pins,
// inserts a TOR-center terminal and returns the merged 3D HPWL (matches
// the formula used by MultiDieManager::get3DHPWL and CellsLegalizer's
// pairNetsHPWL). Single-die nets are just bbox HPWL.
int64_t netHpwl(const PinPartition& pp)
{
  const bool has_from = !pp.from_xs.empty();
  const bool has_to = !pp.to_xs.empty();
  if (has_from && !has_to) {
    return bboxHpwl(pp.from_xs, pp.from_ys);
  }
  if (!has_from && has_to) {
    return bboxHpwl(pp.to_xs, pp.to_ys);
  }
  if (!has_from && !has_to) {
    return 0;
  }
  // Cross-die: TOR is the inner two of sorted endpoint quartet (x and y
  // independently). Place terminal at TOR center, merge into each die's
  // bbox.
  auto from_x = std::minmax_element(pp.from_xs.begin(), pp.from_xs.end());
  auto from_y = std::minmax_element(pp.from_ys.begin(), pp.from_ys.end());
  auto to_x = std::minmax_element(pp.to_xs.begin(), pp.to_xs.end());
  auto to_y = std::minmax_element(pp.to_ys.begin(), pp.to_ys.end());

  std::array<int, 4> xs_q{
      *from_x.first, *from_x.second, *to_x.first, *to_x.second};
  std::array<int, 4> ys_q{
      *from_y.first, *from_y.second, *to_y.first, *to_y.second};
  std::sort(xs_q.begin(), xs_q.end());
  std::sort(ys_q.begin(), ys_q.end());
  const int tor_xc = (xs_q[1] + xs_q[2]) / 2;
  const int tor_yc = (ys_q[1] + ys_q[2]) / 2;

  const int from_x_lo = std::min(*from_x.first, tor_xc);
  const int from_x_hi = std::max(*from_x.second, tor_xc);
  const int from_y_lo = std::min(*from_y.first, tor_yc);
  const int from_y_hi = std::max(*from_y.second, tor_yc);
  const int to_x_lo = std::min(*to_x.first, tor_xc);
  const int to_x_hi = std::max(*to_x.second, tor_xc);
  const int to_y_lo = std::min(*to_y.first, tor_yc);
  const int to_y_hi = std::max(*to_y.second, tor_yc);

  return static_cast<int64_t>(from_x_hi - from_x_lo)
         + static_cast<int64_t>(from_y_hi - from_y_lo)
         + static_cast<int64_t>(to_x_hi - to_x_lo)
         + static_cast<int64_t>(to_y_hi - to_y_lo);
}

}  // namespace

GlobalTierOptimizer::GlobalTierOptimizer(odb::dbDatabase* db,
                                         utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

int64_t GlobalTierOptimizer::cellAreaDbu(odb::dbInst* cell)
{
  return static_cast<int64_t>(cell->getMaster()->getWidth())
         * static_cast<int64_t>(cell->getMaster()->getHeight());
}

GlobalTierOptimizer::MoveDelta GlobalTierOptimizer::evaluateMove(
    odb::dbInst* cell,
    const Context& ctx) const
{
  MoveDelta md;
  std::unordered_set<odb::dbNet*> seen;
  for (auto* iterm : cell->getITerms()) {
    odb::dbNet* net = iterm->getNet();
    if (!net) {
      continue;
    }
    if (!seen.insert(net).second) {
      continue;  // multi-pin same-net iterms — count once
    }
    if (static_cast<int>(net->getITerms().size()) > ctx.params.max_net_fanout) {
      continue;  // skip clock-tree-like huge nets
    }
    PinPartition pp_before
        = classifyPins(net, cell, ctx.S, /*count_moving_cell_as_to=*/false);
    PinPartition pp_after
        = classifyPins(net, cell, ctx.S, /*count_moving_cell_as_to=*/true);
    md.delta_wl += netHpwl(pp_after) - netHpwl(pp_before);
    md.delta_term
        += (isCrossing(pp_after) ? 1 : 0) - (isCrossing(pp_before) ? 1 : 0);
  }
  return md;
}

double GlobalTierOptimizer::overflow(const Context& ctx) const
{
  // Paper Eq 10b: d(S) = Σ_{region r} max((A_r − M_r)/h_r, 0).
  // Single-bin per die. Uses cached from_total_area, to_existing_area,
  // s_total_area for O(1) work.
  const int64_t from_used = ctx.from_total_area - ctx.s_total_area;
  const int64_t to_used = ctx.to_existing_area + ctx.s_total_area;
  const int64_t from_over = std::max<int64_t>(from_used - ctx.cap_from_dbu, 0);
  const int64_t to_over = std::max<int64_t>(to_used - ctx.cap_t_dbu, 0);
  return static_cast<double>(from_over + to_over)
         / static_cast<double>(ctx.row_height);
}

double GlobalTierOptimizer::surrogateDelta(odb::dbInst* cell,
                                           const Context& ctx) const
{
  const MoveDelta md = evaluateMove(cell, ctx);
  const double d_before = overflow(ctx);
  // d_after: shift cell's area from "from" bin to "to" bin.
  const int64_t c_area = cellAreaDbu(cell);
  const int64_t from_used_after
      = ctx.from_total_area - ctx.s_total_area - c_area;
  const int64_t to_used_after
      = ctx.to_existing_area + ctx.s_total_area + c_area;
  const int64_t from_over_after
      = std::max<int64_t>(from_used_after - ctx.cap_from_dbu, 0);
  const int64_t to_over_after
      = std::max<int64_t>(to_used_after - ctx.cap_t_dbu, 0);
  const double d_after = static_cast<double>(from_over_after + to_over_after)
                         / static_cast<double>(ctx.row_height);

  return static_cast<double>(md.delta_wl)
         + ctx.params.rho * static_cast<double>(md.delta_term)
         + ctx.params.alpha * (d_after - d_before)
         - ctx.params.gamma * d_before;
}

bool GlobalTierOptimizer::fitsKnapsack(odb::dbInst* cell,
                                       const Context& ctx) const
{
  const int64_t to_area
      = ctx.to_existing_area + ctx.s_total_area + cellAreaDbu(cell);
  const int64_t B = static_cast<int64_t>(static_cast<double>(ctx.cap_t_dbu)
                                         * ctx.params.B_factor);
  return to_area <= B;
}

std::vector<odb::dbInst*> GlobalTierOptimizer::run(odb::dbBlock* from_block,
                                                   odb::dbBlock* to_block,
                                                   const TierOptParams& params)
{
  std::vector<odb::dbInst*> result;
  if (!from_block || !to_block) {
    logger_->warn(utl::MDM,
                  307,
                  "GlobalTierOptimizer::run: missing from_block or to_block.");
    return result;
  }

  Context ctx;
  ctx.from_block = from_block;
  ctx.to_block = to_block;
  ctx.params = params;

  // Knapsack caps. Prefer the caller-supplied dbu values; fall back to
  // u_*_percent × core_area if caller left them at zero.
  if (params.cap_from_dbu > 0 && params.cap_to_dbu > 0) {
    ctx.cap_from_dbu = params.cap_from_dbu;
    ctx.cap_t_dbu = params.cap_to_dbu;
  } else {
    odb::Rect core = to_block->getCoreArea();
    int64_t core_area
        = static_cast<int64_t>(core.dx()) * static_cast<int64_t>(core.dy());
    if (core_area == 0) {
      odb::Rect from_core = from_block->getCoreArea();
      core_area = static_cast<int64_t>(from_core.dx())
                  * static_cast<int64_t>(from_core.dy());
    }
    ctx.cap_from_dbu = core_area * params.u_t_percent / 100;  // assume from=top
    ctx.cap_t_dbu = core_area * params.u_b_percent / 100;  // assume to=bottom
  }
  ctx.from_total_area = 0;
  for (auto* c : from_block->getInsts()) {
    ctx.from_total_area += cellAreaDbu(c);
  }
  ctx.to_existing_area = 0;
  for (auto* c : to_block->getInsts()) {
    ctx.to_existing_area += cellAreaDbu(c);
  }
  ctx.s_total_area = 0;

  auto rows = from_block->getRows();
  if (rows.begin() != rows.end()) {
    ctx.row_height = (*rows.begin())->getBBox().dy();
    if (ctx.row_height <= 0) {
      ctx.row_height = 1;
    }
  }

  // Priority queue: max-heap by b = -Δp / area, tiebreak smaller id first.
  struct Entry
  {
    double b;
    uint64_t inst_id;
    odb::dbInst* cell;
    bool operator<(const Entry& o) const
    {
      if (b != o.b) {
        return b < o.b;
      }
      return inst_id > o.inst_id;
    }
  };
  std::priority_queue<Entry> pq;
  for (auto* c : from_block->getInsts()) {
    const double dp = surrogateDelta(c, ctx);
    const int64_t area = std::max<int64_t>(cellAreaDbu(c), 1);
    const double b = -dp / static_cast<double>(area);
    pq.push({b, static_cast<uint64_t>(c->getId()), c});
  }

  int popped = 0, applied = 0, skipped_kn = 0;
  while (!pq.empty()) {
    Entry top = pq.top();
    pq.pop();
    ++popped;
    if (ctx.S.count(top.cell)) {
      continue;  // stale (re-pushed with newer priority earlier)
    }
    if (top.b < 0) {
      break;  // paper Algorithm 2 line 3 stop condition
    }
    if (!fitsKnapsack(top.cell, ctx)) {
      ++skipped_kn;
      continue;
    }
    ctx.S.insert(top.cell);
    ctx.s_total_area += cellAreaDbu(top.cell);
    result.push_back(top.cell);
    ++applied;

    // Re-priority of cells incident to top.cell's nets (small nets only).
    std::unordered_set<odb::dbInst*> affected;
    for (auto* iterm : top.cell->getITerms()) {
      odb::dbNet* net = iterm->getNet();
      if (!net) {
        continue;
      }
      if (static_cast<int>(net->getITerms().size())
          > ctx.params.max_net_fanout) {
        continue;  // huge net peers stay at their stale priority
      }
      for (auto* peer_iterm : net->getITerms()) {
        odb::dbInst* peer = peer_iterm->getInst();
        if (peer && peer != top.cell && !ctx.S.count(peer)) {
          affected.insert(peer);
        }
      }
    }
    for (auto* a : affected) {
      const double dp = surrogateDelta(a, ctx);
      const int64_t area = std::max<int64_t>(cellAreaDbu(a), 1);
      const double b = -dp / static_cast<double>(area);
      pq.push({b, static_cast<uint64_t>(a->getId()), a});
    }
  }

  logger_->info(utl::MDM,
                300,
                "GlobalTierOptimizer: popped={}, applied={}, "
                "knapsack_skipped={}, S.size={}.",
                popped,
                applied,
                skipped_kn,
                result.size());
  return result;
}

}  // namespace mdm
