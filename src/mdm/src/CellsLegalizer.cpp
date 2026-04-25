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

// Best-row driver — mirrors SemiLegalizer::runAbacus(block) so that Stage 3.1
// reproduces SemiLegalizer's behaviour exactly. Stage 3.2 will lift the
// "left-x asc" insertion order assumption inside placeRow.
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

  std::vector<std::vector<odb::dbInst*>> row_set(num_rows);
  for (auto* inst : inst_set) {
    int cost_best = std::numeric_limits<int>::max();
    int row_best = 0;
    const int orig_x = inst->getLocation().x();
    const int orig_y = inst->getLocation().y();
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

      inst->setLocation(orig_x, row_y);
      auto& candidate_row = row_set[row_idx];
      candidate_row.push_back(inst);

      int total_width = 0;
      for (auto* c : candidate_row) {
        total_width += instWidth(c);
      }
      if (total_width > row_width) {
        candidate_row.pop_back();
        if (search_up) {
          ++next_up;
        } else {
          --next_down;
        }
        continue;
      }

      placeRow(candidate_row, row_xmin, row_xmax);
      const int cost = std::abs(inst->getLocation().x() - orig_x)
                       + std::abs(inst->getLocation().y() - orig_y);
      if (cost < cost_best) {
        cost_best = cost;
        row_best = row_idx;
      }
      candidate_row.pop_back();
      if (search_up) {
        ++next_up;
      } else {
        --next_down;
      }
    }

    inst->setLocation(orig_x, y_min + row_height * row_best);
    auto& chosen_row = row_set[row_best];
    chosen_row.push_back(inst);
    placeRow(chosen_row, row_xmin, row_xmax);
  }

  logger_->info(utl::MDM,
                50,
                "CellsLegalizer: legalized {} ({} insts, {} rows).",
                block->getName(),
                block->getInsts().size(),
                num_rows);
}

void CellsLegalizer::placeRow(const std::vector<odb::dbInst*>& cells,
                              int row_xmin,
                              int row_xmax)
{
  Row row;
  for (odb::dbInst* inst : cells) {
    insertCell(row, inst, row_xmin, row_xmax);
  }
  commitPlacement(row);
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

void CellsLegalizer::insertCell(Row& row,
                                odb::dbInst* inst,
                                int row_xmin,
                                int row_xmax)
{
  const double inst_x = static_cast<double>(inst->getLocation().x());
  const double inst_w = static_cast<double>(instWidth(inst));
  const double inst_e = 1.0;  // no fixed-cell weighting in this port

  // Stage 3.2 contract: preserve SemiLegalizer's behaviour for left-x
  // ascending input. The cluster container is a partial-ordered map (so
  // future stages can run a true mid-row insert via cascadeMerge), but
  // the driver still hands cells in left-x asc order, so for the typical
  // tail case we MUST behave exactly like SemiLegalizer:
  //   * if the last cluster's right edge is to the left of inst_x, open a
  //     new cluster at the tail
  //   * otherwise, append inst to the last cluster and let cascadeMerge
  //     pull predecessors in if recompute shifts xc enough
  // Anything else changes the cells-in-cluster ordering relative to
  // SemiLegalizer and degrades HPWL on cells whose current x has been
  // shuffled by prior placeRow calls.
  if (!row.empty()) {
    auto last_it = std::prev(row.end());
    Cluster& last = last_it->second;
    if (last.xc + last.w > inst_x) {
      last.cells.push_back(inst);
      last.q += inst_e * (inst_x - last.w);
      last.e += inst_e;
      last.w += inst_w;
      cascadeMerge(row, last_it, row_xmin, row_xmax);
      return;
    }
  }

  // No overlap with the existing tail (or row empty) — open a new cluster.
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
  cascadeMerge(row, it, row_xmin, row_xmax);
}

void CellsLegalizer::cascadeMerge(Row& row,
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
