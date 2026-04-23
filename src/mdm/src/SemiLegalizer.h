// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

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

// Abacus-based row legalizer ported from the multi-die reference flow.
// Handles ICCAD 2022 fine-grid + multi-row scenarios that Opendp cannot
// converge on. Operates per child block; intersected-net terminals stay
// the responsibility of the caller.
class SemiLegalizer
{
 public:
  SemiLegalizer(odb::dbDatabase* db, utl::Logger* logger);

  // target_die: "" runs every child block (and optionally the top hier
  //             block when use_abacus is true and top_hier_die is set on
  //             the Abacus path). "top" / "bottom" select the first /
  //             second child of the top hier block, matching the ICCAD
  //             writer's TopDie / BottomDie order.
  void run(bool use_abacus, const std::string& target_die);

 private:
  struct AbacusCluster
  {
    std::vector<odb::dbInst*> insts;
    double e;  // weight of displacement in the objective
    double q;  // x = q/e
    double w;  // cluster width
    double x;  // optimal location (cluster's left edge)
  };

  using InstRow = std::vector<odb::dbInst*>;

  void runAbacus(const std::string& target_die, bool top_hier_die);
  void runAbacus(odb::dbBlock* block);
  // Trial-place every cell in `row` left-to-right with Abacus clustering.
  // After return, all cells in `row` have their final x positions; y is
  // already fixed at the row's y-coordinate by the caller.
  void placeRow(const InstRow& row);
  void addCell(AbacusCluster* cluster, odb::dbInst* inst);
  void addCluster(AbacusCluster* predecessor, AbacusCluster* cluster);
  void collapse(AbacusCluster* cluster,
                std::vector<AbacusCluster>& abacus_clusters);

  // Fallback shift-based legalizer (single-row only). Kept for completeness;
  // run() routes here when use_abacus is false.
  void doSimpleLegalize(bool top_hier_die);
  void doSimpleLegalize(odb::dbBlock* block);
  void shiftLegalize();
  void shiftCellsToLeft(InstRow& cell_row, odb::dbInst* inst, int idx);
  int degreeOfExcess(const InstRow& row) const;
  bool utilCheck() const;
  void adjustRowCapacity();

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  odb::dbBlock* target_block_ = nullptr;
};

}  // namespace mdm
