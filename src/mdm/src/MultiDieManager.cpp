// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "mdm/MultiDieManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "CellsLegalizer.h"
#include "GlobalTierOptimizer.h"
#include "SemiLegalizer.h"
#include "TerminalLegalizer.h"
#include "TestCaseManager.h"
#include "dpl/Opendp.h"
#include "gpl/Replace.h"
#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

using std::ifstream;
using std::ofstream;
using std::string;
using std::vector;

MultiDieManager::MultiDieManager(odb::dbDatabase* db,
                                 utl::Logger* logger,
                                 gpl::Replace* replace,
                                 dpl::Opendp* opendp)
    : db_(db),
      logger_(logger),
      replace_(replace),
      opendp_(opendp),
      test_case_manager_(std::make_unique<TestCaseManager>())
{
  test_case_manager_->setMDM(this);
}

std::vector<odb::dbBlock*> MultiDieManager::getChildBlocks() const
{
  std::vector<odb::dbBlock*> result;
  if (!db_ || !db_->getChip() || !db_->getChip()->getBlock()) {
    return result;
  }
  for (auto* child : db_->getChip()->getBlock()->getChildren()) {
    result.push_back(child);
  }
  return result;
}

int MultiDieManager::dieIndexOf(odb::dbBlock* block) const
{
  const auto children = getChildBlocks();
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == block) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

MultiDieManager::FromToBlocks MultiDieManager::findFromToBlocks(
    const std::vector<odb::dbBlock*>& children) const
{
  FromToBlocks result{children[0], children[1], true};
  if (children[0]->getInsts().size() < children[1]->getInsts().size()) {
    result.from = children[1];
    result.to = children[0];
    result.from_is_top_die = false;
  }
  return result;
}

void MultiDieManager::mapKnapsackCaps(
    const std::vector<odb::dbBlock*>& children,
    const FromToBlocks& ft,
    int u_t_percent,
    int u_b_percent,
    int64_t& out_cap_from_dbu,
    int64_t& out_cap_to_dbu) const
{
  out_cap_from_dbu = 0;
  out_cap_to_dbu = 0;
  // Convention: children[0] = top, children[1] = bottom. ICCAD header
  // gives u_t for top and u_b for bottom. Use children[0]'s core
  // (top die) for area to preserve the legacy behavior — different-tech
  // designs may have differing-dim cores between dies.
  odb::Rect core = children[0]->getCoreArea();
  int64_t core_area
      = static_cast<int64_t>(core.dx()) * static_cast<int64_t>(core.dy());
  if (core_area == 0) {
    return;
  }
  if (ft.from_is_top_die) {
    out_cap_from_dbu = core_area * u_t_percent / 100;
    out_cap_to_dbu = core_area * u_b_percent / 100;
  } else {
    out_cap_from_dbu = core_area * u_b_percent / 100;
    out_cap_to_dbu = core_area * u_t_percent / 100;
  }
}

odb::dbLib* MultiDieManager::findLibForBlock(odb::dbBlock* block) const
{
  if (!db_ || !block) {
    return nullptr;
  }
  const int target_die_id = dieIndexOf(block);
  if (target_die_id < 0) {
    return nullptr;
  }
  auto lib_iter = db_->getLibs().begin();
  if (lib_iter == db_->getLibs().end()) {
    return nullptr;
  }
  ++lib_iter;  // skip TopHierLib
  for (int i = 0; i < target_die_id && lib_iter != db_->getLibs().end(); ++i) {
    ++lib_iter;
  }
  return (lib_iter != db_->getLibs().end()) ? *lib_iter : nullptr;
}

int MultiDieManager::applyMigration(const std::vector<odb::dbInst*>& cells,
                                    odb::dbBlock* to_block)
{
  if (cells.empty() || !to_block) {
    return 0;
  }
  const int new_die_id = dieIndexOf(to_block);
  if (new_die_id < 0) {
    return 0;
  }
  int migrated = 0;
  for (auto* c : cells) {
    auto* prop = odb::dbIntProperty::find(c, "partition_id");
    if (prop) {
      prop->setValue(new_die_id);
    } else {
      odb::dbIntProperty::create(c, "partition_id", new_die_id);
    }
    SwitchInstanceHelper::switchInstanceToAssignedDie(this, c);
    ++migrated;
  }
  // Re-pair cross-die nets that became cross-die through this migration.
  const auto children = getChildBlocks();
  if (children.size() >= 2) {
    makeInterconnections(children[0], children[1]);
    makeIOPinInterconnections();
    odb::dbBlock* parent = db_->getChip()->getBlock();
    std::vector<odb::dbNet*> floating;
    for (auto* net : parent->getNets()) {
      if (net->getBTermCount() == 0 && net->getITermCount() == 0) {
        floating.push_back(net);
      }
    }
    for (auto* net : floating) {
      odb::dbNet::destroy(net);
    }
  }
  return migrated;
}

