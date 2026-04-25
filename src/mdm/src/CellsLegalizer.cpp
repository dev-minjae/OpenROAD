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
    collapse(row, row_xmin, row_xmax);
  }
  commitPlacement(row);
}

void CellsLegalizer::insertCell(Row& row,
                                odb::dbInst* inst,
                                int /*row_xmin*/,
                                int /*row_xmax*/)
{
  const double inst_x = static_cast<double>(inst->getLocation().x());
  const double inst_w = static_cast<double>(instWidth(inst));
  const double inst_e = 1.0;  // no fixed-cell weighting in this port

  const auto add_to_cluster = [&](Cluster& c) {
    c.cells.push_back(inst);
    c.q += inst_e * (inst_x - c.w);
    c.e += inst_e;
    c.w += inst_w;
  };

  // Stage 3.1: cells arrive left-to-right; we only ever append to the tail.
  if (row.empty()) {
    Cluster c;
    c.xc = inst_x;
    c.q = inst_e * inst_x;
    c.e = inst_e;
    c.w = inst_w;
    c.cells.push_back(inst);
    const int key = static_cast<int>(inst_x + inst_w / 2.0);
    row.emplace(key, std::move(c));
    return;
  }

  Cluster& last = std::prev(row.end())->second;
  if (last.xc + last.w <= inst_x) {
    Cluster c;
    c.xc = inst_x;
    c.q = inst_e * inst_x;
    c.e = inst_e;
    c.w = inst_w;
    c.cells.push_back(inst);
    int key = static_cast<int>(inst_x + inst_w / 2.0);
    while (row.find(key) != row.end()) {
      ++key;  // disambiguate degenerate equal keys
    }
    row.emplace(key, std::move(c));
    return;
  }

  add_to_cluster(last);
}

void CellsLegalizer::collapse(Row& row, int row_xmin, int row_xmax)
{
  if (row.empty()) {
    return;
  }
  auto last_it = std::prev(row.end());
  Cluster& last = last_it->second;
  last.xc = last.q / last.e;
  if (last.xc < row_xmin) {
    last.xc = row_xmin;
  }
  if (last.xc + last.w > row_xmax) {
    last.xc = row_xmax - last.w;
  }

  if (row.size() < 2) {
    return;
  }
  auto prev_it = std::prev(last_it);
  Cluster& prev = prev_it->second;
  if (prev.xc + prev.w <= last.xc) {
    return;  // no overlap
  }

  // Merge `last` into `prev`. Abacus invariant:
  //   q_new = q_a + q_b - e_b * w_a
  //   e_new = e_a + e_b
  //   w_new = w_a + w_b
  prev.cells.insert(prev.cells.end(), last.cells.begin(), last.cells.end());
  prev.q += last.q - last.e * prev.w;
  prev.e += last.e;
  prev.w += last.w;
  row.erase(last_it);
  collapse(row, row_xmin, row_xmax);
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
