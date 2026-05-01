// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "SemiLegalizer.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

SemiLegalizer::SemiLegalizer(odb::dbDatabase* db, utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

void SemiLegalizer::run(bool use_abacus, const std::string& target_die)
{
  if (use_abacus) {
    runAbacus(target_die, false);
  } else {
    doSimpleLegalize(false);
  }
}

void SemiLegalizer::runAbacus(const std::string& target_die, bool top_hier_die)
{
  odb::dbBlock* top_block = db_->getChip()->getBlock();
  auto children = top_block->getChildren();
  if (children.begin() == children.end()) {
    logger_->warn(utl::MDM,
                  41,
                  "run_semi_legalizer: no child dies; call set_3D_IC first.");
    return;
  }

  if (target_die == "top") {
    runAbacus(*children.begin());
    return;
  }
  if (target_die == "bottom") {
    auto it = children.begin();
    std::advance(it, 1);
    if (it == children.end()) {
      logger_->warn(utl::MDM,
                    42,
                    "run_semi_legalizer: -target_die bottom requested but only "
                    "one child die exists.");
      return;
    }
    runAbacus(*it);
    return;
  }
  if (top_hier_die) {
    runAbacus(top_block);
  }
  for (auto* block : children) {
    runAbacus(block);
  }
}

// Abacus best-row search, ported verbatim from Xueyan Zhao's reviewed
// reference implementation (paste.ubuntu.com/p/dwcvNfHv5M, applied as
// commit 4c941f48c5 in the legacy MDM branch).
//
// Pseudocode (Spindler-Schlichtmann-Johannes 2008, Fig. 3):
//   Sort cells by x;
//   for each cell i:
//     c_best = INF
//     for each row r near i:
//       trial-insert i into r;
//       PlaceRow(r);
//       cost = |Δx| + |Δy|;
//       if cost < c_best: c_best = cost, r_best = r;
//       remove i from r;
//     end
//     insert i into r_best;
//     PlaceRow(r_best);
//   end
//
// Implementation specifics, kept identical to the reference because they
// are what produced the contest-winning QoR:
//   * Search rows in alternating up/down order from the row that contains
//     the cell's current y. The direction comparison uses cell X (looks
//     like an x/y mix-up in the original; preserved verbatim).
//   * Early-terminate either branch once the row-distance lower bound on
//     cost exceeds the current c_best.
//   * Reject trial rows whose width capacity would be exceeded.
//   * Use raw integer division for the starting row index (no clamp): if
//     the cell lies above row 0 the down branch starts negative and the
//     while loop simply skips it.
void SemiLegalizer::runAbacus(odb::dbBlock* block)
{
  target_block_ = block;
  auto rows = target_block_->getRows();
  if (rows.begin() == rows.end()) {
    logger_->warn(utl::MDM,
                  43,
                  "run_semi_legalizer: block {} has no rows; skipping.",
                  block->getName());
    return;
  }
  const int num_rows = static_cast<int>(rows.size());
  const int row_height = (*rows.begin())->getBBox().dy();
  const int y_min = (*rows.begin())->getBBox().yMin();

  auto x_less = [](const odb::dbInst* a, const odb::dbInst* b) {
    return a->getLocation().x() < b->getLocation().x();
  };
  std::multiset<odb::dbInst*, decltype(x_less)> inst_set(
      target_block_->getInsts().begin(),
      target_block_->getInsts().end(),
      x_less);

  std::vector<InstRow> row_set(num_rows);
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
      // Pick the closer row direction. `orig_x` here was a long-standing
      // typo from the contest-winning reference; correct field is
      // `orig_y` (advisor 2026-05-01). Effect was hidden when cells were
      // already nearly row-aligned (Xueyan GP), surfaces on Phase 4.5
      // free-form Nesterov output.
      const bool search_up
          = !can_down
            || (can_up && std::abs(up_y - orig_y) < std::abs(down_y - orig_y));
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
      auto& candidate_row = row_set.at(row_idx);
      candidate_row.push_back(inst);
      if (degreeOfExcess(candidate_row) > 0) {
        candidate_row.pop_back();
        if (search_up) {
          ++next_up;
        } else {
          --next_down;
        }
        continue;
      }
      placeRow(candidate_row);
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
    auto& chosen_row = row_set.at(row_best);
    chosen_row.push_back(inst);
    placeRow(chosen_row);
  }

  logger_->info(utl::MDM,
                40,
                "run_semi_legalizer: legalized {} ({} insts, {} rows).",
                block->getName(),
                target_block_->getInsts().size(),
                row_set.size());
}