////////////////////////////////////////////////////////////////
// Multi-die detailed placement — runs dpl::Opendp on every child
// die in the top hier block using the block-aware detailedPlacement
// overload added in Stage 1.6.
////////////////////////////////////////////////////////////////

void MultiDieManager::multiDieDetailPlacement(int max_displacement_x,
                                              int max_displacement_y)
{
  if (!opendp_) {
    logger_->error(utl::MDM, 30, "Opendp pointer is null.");
    return;
  }
  odb::dbBlock* top_block = db_->getChip()->getBlock();
  auto children = top_block->getChildren();
  if (children.begin() == children.end()) {
    logger_->warn(
        utl::MDM,
        31,
        "No child dies found; call set_3D_IC before detailed placement.");
    return;
  }
  for (auto* child : children) {
    logger_->info(utl::MDM, 32, "Legalizing die {}", child->getName());
    opendp_->detailedPlacement(
        max_displacement_x, max_displacement_y, "", false, child);
  }
}

////////////////////////////////////////////////////////////////
// Multi-die SemiLegalizer — Abacus-based fallback for ICCAD
// 2022 fine-grid scenarios that Opendp cannot legalize.
////////////////////////////////////////////////////////////////

void MultiDieManager::runSemiLegalizer(const std::string& target_die,
                                       bool use_abacus,
                                       bool use_cells_dynamic_row,
                                       bool skip_pair_swap,
                                       bool use_tetris)
{
  if (use_cells_dynamic_row) {
    CellsLegalizer legalizer(db_, logger_);
    const auto mode = use_tetris ? CellsLegalizer::Mode::TETRIS
                                 : CellsLegalizer::Mode::ABACUS;
    legalizer.run(target_die, skip_pair_swap, mode);
    return;
  }
  SemiLegalizer legalizer(db_, logger_);
  legalizer.run(use_abacus, target_die);
}

MultiDieManager::~MultiDieManager() = default;

////////////////////////////////////////////////////////////////
// 3D IC configuration
////////////////////////////////////////////////////////////////

void MultiDieManager::set3DIC(int number_of_die, float area_ratio)
{
  number_of_die_ = number_of_die;
  shrink_area_ratio_ = area_ratio;
  logger_->info(utl::MDM, 1, "Set number of die to {}", number_of_die_);
  splitInstances();
}

void MultiDieManager::splitInstances()
{
  readPartitionInfo(partition_file_);
  makeSubBlocks();
  switchInstancesToAssignedDie();

  // Wire intersected nets between every (m, m+1) pair of child blocks.
  auto children = db_->getChip()->getBlock()->getChildren();
  auto it = children.begin();
  if (it == children.end()) {
    return;
  }
  odb::dbBlock* lower_block = *it;
  for (++it; it != children.end(); ++it) {
    odb::dbBlock* upper_block = *it;
    makeInterconnections(lower_block, upper_block);
    lower_block = upper_block;
  }

  // Wire top-hier block terminals into each child that also owns the net.
  makeIOPinInterconnections();

  // Strip floating nets on the top hier (their iTerms/BTerms have moved to
  // child blocks).
  vector<odb::dbNet*> floating;
  for (auto net : db_->getChip()->getBlock()->getNets()) {
    if (net->getBTermCount() == 0 && net->getITermCount() == 0) {
      floating.push_back(net);
    }
  }
  for (auto* net : floating) {
    odb::dbNet::destroy(net);
  }
}

void MultiDieManager::makeSubBlocks()
{
  odb::dbBlock* top_block = db_->getChip()->getBlock();
  const odb::Rect die_area = top_block->getDieArea();

  // A dbBlock inherits the chip's tech in recent ODB, so child blocks
  // cannot carry per-die LEF techs. Heterogeneous geometry is still
  // expressed through the separate dbLibs (TopLib, BottomLib) whose
  // masters are used when switching instances into each die.
  for (int die_idx = 0; die_idx < number_of_die_; ++die_idx) {
    const string die_name = "Die" + std::to_string(die_idx + 1);
    odb::dbBlock* child_block
        = odb::dbBlock::create(top_block, die_name.c_str());
    odb::dbInst::create(top_block, child_block, die_name.c_str());
    child_block->setDieArea(die_area);
    // DPL reads block->getCoreArea(); keep it aligned with the die area so
    // the ICCAD contest "entire die is usable" assumption is honoured.
    child_block->setCoreArea(die_area);
    if (!test_case_manager_->isICCADParsed()) {
      inheritRows(top_block, child_block);
    }
  }
  if (test_case_manager_->isICCADParsed()) {
    test_case_manager_->rowConstruction();
  }
}

