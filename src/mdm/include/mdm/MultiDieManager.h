// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

#include <memory>
#include <string>

namespace odb {
class dbDatabase;
class dbBlock;
class dbInst;
class dbNet;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace gpl {
class Replace;
}  // namespace gpl

namespace dpl {
class Opendp;
}  // namespace dpl

namespace mdm {

class TestCaseManager;
class GlobalTierOptimizer;
class FastTerminalLegalizer;
class BilevelCoordinator;
struct TierOptParams;
struct BilevelParams;

class MultiDieManager
{
 public:
  MultiDieManager(odb::dbDatabase* db,
                  utl::Logger* logger,
                  gpl::Replace* replace,
                  dpl::Opendp* opendp);
  ~MultiDieManager();

  odb::dbDatabase* getDb() const { return db_; }
  utl::Logger* getLogger() const { return logger_; }
  gpl::Replace* getReplace() const { return replace_; }
  dpl::Opendp* getOpendp() const { return opendp_; }

  // Run detailed placement on each child die. Assumes set_3D_IC + global
  // placement have already run. Calls dpl::Opendp::detailedPlacement with
  // the new block overload added in Stage 1.6.
  void multiDieDetailPlacement(int max_displacement_x = 0,
                               int max_displacement_y = 0);

  // Abacus-based row legalizer for child dies. Used for ICCAD 2022 fine-grid
  // scenarios where Opendp cannot converge. target_die: ""/"top"/"bottom".
  // When use_cells_dynamic_row is true (default) the new CellsLegalizer is
  // used; the legacy SemiLegalizer remains accessible via
  // -no_cells_dynamic_row.
  void runSemiLegalizer(const std::string& target_die = "",
                        bool use_abacus = true,
                        bool use_cells_dynamic_row = true,
                        bool skip_pair_swap = false,
                        bool use_tetris = false);

  // 3D IC configuration. Calling set3DIC triggers splitInstances: read the
  // partition file (or fall back to a half/half split), create child blocks
  // per die, move instances into the assigned dies, and wire intersected
  // nets between them.
  void set3DIC(int number_of_die, float area_ratio = 0.5);
  int getNumberOfDie() const { return number_of_die_; }
  void setNumberOfDie(int n) { number_of_die_ = n; }

  // ICCAD 2022 contest I/O.
  void readICCAD2022(const std::string& case_file);
  void writeICCAD2022Output(const std::string& file_name);
  void parseICCADOutput(const std::string& file_name,
                        const char* which_die = "");
  void setICCADScale(int scale);

  // Partitioning.
  void setPartitionFile(const std::string& path) { partition_file_ = path; }
  const std::string& getPartitionFile() const { return partition_file_; }
  void readPartitionInfo(const std::string& path);

  // Utilities.
  void get3DHPWL(bool approximate = true);
  void getHPWL(const char* die_info = nullptr);
  void exportCoordinates(const std::string& file_name);
  void importCoordinates(const std::string& file_name);

  // (top_max_util, bottom_max_util) percentage from ICCAD 2022 case header.
  // {0, 0} if no ICCAD case has been parsed.
  std::pair<int, int> getMaxUtils() const;

  // Phase 4 — iPL-3D paper §IV.A flattened init wrapper. Phase 4.1: stub.
  // Phase 4.4 implements: temporarily homes all cells to one die, runs
  // RePlAce with relaxed density, restores partition state.
  void runFlattenedPlacement(double density = 1.0);

  // Phase 4 — iPL-3D paper §IV.B Algorithm 2 single-shot. Phase 4.1: stub.
  // Phase 4.2 implements via GlobalTierOptimizer.
  void runGlobalTierOptimization(double rho = 500.0,
                                 double alpha = 100.0,
                                 double beta = 0.5,
                                 double gamma = 0.0,
                                 bool apply = false);

  // Phase 4 — iPL-3D paper §IV.D Planar Solution Correcting (SP-2).
  // For each of `iterations` rounds, freezes each child die in turn
  // (sets cells to FIRM placement status, runs `replace_->doIncrementalPlace`
  // so only the un-frozen die moves, then restores statuses). Re-uses the
  // RAII-scoped FIRM toggle planned in Phase 4.0 research §4.
  void runPlanarCorrecting(int iterations = 1);

  // Phase 4 helper — snap each child-die cell's y to its nearest row's
  // y_min. doNesterovPlace produces free-form (row-unaligned) output;
  // CellsLegalizer's row-pack assumes near-row-aligned input. This helper
  // bridges the gap without touching x. ICCAD 2022's site grid is non-
  // uniform in cell width so standard dpl::Opendp loops; CellsLegalizer
  // is the right downstream legalizer once cells are y-aligned.
  void snapCellsToRows();

  // Phase 4 — iPL-3D paper Algorithm 1 bilevel coordinator. Phase 4.1: stub.
  // Phase 4.6 implements full SP-1 ↔ SP-2 alternation.
  void run3DPlacement(int iterations = 4, bool no_alternating = false);

 private:
  void splitInstances();
  void makeSubBlocks();
  void switchInstancesToAssignedDie();
  void makeInterconnections(odb::dbBlock* lower_block,
                            odb::dbBlock* upper_block);
  void makeIOPinInterconnections();
  void inheritRows(odb::dbBlock* parent_block, odb::dbBlock* child_block);

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  gpl::Replace* replace_ = nullptr;
  dpl::Opendp* opendp_ = nullptr;

  int number_of_die_ = 0;
  float shrink_area_ratio_ = 0.0f;

  std::string partition_file_;

  std::unique_ptr<TestCaseManager> test_case_manager_;
};

// SwitchInstanceHelper moves an instance from the top hierarchical block to
// the child block matching its partition_id property. The helper preserves
// placement status, net connectivity (creating nets on the child as needed),
// arbitrary dbProperty values, and group assignments.
class SwitchInstanceHelper
{
 public:
  static void switchInstanceToAssignedDie(MultiDieManager* manager,
                                          odb::dbInst* original_inst);
};

}  // namespace mdm
