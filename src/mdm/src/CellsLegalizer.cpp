// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "CellsLegalizer.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <set>
#include <utility>
#include <vector>

#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

CellsLegalizer::CellsLegalizer(odb::dbDatabase* db, utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

int CellsLegalizer::instWidth(odb::dbInst* inst)
{
  return static_cast<int>(inst->getMaster()->getWidth());
}

void CellsLegalizer::run(const std::string& target_die)
{
  odb::dbBlock* top = db_->getChip()->getBlock();
  auto children = top->getChildren();
  if (children.begin() == children.end()) {
    logger_->warn(
        utl::MDM, 51, "CellsLegalizer: no child dies; call set_3D_IC first.");
    return;
  }
  if (target_die == "top") {
    legalizeBlock(*children.begin());
    return;
  }
  if (target_die == "bottom") {
    auto it = children.begin();
    std::advance(it, 1);
    if (it == children.end()) {
      logger_->warn(utl::MDM,
                    52,
                    "CellsLegalizer: -target_die bottom requested but only "
                    "one child die exists.");
      return;
    }
    legalizeBlock(*it);
    return;
  }
  for (auto* child : children) {
    legalizeBlock(child);
  }
}

// Best-row driver — Stage 3.3 swap: rows hold persistent cluster maps
// instead of cell vectors that get re-clustered from scratch on every
// trial. Each trial copies the candidate row's std::map (snapshot),
// calls insertCell (which returns an iterator to inst's cluster), reads
// inst's predicted x via predictX, and restores the row by moving the
// snapshot back. Width capacity is tracked with a per-row counter so
// the trial does not have to walk every cluster to compute it.
//
// Equivalence note (left-x asc input + tail-fast-path): q/e/w invariants
// for a given set of cells are independent of insertion order, and
// addCluster (left-merge) is mathematically the same operation as the
// "rebuild placeRow" path in SemiLegalizer when those cells are processed
// in the same left-x order. So bench remains regression-free until a
// future stage feeds cells in non-left-x order or removes the tail-fast-
// path entirely.
void CellsLegalizer::legalizeBlock(odb::dbBlock* block)
{
  target_block_ = block;
  auto rows = block->getRows();
  if (rows.begin() == rows.end()) {
    logger_->warn(utl::MDM,
                  53,
                  "CellsLegalizer: block {} has no rows; skipping.",
                  block->getName());
    return;
  }
  const int num_rows = static_cast<int>(rows.size());
  const int row_height = (*rows.begin())->getBBox().dy();
  const int y_min = (*rows.begin())->getBBox().yMin();
  const int row_xmin = (*rows.begin())->getBBox().xMin();
  const int row_xmax = (*rows.begin())->getBBox().xMax();
  const int row_width = row_xmax - row_xmin;

  auto x_less = [](const odb::dbInst* a, const odb::dbInst* b) {
    return a->getLocation().x() < b->getLocation().x();
  };
  std::multiset<odb::dbInst*, decltype(x_less)> inst_set(
      block->getInsts().begin(), block->getInsts().end(), x_less);

  std::vector<Row> row_set(num_rows);
  std::vector<int64_t> row_total_w(num_rows, 0);

  for (auto* inst : inst_set) {
    int cost_best = std::numeric_limits<int>::max();
    int row_best = 0;
    const int orig_x = inst->getLocation().x();
    const int orig_y = inst->getLocation().y();
    const int inst_w = instWidth(inst);
    int next_down = (orig_y - y_min) / row_height;
    int next_up = next_down + 1;

    while (next_up < num_rows || next_down >= 0) {
      const bool can_up = (next_up < num_rows);
      const bool can_down = (next_down >= 0);
      const int up_y = y_min + row_height * next_up;
      const int down_y = y_min + row_height * next_down;
      // Direction comparison preserved verbatim from SemiLegalizer; the
      // up_y/orig_x mix-up matches the contest-winning reference.
      const bool search_up
          = !can_down
            || (can_up && std::abs(up_y - orig_x) < std::abs(down_y - orig_x));
      const int row_idx = search_up ? next_up : next_down;
      const int row_y = search_up ? up_y : down_y;

      if (std::abs(row_y - orig_y) > cost_best) {
        if (search_up) {
          next_up = num_rows;
        } else {
          next_down = -1;
        }
        continue;
      }

      if (row_total_w[row_idx] + inst_w > row_width) {
        if (search_up) {
          ++next_up;
        } else {
          --next_down;
        }
        continue;
      }

      inst->setLocation(orig_x, row_y);
      Row& candidate_row = row_set[row_idx];
      Row snapshot = candidate_row;
      auto inst_cluster = insertCell(candidate_row, inst, row_xmin, row_xmax);
      const int trial_x = predictX(inst_cluster, inst);
      const int cost = std::abs(trial_x - orig_x) + std::abs(row_y - orig_y);
      if (cost < cost_best) {
        cost_best = cost;
        row_best = row_idx;
      }
      candidate_row = std::move(snapshot);

      if (search_up) {
        ++next_up;
      } else {
        --next_down;
      }
    }

    inst->setLocation(orig_x, y_min + row_height * row_best);
    insertCell(row_set[row_best], inst, row_xmin, row_xmax);
    row_total_w[row_best] += inst_w;
  }

  for (auto& row : row_set) {
    commitPlacement(row);
  }

  pairSwap(block);

  logger_->info(utl::MDM,
                50,
                "CellsLegalizer: legalized {} ({} insts, {} rows).",
                block->getName(),
                block->getInsts().size(),
                num_rows);
}

int64_t CellsLegalizer::pairNetsHPWL(odb::dbInst* a, odb::dbInst* b) const
{
  return groupNetsHPWL({a, b});
}

int64_t CellsLegalizer::groupNetsHPWL(
    std::initializer_list<odb::dbInst*> insts) const
{
  std::set<odb::dbNet*> nets;
  for (odb::dbInst* inst : insts) {
    for (auto* iterm : inst->getITerms()) {
      if (auto* net = iterm->getNet()) {
        nets.insert(net);
      }
    }
  }
  int64_t total = 0;
  for (auto* net : nets) {
    odb::Rect box1 = net->getTermBBox();
    auto sibling_it = sibling_bbox_cache_.find(net);
    if (sibling_it == sibling_bbox_cache_.end()) {
      total += box1.dx() + box1.dy();
      continue;
    }
    // Intersected net — apply the TOR-adjusted 3D HPWL formula from
    // MultiDieManager::get3DHPWL so the swap accept condition tracks
    // the true 3D HPWL change rather than just the current die's
    // 2D bbox change.
    odb::Rect box2 = sibling_it->second;
    odb::Rect box3;
    if (box1.intersects(box2)) {
      box3 = box1.intersect(box2);
    } else {
      std::vector<int> xs{box1.xMin(), box1.xMax(), box2.xMin(), box2.xMax()};
      std::vector<int> ys{box1.yMin(), box1.yMax(), box2.yMin(), box2.yMax()};
      std::sort(xs.begin(), xs.end());
      std::sort(ys.begin(), ys.end());
      box3.init(xs[1], ys[1], xs[2], ys[2]);
    }
    odb::Rect centre{
        box3.xCenter(), box3.yCenter(), box3.xCenter(), box3.yCenter()};
    box1.merge(centre);
    box2.merge(centre);
    total += box1.dx() + box1.dy();
    total += box2.dx() + box2.dy();
  }
  return total;
}

odb::dbNet* CellsLegalizer::findSiblingNet(odb::dbNet* net)
{
  for (auto* bterm : net->getBTerms()) {
    odb::dbITerm* upper = bterm->getITerm();
    if (!upper || !upper->getNet()) {
      continue;
    }
    for (auto* iterm : upper->getNet()->getITerms()) {
      odb::dbBTerm* b = iterm->getBTerm();
      if (b && b->getBlock() != net->getBlock()) {
        return b->getNet();
      }
    }
  }
  return nullptr;
}

void CellsLegalizer::buildSiblingCache(odb::dbBlock* block)
{
  sibling_bbox_cache_.clear();
  for (auto* net : block->getNets()) {
    if (!odb::dbBoolProperty::find(net, "intersected")) {
      continue;
    }
    odb::dbNet* sibling = findSiblingNet(net);
    if (sibling) {
      sibling_bbox_cache_[net] = sibling->getTermBBox();
    }
  }
}

void CellsLegalizer::pairSwap(odb::dbBlock* block)
{
  auto rows = block->getRows();
  if (rows.begin() == rows.end()) {
    return;
  }
  const int row_height = (*rows.begin())->getBBox().dy();
  const int y_min = (*rows.begin())->getBBox().yMin();
  const int num_rows = static_cast<int>(rows.size());

  // Cache sibling-die bbox for every intersected net in this block.
  // Sibling cells do not move during this pairSwap call (we only edit
  // `block`'s instances), so the cache stays valid for the duration.
  buildSiblingCache(block);

  // Group cells by their committed row.
  std::vector<std::vector<odb::dbInst*>> row_cells(num_rows);
  for (auto* inst : block->getInsts()) {
    const int row_idx = (inst->getLocation().y() - y_min) / row_height;
    if (row_idx >= 0 && row_idx < num_rows) {
      row_cells[row_idx].push_back(inst);
    }
  }
  for (auto& cells : row_cells) {
    std::sort(cells.begin(),
              cells.end(),
              [](const odb::dbInst* x, const odb::dbInst* y) {
                return x->getLocation().x() < y->getLocation().x();
              });
  }

  // Each pass tries swaps at distances 1..kMaxDist. Distance-1 always
  // legalises by re-packing the pair left-justified into the same
  // combined slot. Distance-d swaps with d>1 require equal widths so
  // the cells in between can stay in place. The 3D-aware cost in
  // pairNetsHPWL filters out swaps that would shrink one die's bbox at
  // the cost of growing the TOR-adjusted contribution; without that we
  // saw 2D-only dist>1 swaps regress on case3 (+0.30%).
  constexpr int kMaxPasses = 8;
  constexpr int kMaxDist = 4;
  int total_swaps = 0;
  for (int pass = 0; pass < kMaxPasses; ++pass) {
    int pass_swaps = 0;
    for (auto& cells : row_cells) {
      for (size_t i = 0; i + 1 < cells.size(); ++i) {
        odb::dbInst* a = cells[i];
        odb::dbInst* b = cells[i + 1];
        const int xa = a->getLocation().x();
        const int xb = b->getLocation().x();
        const int wb = instWidth(b);
        // Distance-1 swap: re-pack pair left-justified. Always legal
        // because xa + wa + wb <= xb + wb (xa + wa <= xb pre-swap).
        const int64_t hpwl_before = pairNetsHPWL(a, b);
        a->setLocation(xa + wb, a->getLocation().y());
        b->setLocation(xa, b->getLocation().y());
        const int64_t hpwl_after = pairNetsHPWL(a, b);
        if (hpwl_after < hpwl_before) {
          std::swap(cells[i], cells[i + 1]);
          ++pass_swaps;
        } else {
          a->setLocation(xa, a->getLocation().y());
          b->setLocation(xb, b->getLocation().y());
        }
      }
      for (int dist = 2; dist <= kMaxDist; ++dist) {
        for (size_t i = 0; i + dist < cells.size(); ++i) {
          odb::dbInst* a = cells[i];
          odb::dbInst* b = cells[i + dist];
          if (instWidth(a) != instWidth(b)) {
            continue;  // distance > 1 needs equal widths to stay legal
          }
          const int xa = a->getLocation().x();
          const int xb = b->getLocation().x();
          const int64_t hpwl_before = pairNetsHPWL(a, b);
          a->setLocation(xb, a->getLocation().y());
          b->setLocation(xa, b->getLocation().y());
          const int64_t hpwl_after = pairNetsHPWL(a, b);
          if (hpwl_after < hpwl_before) {
            std::swap(cells[i], cells[i + dist]);
            ++pass_swaps;
          } else {
            a->setLocation(xa, a->getLocation().y());
            b->setLocation(xb, b->getLocation().y());
          }
        }
      }
      // Triple-cell rotation: for adjacent (i, i+1, i+2), score the
      // identity + reverse + rotate-left + rotate-right permutations
      // and apply the cheapest. Each permutation re-packs left-justified
      // into [xa, xa + wa + wb + wc], which never extends past the
      // original right edge (sum of widths is unchanged). This unlocks
      // moves the dist-1 / dist-2 swap pass cannot express, e.g. when
      // the middle cell wants to swap with both neighbours
      // simultaneously.
      for (size_t i = 0; i + 2 < cells.size(); ++i) {
        odb::dbInst* a = cells[i];
        odb::dbInst* b = cells[i + 1];
        odb::dbInst* c = cells[i + 2];
        const int xa = a->getLocation().x();
        const int xb = b->getLocation().x();
        const int xc = c->getLocation().x();
        const int wa = instWidth(a);
        const int wb = instWidth(b);
        const int wc = instWidth(c);
        const int y_a = a->getLocation().y();
        const int y_b = b->getLocation().y();
        const int y_c = c->getLocation().y();
        const int64_t hpwl_id = groupNetsHPWL({a, b, c});

        int64_t best_hpwl = hpwl_id;
        int best_perm = 0;  // 0=id, 1=reverse, 2=rot-L, 3=rot-R

        // Reverse: C, B, A.
        c->setLocation(xa, y_c);
        b->setLocation(xa + wc, y_b);
        a->setLocation(xa + wc + wb, y_a);
        const int64_t hpwl_rev = groupNetsHPWL({a, b, c});
        if (hpwl_rev < best_hpwl) {
          best_hpwl = hpwl_rev;
          best_perm = 1;
        }
        // Rotate-left: B, C, A.
        b->setLocation(xa, y_b);
        c->setLocation(xa + wb, y_c);
        a->setLocation(xa + wb + wc, y_a);
        const int64_t hpwl_rotl = groupNetsHPWL({a, b, c});
        if (hpwl_rotl < best_hpwl) {
          best_hpwl = hpwl_rotl;
          best_perm = 2;
        }
        // Rotate-right: C, A, B.
        c->setLocation(xa, y_c);
        a->setLocation(xa + wc, y_a);
        b->setLocation(xa + wc + wa, y_b);
        const int64_t hpwl_rotr = groupNetsHPWL({a, b, c});
        if (hpwl_rotr < best_hpwl) {
          best_hpwl = hpwl_rotr;
          best_perm = 3;
        }

        // Apply the chosen permutation. Note: cells currently sit in
        // rot-right configuration from the last test; rewrite for the
        // chosen permutation explicitly so each branch is self-contained.
        switch (best_perm) {
          case 0:
            a->setLocation(xa, y_a);
            b->setLocation(xb, y_b);
            c->setLocation(xc, y_c);
            break;
          case 1:
            c->setLocation(xa, y_c);
            b->setLocation(xa + wc, y_b);
            a->setLocation(xa + wc + wb, y_a);
            std::swap(cells[i], cells[i + 2]);
            ++pass_swaps;
            break;
          case 2:
            b->setLocation(xa, y_b);
            c->setLocation(xa + wb, y_c);
            a->setLocation(xa + wb + wc, y_a);
            {
              odb::dbInst* tmp = cells[i];
              cells[i] = cells[i + 1];
              cells[i + 1] = cells[i + 2];
              cells[i + 2] = tmp;
            }
            ++pass_swaps;
            break;
          case 3:
            // Already in rot-right config; just commit cells[] order.
            {
              odb::dbInst* tmp = cells[i + 2];
              cells[i + 2] = cells[i + 1];
              cells[i + 1] = cells[i];
              cells[i] = tmp;
            }
            ++pass_swaps;
            break;
        }
      }
    }
    total_swaps += pass_swaps;
    if (pass_swaps == 0) {
      break;
    }
  }
  if (total_swaps > 0) {
    logger_->info(utl::MDM,
                  54,
                  "CellsLegalizer: pairSwap accepted {} swaps in {}.",
                  total_swaps,
                  block->getName());
  }
}

int CellsLegalizer::predictX(Row::iterator inst_cluster,
                             odb::dbInst* inst) const
{
  int x = static_cast<int>(inst_cluster->second.xc);
  for (odb::dbInst* c : inst_cluster->second.cells) {
    if (c == inst) {
      return x;
    }
    x += instWidth(c);
  }
  // Should never happen: caller guarantees inst was just inserted into
  // this cluster.
  return x;
}

void CellsLegalizer::recomputeCenter(Cluster& cluster,
                                     int row_xmin,
                                     int row_xmax) const
{
  cluster.xc = cluster.q / cluster.e;
  if (cluster.xc < row_xmin) {
    cluster.xc = row_xmin;
  }
  if (cluster.xc + cluster.w > row_xmax) {
    cluster.xc = row_xmax - cluster.w;
  }
}

CellsLegalizer::Row::iterator CellsLegalizer::insertCell(Row& row,
                                                         odb::dbInst* inst,
                                                         int row_xmin,
                                                         int row_xmax)
{
  const double inst_x = static_cast<double>(inst->getLocation().x());
  const double inst_w = static_cast<double>(instWidth(inst));
  const double inst_e = 1.0;  // no fixed-cell weighting in this port

  // Always open a singleton cluster at inst's centre-x, then cascadeMerge
  // resolves overlaps in either direction. For left-x ascending input the
  // result is mathematically identical to a tail-append + collapse path:
  // recomputeCenter clamps xc to inst_x (within row bounds), so the only
  // overlap that fires is a left-merge into the existing tail, and the
  // q/e/w arithmetic of `prev.q += cur.q - cur.e * prev.w` reduces to
  // `prev.q += inst_x - prev.w` — exactly addCell's update. The cluster
  // identity (key) survives because cascadeMerge erases the singleton
  // when left-merging into the existing tail. For non-left-x input
  // (Stage 3.3d and beyond), this same path inserts in the middle of
  // the map and cascadeMerge resolves overlaps both ways.
  Cluster c;
  c.xc = inst_x;
  c.q = inst_e * inst_x;
  c.e = inst_e;
  c.w = inst_w;
  c.cells.push_back(inst);
  int key = static_cast<int>(inst_x + inst_w / 2.0);
  while (row.find(key) != row.end()) {
    ++key;
  }
  auto [it, inserted] = row.emplace(key, std::move(c));
  return cascadeMerge(row, it, row_xmin, row_xmax);
}

CellsLegalizer::Row::iterator CellsLegalizer::cascadeMerge(Row& row,
                                                           Row::iterator it,
                                                           int row_xmin,
                                                           int row_xmax)
{
  // Fixed-point cascade: at every step recompute it's centre, then attempt
  // a right merge or a left merge — whichever finds an overlap first.
  // After a merge re-enter the loop because growing `it` (or substituting
  // `it = prev_it`) can expose a freshly-overlapping neighbour on either
  // side. Terminating: every iteration that takes a merge erases one
  // node, and the row size is finite.
  while (true) {
    recomputeCenter(it->second, row_xmin, row_xmax);

    // Right merge: `next` overlaps and must fold into `it`.
    auto next_it = std::next(it);
    if (next_it != row.end()
        && it->second.xc + it->second.w > next_it->second.xc) {
      Cluster& cur = it->second;
      Cluster& next = next_it->second;
      cur.cells.insert(cur.cells.end(), next.cells.begin(), next.cells.end());
      cur.q += next.q - next.e * cur.w;
      cur.e += next.e;
      cur.w += next.w;
      row.erase(next_it);
      continue;
    }

    // Left merge: `prev` overlaps so `it` folds into `prev`.
    if (it != row.begin()) {
      auto prev_it = std::prev(it);
      if (prev_it->second.xc + prev_it->second.w > it->second.xc) {
        Cluster& prev = prev_it->second;
        Cluster& cur = it->second;
        prev.cells.insert(prev.cells.end(), cur.cells.begin(), cur.cells.end());
        prev.q += cur.q - cur.e * prev.w;
        prev.e += cur.e;
        prev.w += cur.w;
        row.erase(it);
        it = prev_it;
        continue;
      }
    }

    break;  // both neighbours are clear of `it`
  }
  return it;
}

void CellsLegalizer::commitPlacement(Row& row)
{
  for (auto& [key, cluster] : row) {
    int x = static_cast<int>(cluster.xc);
    for (odb::dbInst* inst : cluster.cells) {
      inst->setLocation(x, inst->getLocation().y());
      x += instWidth(inst);
    }
  }
}

}  // namespace mdm
