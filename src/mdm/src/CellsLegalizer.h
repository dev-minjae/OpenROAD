// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <map>
#include <string>
#include <vector>

namespace odb {
class dbBlock;
class dbDatabase;
class dbInst;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// Dynamic row-based legalizer (iPL-3D paper §IV.C.1, Fig. 5). The cluster
// container is a partial-ordered map keyed by the leftmost cell's centre x,
// which lets us insert cells in the middle of a row and merge with neighbours
// in either direction.
//
// Stage 3.3 lifts the legacy "rebuild-row-per-trial" driver: each row keeps
// a persistent cluster map, trials snapshot the affected row (std::map copy)
// and restore on revert, and `insertCell` returns the iterator to inst's
// containing cluster so the driver can predict inst's x without an extra
// scan. Cells still arrive in left-x ascending order in this stage, so the
// behaviour is mathematically equivalent to SemiLegalizer's Abacus path
// (q/e/w invariants are preserved across re-clustering). Subsequent stages
// will exploit the partial-ordered map by feeding cells in non-left-x order
// or by removing the tail-fast-path entirely.
class CellsLegalizer
{
 public:
  CellsLegalizer(odb::dbDatabase* db, utl::Logger* logger);

  // Legalize every (or one) child die under the top hier block.
  // target_die: "" (all), "top", "bottom".
  void run(const std::string& target_die);

 private:
  struct Cluster
  {
    double xc = 0.0;  // weighted centre x (q/e), clamped to row bounds
    double w = 0.0;   // cumulative cluster width
    double q = 0.0;   // Σ e_i * (x_i − Σ_{j<i} w_j)  (Abacus invariant)
    double e
        = 0.0;  // Σ e_i (uniform 1.0 in this port — no fixed-cell weighting)
    std::vector<odb::dbInst*> cells;
  };
  using Row
      = std::map<int, Cluster>;  // key = leftmost cell's centre x at insert

  void legalizeBlock(odb::dbBlock* block);

  // Stage 3.3 contract: insert `inst` into the partial-ordered row map,
  // run cascadeMerge to absorb overlapping neighbours, and return the
  // iterator to the cluster that now contains `inst`. The tail-fast-path
  // (append to last cluster when the new cell sits inside its right edge)
  // is preserved so left-x ascending input behaves exactly like
  // SemiLegalizer's Abacus path; future stages will remove that path to
  // unlock true mid-row insert.
  Row::iterator insertCell(Row& row,
                           odb::dbInst* inst,
                           int row_xmin,
                           int row_xmax);

  // Fixed-point cascade: from `it`, alternately merge an overlapping right
  // neighbour into `it` or merge `it` into an overlapping left neighbour
  // (substituting `it` with the predecessor in that case). Iterates until
  // both sides are clear. Erasing one cluster per iteration guarantees
  // termination. Returns the (possibly substituted) iterator pointing to
  // the cluster that holds the cells originally inserted by the caller.
  Row::iterator cascadeMerge(Row& row,
                             Row::iterator it,
                             int row_xmin,
                             int row_xmax);

  // Predict inst's x in `row` after a successful insertCell, without
  // mutating any cell positions. Mirrors commitPlacement's left-to-right
  // packing inside `inst`'s cluster. `inst_cluster` must be the iterator
  // returned by insertCell.
  int predictX(Row::iterator inst_cluster, odb::dbInst* inst) const;

  // Recompute xc = q/e and clamp to [row_xmin, row_xmax - w].
  void recomputeCenter(Cluster& cluster, int row_xmin, int row_xmax) const;

  // Walk the row in key order, packing cells left-to-right inside their
  // cluster. Y is preserved (set when the caller committed each cell).
  void commitPlacement(Row& row);

  static int instWidth(odb::dbInst* inst);

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  odb::dbBlock* target_block_ = nullptr;
};

}  // namespace mdm