void MultiDieManager::switchInstancesToAssignedDie()
{
  odb::dbBlock* top_block = db_->getChip()->getBlock();

  // Snapshot instance pointers first because switchInstanceToAssignedDie
  // destroys them inside the loop.
  vector<odb::dbInst*> inst_set;
  for (auto inst : top_block->getInsts()) {
    if (inst->getChild()) {
      continue;  // skip the instances that represent child blocks
    }
    inst_set.push_back(inst);
  }
  for (auto* inst : inst_set) {
    SwitchInstanceHelper::switchInstanceToAssignedDie(this, inst);
  }
}

void MultiDieManager::makeInterconnections(odb::dbBlock* lower_block,
                                           odb::dbBlock* upper_block)
{
  odb::dbBlock* top_hier = db_->getChip()->getBlock();
  int interconnect_count = 0;
  for (auto* lower_net : lower_block->getNets()) {
    const string net_name = lower_net->getName();
    const string interconnect_name = net_name + "Interconnection";
    // Cross-call idempotency: if this BTerm already exists in
    // lower_block, the net has already been paired. The Phase 4.2
    // -apply path may invoke this incrementally after cross-die
    // migrations, so some lower_block nets are already paired from
    // the initial set_3D_IC call. BTerm names are unique within a
    // block, so a name lookup is a sufficient signal — no DB-state
    // metadata required.
    if (lower_block->findBTerm(interconnect_name.c_str()) != nullptr) {
      continue;
    }
    odb::dbNet* upper_net = upper_block->findNet(net_name.c_str());
    if (!upper_net) {
      continue;
    }
    odb::dbNet* top_hier_net = top_hier->findNet(net_name.c_str());
    if (!top_hier_net) {
      // top_hier net was stripped after the first splitInstances pass
      // (it became floating). Recreate so BTerms have something to land on.
      top_hier_net = odb::dbNet::create(top_hier, net_name.c_str());
    }

    odb::dbBTerm* lower_term
        = odb::dbBTerm::create(lower_net, interconnect_name.c_str());
    odb::dbBTerm* upper_term
        = odb::dbBTerm::create(upper_net, interconnect_name.c_str());
    lower_term->getITerm()->connect(top_hier_net);
    upper_term->getITerm()->connect(top_hier_net);

    // The "intersected" dbBoolProperty marker is a downstream signal
    // for CellsLegalizer (Stage 3.3 sibling-bbox cache), independent
    // of the idempotency check above. Keep writing it.
    odb::dbBoolProperty::create(top_hier_net, "intersected", true);
    odb::dbBoolProperty::create(lower_net, "intersected", true);
    odb::dbBoolProperty::create(upper_net, "intersected", true);
    interconnect_count++;
  }
  logger_->info(
      utl::MDM, 12, "The interconnection number: {}", interconnect_count);
}

void MultiDieManager::makeIOPinInterconnections()
{
  odb::dbBlock* top_block = db_->getChip()->getBlock();
  for (auto* bterm : top_block->getBTerms()) {
    odb::dbNet* top_hier_net = bterm->getNet();
    if (!top_hier_net) {
      continue;
    }
    for (auto* child : db_->getChip()->getBlock()->getChildren()) {
      odb::dbNet* child_net = child->findNet(top_hier_net->getName().c_str());
      if (!child_net) {
        continue;
      }
      odb::dbBTerm* child_term
          = odb::dbBTerm::create(child_net, bterm->getName().c_str());
      child_term->getITerm()->connect(top_hier_net);
      child_term->setIoType(bterm->getIoType());
      child_term->setSigType(bterm->getSigType());

      for (auto* top_pin : bterm->getBPins()) {
        odb::dbBPin* child_pin = odb::dbBPin::create(child_term);
        for (auto* top_box : top_pin->getBoxes()) {
          odb::dbTechLayer* layer = child->getTech()->findLayer(
              top_box->getTechLayer()->getName().c_str());
          if (!layer) {
            // The child block shares the top-hier tech in current ODB, so
            // this should only fail if the user built a custom tech tree;
            // skip gracefully rather than assert.
            continue;
          }
          odb::dbBox::create(child_pin,
                             layer,
                             top_box->xMin(),
                             top_box->yMin(),
                             top_box->xMax(),
                             top_box->yMax());
        }
      }
    }
  }
}

void MultiDieManager::inheritRows(odb::dbBlock* parent_block,
                                  odb::dbBlock* child_block)
{
  for (auto* row : parent_block->getRows()) {
    const odb::Point origin = row->getOrigin();
    odb::dbRow::create(child_block,
                       (child_block->getName() + "Site").c_str(),
                       row->getSite(),
                       origin.x(),
                       origin.y(),
                       row->getOrient(),
                       row->getDirection(),
                       row->getSiteCount(),
                       row->getSpacing());
  }
}