void SemiLegalizer::placeRow(const InstRow& row)
{
  std::vector<AbacusCluster> abacus_clusters;
  abacus_clusters.reserve(row.size());
  for (auto* inst : row) {
    const double inst_x = static_cast<double>(inst->getLocation().x());
    if (abacus_clusters.empty()
        || abacus_clusters.back().x + abacus_clusters.back().w <= inst_x) {
      AbacusCluster cluster;
      cluster.e = 0;
      cluster.w = 0;
      cluster.q = 0;
      cluster.x = inst_x;
      addCell(&cluster, inst);
      abacus_clusters.push_back(cluster);
    } else {
      addCell(&abacus_clusters.back(), inst);
      collapse(&abacus_clusters.back(), abacus_clusters);
    }
  }

  // Project cluster positions back onto cell positions, packing left-to-right.
  for (const auto& cluster : abacus_clusters) {
    int x = static_cast<int>(cluster.x);
    for (auto* inst : cluster.insts) {
      inst->setLocation(x, inst->getLocation().y());
      x += static_cast<int>(inst->getMaster()->getWidth());
    }
  }
}

void SemiLegalizer::addCell(AbacusCluster* cluster, odb::dbInst* inst)
{
  const double inst_x = static_cast<double>(inst->getLocation().x());
  const double inst_width = static_cast<double>(inst->getMaster()->getWidth());
  const double inst_e = 1.0;  // no fixed-cell weighting in this port
  cluster->insts.push_back(inst);
  cluster->e += inst_e;
  cluster->q += inst_e * (inst_x - cluster->w);
  cluster->w += inst_width;
}

void SemiLegalizer::addCluster(AbacusCluster* predecessor,
                               AbacusCluster* cluster)
{
  predecessor->insts.insert(
      predecessor->insts.end(), cluster->insts.begin(), cluster->insts.end());
  predecessor->e += cluster->e;
  predecessor->q += cluster->q - cluster->e * predecessor->w;
  predecessor->w += cluster->w;
}

void SemiLegalizer::collapse(AbacusCluster* cluster,
                             std::vector<AbacusCluster>& abacus_clusters)
{
  auto rows = target_block_->getRows();
  const int x_min = (*rows.begin())->getBBox().xMin();
  const int x_max = (*rows.begin())->getBBox().xMax();

  cluster->x = cluster->q / cluster->e;
  if (cluster->x < x_min) {
    cluster->x = x_min;
  } else if (cluster->x > x_max - cluster->w) {
    cluster->x = x_max - cluster->w;
  }

  if (abacus_clusters.size() > 1) {
    auto* predecessor = &*(abacus_clusters.end() - 2);
    if (predecessor->x + predecessor->w > cluster->x) {
      addCluster(predecessor, cluster);
      abacus_clusters.erase(abacus_clusters.end() - 1);
      collapse(predecessor, abacus_clusters);
    }
  }
}

void SemiLegalizer::doSimpleLegalize(bool top_hier_die)
{
  if (top_hier_die) {
    doSimpleLegalize(db_->getChip()->getBlock());
  }
  for (auto* block : db_->getChip()->getBlock()->getChildren()) {
    doSimpleLegalize(block);
  }
}

void SemiLegalizer::doSimpleLegalize(odb::dbBlock* block)
{
  target_block_ = block;
  if (!utilCheck()) {
    logger_->warn(
        utl::MDM,
        44,
        "run_semi_legalizer: block {} over-utilised; skipping shift legalize.",
        block->getName());
    return;
  }
  adjustRowCapacity();
  shiftLegalize();
}

void SemiLegalizer::shiftLegalize()
{
  std::vector<InstRow> row_clusters;
  auto rows = target_block_->getRows();
  const int num_rows = static_cast<int>(rows.size());
  const int row_height = (*rows.begin())->getBBox().dy();
  const int y_min = (*rows.begin())->getBBox().yMin();

  row_clusters.resize(num_rows);
  for (auto* inst : target_block_->getInsts()) {
    const int inst_y = inst->getLocation().y();
    int row_idx = (inst_y - y_min) / row_height;
    if (row_idx < 0) {
      row_idx = 0;
    }
    if (row_idx >= num_rows) {
      row_idx = num_rows - 1;
    }
    inst->setLocation(inst->getLocation().x(), row_idx * row_height + y_min);
    row_clusters[row_idx].push_back(inst);
  }

  for (auto& cluster : row_clusters) {
    std::sort(cluster.begin(),
              cluster.end(),
              [](const odb::dbInst* a, const odb::dbInst* b) {
                return a->getLocation().x() < b->getLocation().x();
              });
    int cursor = target_block_->getDieArea().xMin();
    int cell_row_idx = 0;
    for (auto* inst : cluster) {
      if (cursor > inst->getLocation().x()) {
        inst->setLocation(cursor, inst->getLocation().y());
      }
      cursor = inst->getBBox()->xMax();
      if (cursor > target_block_->getDieArea().xMax()) {
        shiftCellsToLeft(cluster, inst, cell_row_idx);
      }
      cell_row_idx++;
    }
  }
}

