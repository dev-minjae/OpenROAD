// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "mdm/MultiDieManager.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

#include "TestCaseManager.h"
#include "dpl/Opendp.h"
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
    odb::dbNet* upper_net = upper_block->findNet(net_name.c_str());
    if (!upper_net) {
      continue;
    }
    odb::dbNet* top_hier_net = top_hier->findNet(net_name.c_str());
    assert(top_hier_net);

    const string interconnect_name = net_name + "Interconnection";
    odb::dbBTerm* lower_term
        = odb::dbBTerm::create(lower_net, interconnect_name.c_str());
    odb::dbBTerm* upper_term
        = odb::dbBTerm::create(upper_net, interconnect_name.c_str());
    lower_term->getITerm()->connect(top_hier_net);
    upper_term->getITerm()->connect(top_hier_net);

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

  // Hybrid-bond terminals — the x/y centre of every top-hier net marked
  // intersected. Stage 1 leaves these at the net bounding-box centre;
  // Stage 3 will explore force-directed refinement.
  vector<std::pair<string, odb::Point>> terminals;
  for (auto* net : top_block->getNets()) {
    if (!odb::dbBoolProperty::find(net, "intersected")) {
      continue;
    }
    const odb::Rect box = net->getTermBBox();
    terminals.emplace_back(net->getName(),
                           odb::Point(box.xCenter(), box.yCenter()));
  }
  out << "NumTerminals " << terminals.size() << '\n';
  for (const auto& [name, pt] : terminals) {
    out << "Terminal " << name << ' ' << pt.getX() / scale << ' '
        << pt.getY() / scale << '\n';
  }
}

void MultiDieManager::parseICCADOutput(const string& file_name,
                                       const char* which_die)
{
  test_case_manager_->parseICCADOutput(file_name, which_die);
}

}  // namespace mdm