////////////////////////////////////////////////////////////////
// Partitioning
////////////////////////////////////////////////////////////////

void MultiDieManager::readPartitionInfo(const string& path)
{
  const string file_name = path.empty() ? partition_file_ : path;
  ifstream partition_file(file_name);
  if (!partition_file.is_open()) {
    // Default: split instances evenly across the configured number of dies,
    // in deterministic iteration order.
    logger_->warn(utl::MDM, 3, "Cannot open partition file, use default");
    vector<odb::dbInst*> inst_set;
    for (auto inst : db_->getChip()->getBlock()->getInsts()) {
      inst_set.push_back(inst);
    }
    for (auto* child : db_->getChip()->getBlock()->getChildren()) {
      for (auto inst : child->getInsts()) {
        inst_set.push_back(inst);
      }
    }
    const int dies = std::max(1, number_of_die_);
    const int per_die = static_cast<int>(inst_set.size()) / dies;
    int die = 0;
    int count = 0;
    for (auto* inst : inst_set) {
      odb::dbIntProperty::create(inst, "partition_id", die);
      count++;
      if (count >= per_die && die < dies - 1) {
        die++;
        count = 0;
      }
    }
    return;
  }

  string line;
  while (std::getline(partition_file, line)) {
    std::istringstream iss(line);
    string inst_name;
    int partition_id;
    if (!(iss >> inst_name >> partition_id)) {
      continue;
    }
    odb::dbInst* inst = db_->getChip()->getBlock()->findInst(inst_name.c_str());
    if (!inst) {
      for (auto* child : db_->getChip()->getBlock()->getChildren()) {
        inst = child->findInst(inst_name.c_str());
        if (inst) {
          break;
        }
      }
    }
    if (inst) {
      odb::dbIntProperty::create(inst, "partition_id", partition_id);
    }
  }
}

////////////////////////////////////////////////////////////////
// QoR / debug utilities
////////////////////////////////////////////////////////////////

void MultiDieManager::get3DHPWL(bool approximate)
{
  int64_t hpwl = 0;
  for (auto* block : db_->getChip()->getBlock()->getChildren()) {
    for (auto* net : block->getNets()) {
      if (odb::dbBoolProperty::find(net, "intersected")) {
        continue;
      }
      odb::Rect bbox = net->getTermBBox();
      hpwl += bbox.dx() + bbox.dy();
    }
  }

  auto children = db_->getChip()->getBlock()->getChildren();
  auto child_it = children.begin();
  if (child_it == children.end()) {
    logger_->report("HPWL is: 0 (no child blocks — call set_3D_IC first)");
    return;
  }
  odb::dbBlock* first_child = *child_it;

  for (auto* intersected_net_1 : first_child->getNets()) {
    if (!odb::dbBoolProperty::find(intersected_net_1, "intersected")) {
      continue;
    }
    odb::dbNet* intersected_net_2 = nullptr;
    for (auto* bterm : intersected_net_1->getBTerms()) {
      odb::dbITerm* upper = bterm->getITerm();
      if (!upper || !upper->getNet()) {
        continue;
      }
      for (auto* iterm : upper->getNet()->getITerms()) {
        odb::dbBTerm* b = iterm->getBTerm();
        if (b && b->getBlock() != intersected_net_1->getBlock()) {
          intersected_net_2 = b->getNet();
        }
      }
    }
    if (!intersected_net_2) {
      continue;
    }
    odb::Rect box1 = intersected_net_1->getTermBBox();
    odb::Rect box2 = intersected_net_2->getTermBBox();
    odb::Rect box3;
    if (approximate) {
      if (box1.intersects(box2)) {
        box3 = box1.intersect(box2);
      } else {
        vector<int> xs{box1.xMin(), box1.xMax(), box2.xMin(), box2.xMax()};
        vector<int> ys{box1.yMin(), box1.yMax(), box2.yMin(), box2.yMax()};
        std::sort(xs.begin(), xs.end());
        std::sort(ys.begin(), ys.end());
        box3.init(xs.at(1), ys.at(1), xs.at(2), ys.at(2));
      }
      odb::Rect center{
          box3.xCenter(), box3.yCenter(), box3.xCenter(), box3.yCenter()};
      box1.merge(center);
      box2.merge(center);
    }
    hpwl += box1.dx() + box1.dy();
    hpwl += box2.dx() + box2.dy();
  }

  const int scale
      = test_case_manager_->getScale() > 0 ? test_case_manager_->getScale() : 1;
  hpwl /= scale;

  std::ostringstream oss;
  oss.imbue(std::locale(""));
  oss << std::fixed << std::setprecision(2) << hpwl;
  logger_->report("HPWL is: {}", oss.str());
}

