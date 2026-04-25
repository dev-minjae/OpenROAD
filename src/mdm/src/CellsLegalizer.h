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
// which lets future stages (3.2+) insert cells in the middle of a row and
// merge with neighbours in either direction.
//
// Stage 3.1 only seeds the data structure: cells still arrive in left-x
// ascending order, every insert lands at the row tail, and `collapse` walks
// backward exactly like SemiLegalizer. The output is therefore identical to
// SemiLegalizer's Abacus path; this commit is regression-free by design and
// exists to land the new container before Stage 3.2 plugs in mid-row insert.
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

  // Fully rebuild a row from its cell list and write back x positions.
  // Stage 3.1 contract: `cells` is in left-x ascending order.
  void placeRow(const std::vector<odb::dbInst*>& cells,
                int row_xmin,
                int row_xmax);

  // Stage 3.1: append-only insert. Stage 3.2 will replace this with a true
  // mid-row insert that does cascade-merge in both directions.
  void insertCell(Row& row, odb::dbInst* inst, int row_xmin, int row_xmax);

  // Stage 3.1: backward collapse — only the last cluster can move because
  // every insert lands at the tail.
  void collapse(Row& row, int row_xmin, int row_xmax);

  // Walk the row in key order, packing cells left-to-right inside their
  // cluster. Y is preserved (set by the caller before placeRow).
  void commitPlacement(Row& row);

  static int instWidth(odb::dbInst* inst);

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  odb::dbBlock* target_block_ = nullptr;
};

}  // namespace mdm