void SemiLegalizer::shiftCellsToLeft(InstRow& cell_row,
                                     odb::dbInst* inst,
                                     int idx)
{
  std::vector<odb::dbInst*> candidates;
  const int stick_out_width
      = inst->getBBox()->xMax() - target_block_->getDieArea().xMax();
  int sum_of_space = 0;
  int cursor = inst->getLocation().x();
  candidates.push_back(inst);
  while (sum_of_space < stick_out_width) {
    idx -= 1;
    int spacing;
    if (idx < 0) {
      spacing = cursor;
      candidates.push_back(nullptr);
    } else {
      odb::dbInst* prev = cell_row.at(idx);
      spacing = cursor - prev->getBBox()->xMax();
      candidates.push_back(prev);
      cursor = prev->getLocation().x();
    }
    sum_of_space += spacing;
  }
  if (!candidates.empty() && candidates.back() == nullptr) {
    candidates.pop_back();
  }

  cursor = target_block_->getDieArea().xMax();
  for (auto* cand : candidates) {
    const int cell_width = cand->getMaster()->getWidth();
    const int place_point = cursor - cell_width;
    cand->setLocation(place_point, cand->getLocation().y());
    cursor = cand->getLocation().x();
  }
}

void SemiLegalizer::adjustRowCapacity()
{
  std::vector<InstRow> row_clusters;
  auto rows = target_block_->getRows();
  const int num_rows = static_cast<int>(rows.size());
  const int row_height = (*rows.begin())->getBBox().dy();
  const int y_min = (*rows.begin())->getBBox().yMin();

  row_clusters.resize(num_rows);
  for (auto* inst : target_block_->getInsts()) {
    const int inst_y = inst->getLocation().y();
    int row_idx = (inst_y - y_min) / row_height;
    if (row_idx < 0) {
      row_idx = 0;
    }
    if (row_idx >= num_rows) {
      row_idx = num_rows - 1;
    }
    inst->setLocation(inst->getLocation().x(), row_idx * row_height + y_min);
    row_clusters[row_idx].push_back(inst);
  }
  for (auto& cluster : row_clusters) {
    std::sort(cluster.begin(),
              cluster.end(),
              [](const odb::dbInst* a, const odb::dbInst* b) {
                return a->getLocation().x() < b->getLocation().x();
              });
  }

  enum Direction
  {
    kUpward,
    kDownward
  };
  Direction direction = kUpward;
  bool solved = false;
  while (!solved) {
    solved = true;
    const int cluster_max_idx = static_cast<int>(row_clusters.size());
    const int start_idx = direction == kUpward ? 0 : cluster_max_idx - 1;
    const int end_idx = direction == kUpward ? cluster_max_idx - 1 : 0;
    const int step = direction == kUpward ? 1 : -1;

    for (int i = start_idx;
         direction == kUpward ? (i < end_idx) : (i > end_idx);
         i += step) {
      auto& cluster = row_clusters.at(i);
      int excess = degreeOfExcess(cluster);
      if (excess > 0) {
        solved = false;
      }
      int reduced_width = 0;
      while (excess > reduced_width && !cluster.empty()) {
        odb::dbInst* smallest = cluster.front();
        cluster.erase(cluster.begin());
        const int target_row_idx = direction == kUpward ? i + 1 : i - 1;
        if (target_row_idx < 0 || target_row_idx >= cluster_max_idx) {
          break;
        }
        row_clusters.at(target_row_idx).push_back(smallest);
        smallest->setLocation(smallest->getLocation().x(),
                              target_row_idx * row_height + y_min);
        reduced_width += smallest->getMaster()->getWidth();
      }
    }
    direction = direction == kUpward ? kDownward : kUpward;
  }
}

int SemiLegalizer::degreeOfExcess(const InstRow& row) const
{
  const int row_width = (*target_block_->getRows().begin())->getBBox().dx();
  int total_width = 0;
  for (auto* inst : row) {
    total_width += inst->getMaster()->getWidth();
  }
  return total_width > row_width ? total_width - row_width : 0;
}

bool SemiLegalizer::utilCheck() const
{
  int64_t sum_area = 0;
  for (auto* inst : target_block_->getInsts()) {
    sum_area += static_cast<int64_t>(inst->getMaster()->getWidth())
                * static_cast<int64_t>(inst->getMaster()->getHeight());
  }
  return sum_area <= target_block_->getDieArea().area();
}

}  // namespace mdm