void MultiDieManager::getHPWL(const char* die_info)
{
  const string which_die = die_info ? string(die_info) : string();
  odb::dbBlock* block = nullptr;
  if (!which_die.empty()) {
    const int target = (which_die == "top") ? 1 : 2;
    int idx = 0;
    for (auto* child : db_->getChip()->getBlock()->getChildren()) {
      if (++idx == target) {
        block = child;
        break;
      }
    }
  } else {
    block = db_->getChip()->getBlock();
  }
  if (!block) {
    logger_->error(utl::MDM, 13, "Invalid die information or block not found.");
    return;
  }

  int64_t hpwl = 0;
  for (auto* net : block->getNets()) {
    odb::Rect bbox = net->getTermBBox();
    hpwl += bbox.dx() + bbox.dy();
  }
  std::ostringstream oss;
  oss.imbue(std::locale(""));
  oss << std::fixed << std::setprecision(2) << hpwl;
  logger_->report("{} HPWL is: {}", block->getName(), oss.str());
}

void MultiDieManager::exportCoordinates(const string& file_name)
{
  ofstream out(file_name);
  if (!out.is_open()) {
    logger_->error(
        utl::MDM, 21, "Cannot open coord export file: {}", file_name);
    return;
  }
  vector<odb::dbBlock*> blocks;
  blocks.push_back(db_->getChip()->getBlock());
  for (auto* child : db_->getChip()->getBlock()->getChildren()) {
    blocks.push_back(child);
  }
  for (auto* block : blocks) {
    for (auto inst : block->getInsts()) {
      if (inst->getPlacementStatus() != odb::dbPlacementStatus::PLACED) {
        continue;
      }
      odb::Point loc = inst->getLocation();
      out << inst->getName() << ' ' << loc.getX() << ' ' << loc.getY() << '\n';
    }
  }
}

void MultiDieManager::importCoordinates(const string& file_name)
{
  ifstream in(file_name);
  if (!in.is_open()) {
    logger_->error(
        utl::MDM, 22, "Cannot open coord import file: {}", file_name);
    return;
  }
  vector<odb::dbBlock*> blocks;
  blocks.push_back(db_->getChip()->getBlock());
  for (auto* child : db_->getChip()->getBlock()->getChildren()) {
    blocks.push_back(child);
  }
  string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    string inst_name;
    int x, y;
    if (!(iss >> inst_name >> x >> y)) {
      continue;
    }
    for (auto* block : blocks) {
      odb::dbInst* inst = block->findInst(inst_name.c_str());
      if (inst) {
        inst->setLocation(x, y);
        inst->setPlacementStatus(odb::dbPlacementStatus::PLACED);
      }
    }
  }
}

////////////////////////////////////////////////////////////////
// ICCAD 2022 I/O
////////////////////////////////////////////////////////////////

void MultiDieManager::setICCADScale(int scale)
{
  test_case_manager_->setScale(scale);
}

void MultiDieManager::readICCAD2022(const string& case_file)
{
  const std::filesystem::path p(case_file);
  const string stem = p.stem().string();

  TestCaseManager::TESTCASE test_case = TestCaseManager::NA;
  if (stem == "case1") {
    test_case = TestCaseManager::ICCAD2022_CASE1;
  } else if (stem == "case2") {
    test_case = TestCaseManager::ICCAD2022_CASE2;
  } else if (stem == "case3") {
    test_case = TestCaseManager::ICCAD2022_CASE3;
  } else if (stem == "case4") {
    test_case = TestCaseManager::ICCAD2022_CASE4;
  } else if (stem == "case2_hidden") {
    test_case = TestCaseManager::ICCAD2022_CASE2_H;
  } else if (stem == "case3_hidden") {
    test_case = TestCaseManager::ICCAD2022_CASE3_H;
  } else if (stem == "case4_hidden") {
    test_case = TestCaseManager::ICCAD2022_CASE4_H;
  } else {
    test_case = TestCaseManager::lookupTestCase(case_file);
  }

  if (test_case == TestCaseManager::NA) {
    logger_->error(
        utl::MDM, 9, "Cannot resolve ICCAD 2022 test case for: {}", case_file);
    return;
  }

  test_case_manager_->setBenchmarkDir(p.parent_path().string());
  test_case_manager_->ICCADContest(test_case, this);
  logger_->info(utl::MDM, 10, "Successfully parsed ICCAD 2022: {}", case_file);
}

