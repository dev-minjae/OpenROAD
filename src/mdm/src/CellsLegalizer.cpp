// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "CellsLegalizer.h"

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

  logger_->info(utl::MDM,
                50,
                "CellsLegalizer: legalized {} ({} insts, {} rows).",
                block->getName(),
                block->getInsts().size(),
                num_rows);
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