void MultiDieManager::writeICCAD2022Output(const string& file_name)
{
  ofstream out(file_name);
  if (!out.is_open()) {
    logger_->error(utl::MDM, 20, "Cannot open output file: {}", file_name);
    return;
  }

  odb::dbBlock* top_block = db_->getChip()->getBlock();
  auto children = top_block->getChildren();

  odb::dbBlock* top_die = nullptr;
  odb::dbBlock* bottom_die = nullptr;
  {
    auto it = children.begin();
    if (it != children.end()) {
      top_die = *it;
      ++it;
    }
    if (it != children.end()) {
      bottom_die = *it;
    }
  }

  const int scale
      = test_case_manager_->getScale() > 0 ? test_case_manager_->getScale() : 1;

  auto emit_die = [&](const string& header, odb::dbBlock* block) {
    vector<odb::dbInst*> insts;
    if (block) {
      for (auto inst : block->getInsts()) {
        insts.push_back(inst);
      }
    }
    out << header << ' ' << insts.size() << '\n';
    for (auto* inst : insts) {
      const odb::Point loc = inst->getLocation();
      out << "Inst " << inst->getName() << ' ' << loc.getX() / scale << ' '
          << loc.getY() / scale << '\n';
    }
  };

  if (top_die == nullptr) {
    emit_die("TopDiePlacement", top_block);
    out << "BottomDiePlacement 0\n";
  } else {
    emit_die("TopDiePlacement", top_die);
    emit_die("BottomDiePlacement", bottom_die);
  }

  // Hybrid-bond terminals. Contest rules require:
  //   1. Each terminal sits inside the die with `boundary-spacing` margin.
  //   2. Any two terminals are separated by `spacing` (centre-to-centre
  //      distance >= terminal_size + spacing on either axis).
  //
  // The terminal size + spacing `C` defines a uniform grid whose origin is
  // (size/2 + spacing) from the die boundary — that choice trivially
  // satisfies both rules and matches the iPL-3D reference output
  // (case2: 150 + 200·i). The per-terminal assignment is computed by
  // TerminalLegalizer: each terminal carries its Terminal Optimal Region
  // (TOR, iPL-3D paper Definition 2) — the rectangle in which the combined
  // top/bottom HPWL contribution is minimum — and is matched to the grid
  // cell that minimises Manhattan distance to that rectangle (zero when
  // inside).
  //
  // Top-hier net::getTermBBox() reports (0,0) for our intersected nets
  // because the BTerm coordinates live in child-block scope, so we walk
  // the same-named net inside each child block and merge their bboxes to
  // reconstruct the TOR.
  odb::dbChip* chip = db_->getChip();
  auto* hb_x_prop = odb::dbIntProperty::find(chip, "hybridBondX");
  auto* hb_y_prop = odb::dbIntProperty::find(chip, "hybridBondY");
  auto* hb_s_prop = odb::dbIntProperty::find(chip, "hybridBondSpacing");
  const int hb_w = hb_x_prop ? hb_x_prop->getValue() : 0;
  const int hb_h = hb_y_prop ? hb_y_prop->getValue() : 0;
  const int hb_s = hb_s_prop ? hb_s_prop->getValue() : 0;
  const odb::Rect die = top_block->getDieArea();

  TerminalLegalizer::Config cfg;
  cfg.step_x = std::max(1, hb_w + hb_s);
  cfg.step_y = std::max(1, hb_h + hb_s);
  cfg.origin_x = die.xMin() + hb_w / 2 + hb_s;
  cfg.origin_y = die.yMin() + hb_h / 2 + hb_s;
  const int max_x = die.xMax() - hb_w / 2 - hb_s;
  const int max_y = die.yMax() - hb_h / 2 - hb_s;
  cfg.grid_w = std::max(1, (max_x - cfg.origin_x) / cfg.step_x + 1);
  cfg.grid_h = std::max(1, (max_y - cfg.origin_y) / cfg.step_y + 1);

  TerminalLegalizer legalizer(logger_, cfg);

  vector<odb::dbBlock*> child_blocks;
  for (auto* child : children) {
    child_blocks.push_back(child);
  }

  for (auto* net : top_block->getNets()) {
    if (!odb::dbBoolProperty::find(net, "intersected")) {
      continue;
    }
    // Collect per-child bboxes (typically one from top die, one from bottom).
    vector<odb::Rect> child_bboxes;
    for (auto* child : child_blocks) {
      odb::dbNet* child_net = child->findNet(net->getName().c_str());
      if (!child_net) {
        continue;
      }
      child_bboxes.push_back(child_net->getTermBBox());
    }

    // Construct the TOR rectangle. Paper Definition 2: the optimal region
    // is the "bounding boxes' midpoint of the net e_j⁻ and net e_j⁺".
    // In 1D the HPWL sum is minimised over the middle range of the four
    // sorted coordinates {xMin_top, xMax_top, xMin_bot, xMax_bot};
    // generalises to the intersection-or-gap interval in each axis.
    int tor_xlo = (die.xMin() + die.xMax()) / 2;
    int tor_xhi = tor_xlo;
    int tor_ylo = (die.yMin() + die.yMax()) / 2;
    int tor_yhi = tor_ylo;
    if (child_bboxes.size() >= 2) {
      int xs[4] = {child_bboxes[0].xMin(),
                   child_bboxes[0].xMax(),
                   child_bboxes[1].xMin(),
                   child_bboxes[1].xMax()};
      int ys[4] = {child_bboxes[0].yMin(),
                   child_bboxes[0].yMax(),
                   child_bboxes[1].yMin(),
                   child_bboxes[1].yMax()};
      std::sort(std::begin(xs), std::end(xs));
      std::sort(std::begin(ys), std::end(ys));
      tor_xlo = xs[1];
      tor_xhi = xs[2];
      tor_ylo = ys[1];
      tor_yhi = ys[2];
    } else if (child_bboxes.size() == 1) {
      const odb::Rect& b = child_bboxes.front();
      tor_xlo = b.xMin();
      tor_xhi = b.xMax();
      tor_ylo = b.yMin();
      tor_yhi = b.yMax();
    }
    legalizer.addTerminal(net->getName(), tor_xlo, tor_ylo, tor_xhi, tor_yhi);
  }

  legalizer.legalize();

  const auto& terms = legalizer.terminals();
  out << "NumTerminals " << terms.size() << '\n';
  for (const auto& term : terms) {
    out << "Terminal " << term.name << ' ' << term.x / scale << ' '
        << term.y / scale << '\n';
  }
}

void MultiDieManager::parseICCADOutput(const string& file_name,
                                       const char* which_die)
{
  test_case_manager_->parseICCADOutput(file_name, which_die);
}

std::pair<int, int> MultiDieManager::getMaxUtils() const
{
  if (!test_case_manager_) {
    return {0, 0};
  }
  return test_case_manager_->getMaxUtils();
}

int MultiDieManager::getICCADScale() const
{
  if (!test_case_manager_) {
    return 1;
  }
  return std::max(1, test_case_manager_->getScale());
}

////////////////////////////////////////////////////////////////
// Phase 4 — iPL-3D Global Tier Optimization plumbing (skeleton).
// Bodies are filled in Phases 4.2/4.4/4.6.
////////////////////////////////////////////////////////////////

void MultiDieManager::runFlattenedPlacement(double density,
                                            double target_density,
                                            int nesterov_max_iter,
                                            bool skip_io_mode)
{
  logger_->info(utl::MDM,
                304,
                "runFlattenedPlacement: stub (Task 1). density={}, "
                "target_density={}, nesterov_max_iter={}, skip_io_mode={}.",
                density,
                target_density,
                nesterov_max_iter,
                skip_io_mode);
}

void MultiDieManager::runGlobalTierOptimization(double rho,
                                                double alpha,
                                                double beta,
                                                double gamma,
                                                double b_factor,
                                                int max_net_fanout,
                                                int u_t_percent,
                                                int u_b_percent,
                                                bool apply)
{
  TierOptParams params;
  params.rho = rho;
  params.alpha = alpha;
  params.beta = beta;
  params.gamma = gamma;
  params.B_factor = b_factor;
  params.max_net_fanout = max_net_fanout;
  // dbu/μm conversion so the surrogate runs in paper's normalized μm
  // units; paper Table III's ρ=500, α=100, β=0.5 then apply directly.
  params.dbu_per_um = getICCADScale();
  // u_t/u_b: caller-provided > 0 wins; else fall back to ICCAD case
  // header (TestCaseManager); else struct default.
  auto utils = getMaxUtils();
  if (u_t_percent > 0) {
    params.u_t_percent = u_t_percent;
  } else if (utils.first > 0) {
    params.u_t_percent = utils.first;
  }
  if (u_b_percent > 0) {
    params.u_b_percent = u_b_percent;
  } else if (utils.second > 0) {
    params.u_b_percent = utils.second;
  }

  const auto children = getChildBlocks();
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  308,
                  "runGlobalTierOptimization: need >=2 child blocks; got {}.",
                  children.size());
    return;
  }

  const FromToBlocks ft = findFromToBlocks(children);
  mapKnapsackCaps(children,
                  ft,
                  params.u_t_percent,
                  params.u_b_percent,
                  params.cap_from_dbu,
                  params.cap_to_dbu);
  odb::dbLib* to_lib = findLibForBlock(ft.to);

  GlobalTierOptimizer optimizer(db_, logger_);
  auto delta = optimizer.run(ft.from, ft.to, params, to_lib);

  if (apply && !delta.empty()) {
    const int migrated = applyMigration(delta, ft.to);
    logger_->info(utl::MDM,
                  306,
                  "runGlobalTierOptimization: returned {} cells, applied "
                  "{} migrations to die {}.",
                  delta.size(),
                  migrated,
                  dieIndexOf(ft.to));
  } else {
    logger_->info(utl::MDM,
                  309,
                  "runGlobalTierOptimization: returned {} cells, apply={} "
                  "(no migration).",
                  delta.size(),
                  apply);
  }
}

namespace {

// RAII-scoped placement-status toggle: marks every inst in `block` as
// FIRM on construction, restores its prior status on destruction.
// Honours odb's FIRM behaviour: gpl::PlacerBase::isFixed() now returns
// true for these cells, so doIncrementalPlace skips them.
class ScopedFirmFreeze
{
 public:
  explicit ScopedFirmFreeze(odb::dbBlock* block) : block_(block)
  {
    if (!block_) {
      return;
    }
    for (odb::dbInst* inst : block_->getInsts()) {
      saved_.emplace_back(inst, inst->getPlacementStatus());
      inst->setPlacementStatus(odb::dbPlacementStatus::FIRM);
    }
  }

  ~ScopedFirmFreeze()
  {
    for (auto& [inst, status] : saved_) {
      inst->setPlacementStatus(status);
    }
  }

  ScopedFirmFreeze(const ScopedFirmFreeze&) = delete;
  ScopedFirmFreeze& operator=(const ScopedFirmFreeze&) = delete;

 private:
  odb::dbBlock* block_;
  std::vector<std::pair<odb::dbInst*, odb::dbPlacementStatus>> saved_;
};

}  // namespace

void MultiDieManager::runPlanarCorrecting(int iterations,
                                          double density,
                                          double intersected_net_weight,
                                          int nesterov_max_iter,
                                          bool skip_io_mode)
{
  if (!replace_) {
    logger_->error(
        utl::MDM, 313, "runPlanarCorrecting: gpl::Replace pointer is null.");
    return;
  }
  const auto children = getChildBlocks();
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  310,
                  "runPlanarCorrecting: need >=2 child blocks (got {}); call "
                  "set_3D_IC + run_global_tier_optimization -apply first.",
                  children.size());
    return;
  }
  for (int k = 0; k < iterations; ++k) {
    for (size_t active_idx = 0; active_idx < children.size(); ++active_idx) {
      // Freeze every die except the active one.
      std::vector<std::unique_ptr<ScopedFirmFreeze>> freezes;
      for (size_t i = 0; i < children.size(); ++i) {
        if (i == active_idx) {
          continue;
        }
        freezes.push_back(std::make_unique<ScopedFirmFreeze>(children[i]));
      }
      logger_->info(utl::MDM,
                    312,
                    "runPlanarCorrecting: iter {}, active die {} (others "
                    "FIRM-frozen).",
                    k,
                    active_idx);
      gpl::PlaceOptions opts;
      opts.skipIoMode = skip_io_mode;
      opts.density = density;
      opts.intersectedNetWeight = intersected_net_weight;
      opts.nesterovPlaceMaxIter = nesterov_max_iter;
      // Use doNesterovPlace (skips initial-place CG iterations that
      // crash on the post-migration mixed-block layout). Cells already
      // have valid placements from the migration, so initial place is
      // not needed.
      replace_->doNesterovPlace(/*threads=*/1, opts);
      // RAII destructors restore statuses here.
    }
  }
}

void MultiDieManager::snapCellsToRows()
{
  int total_snapped = 0;
  int total_max_dy = 0;
  for (auto* child : getChildBlocks()) {
    auto rows = child->getRows();
    if (rows.begin() == rows.end()) {
      continue;
    }
    // Collect row y_mins in sorted order; assume uniform row pitch.
    const int row_height = (*rows.begin())->getBBox().dy();
    const int y_min = (*rows.begin())->getBBox().yMin();
    int num_rows = 0;
    for (auto* r : rows) {
      (void) r;
      ++num_rows;
    }
    int max_dy = 0;
    for (auto* inst : child->getInsts()) {
      const odb::Point loc = inst->getLocation();
      const int orig_y = loc.y();
      // Nearest row index, clamped to [0, num_rows-1].
      int row_idx = (orig_y - y_min + row_height / 2) / row_height;
      if (row_idx < 0) {
        row_idx = 0;
      }
      if (row_idx >= num_rows) {
        row_idx = num_rows - 1;
      }
      const int snapped_y = y_min + row_idx * row_height;
      if (snapped_y != orig_y) {
        ++total_snapped;
        max_dy = std::max(max_dy, std::abs(snapped_y - orig_y));
      }
      inst->setLocation(loc.x(), snapped_y);
    }
    total_max_dy = std::max(total_max_dy, max_dy);
  }
  logger_->info(utl::MDM,
                314,
                "snapCellsToRows: snapped {} cells, max |Δy|={} dbu.",
                total_snapped,
                total_max_dy);
}

}  // namespace mdm
