// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "TestCaseManager.h"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "mdm/MultiDieManager.h"
#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

using namespace odb;
using std::ifstream;
using std::map;
using std::pair;
using std::string;
using std::vector;

////////////////////////////////////////////////////////////////
// TestCaseManager
////////////////////////////////////////////////////////////////

void TestCaseManager::setMDM(MultiDieManager* mdm)
{
  mdm_ = mdm;
  iccad_output_parser_.setDb(mdm_->getDb());
}

void TestCaseManager::setScale(int scale)
{
  scale_ = scale;
  iccad_output_parser_.setScale(scale);
}

void TestCaseManager::setSiteDefined(bool site_defined)
{
  site_defined_ = site_defined;
}

TestCaseManager::TESTCASE TestCaseManager::lookupTestCase(const string& name)
{
  static const map<string, TESTCASE> table{
      {"2022-test1", ICCAD2022_CASE1},
      {"2022-test2", ICCAD2022_CASE2},
      {"2022-test3", ICCAD2022_CASE3},
      {"2022-test4", ICCAD2022_CASE4},
      {"2022-test2-h", ICCAD2022_CASE2_H},
      {"2022-test3-h", ICCAD2022_CASE3_H},
      {"2022-test4-h", ICCAD2022_CASE4_H},
  };
  auto it = table.find(name);
  if (it == table.end()) {
    return NA;
  }
  return it->second;
}

pair<string, int> TestCaseManager::fetchInputFileInfo(TESTCASE test_case)
{
  map<TESTCASE, pair<string, int>> files;
  // ICCAD 2022 Problem B — 2-die F2F 3D placement
  const string prefix
      = benchmark_dir_.empty() ? string() : benchmark_dir_ + "/";
  files[ICCAD2022_CASE1] = {prefix + "case1.txt", 2022};
  files[ICCAD2022_CASE2] = {prefix + "case2.txt", 2022};
  files[ICCAD2022_CASE3] = {prefix + "case3.txt", 2022};
  files[ICCAD2022_CASE4] = {prefix + "case4.txt", 2022};
  files[ICCAD2022_CASE2_H] = {prefix + "case2_hidden.txt", 2022};
  files[ICCAD2022_CASE3_H] = {prefix + "case3_hidden.txt", 2022};
  files[ICCAD2022_CASE4_H] = {prefix + "case4_hidden.txt", 2022};

  testcase_ = test_case;
  auto it = files.find(test_case);
  if (it == files.end()) {
    return {"", 0};
  }
  return it->second;
}

pair<int, int> TestCaseManager::getSiteWidth()
{
  // Site widths found empirically for the site-defined variants of the ICCAD
  // 2022 benchmarks. For plain benchmarks we fall back to 1 (a row width of 1
  // DBU per site means rowConstruction will use the scale_ as site pitch).
  map<TESTCASE, vector<int>> data;
  data[ICCAD2022_CASE2] = {21, 30};
  data[ICCAD2022_CASE2_H] = {30};
  data[ICCAD2022_CASE3] = {14};
  data[ICCAD2022_CASE3_H] = {11, 14};
  data[ICCAD2022_CASE4] = {17, 21};
  data[ICCAD2022_CASE4_H] = {18, 21};

  int width_top = 1;
  int width_bottom = 1;
  if (site_defined_) {
    auto it = data.find(testcase_);
    if (it != data.end()) {
      width_top = !it->second.empty() ? it->second.at(0) : 1;
      width_bottom = it->second.size() > 1 ? it->second.at(1) : width_top;
    }
  }

  width_top *= scale_;
  width_bottom *= scale_;
  return {width_top, width_bottom};
}

void TestCaseManager::ICCADContest(TESTCASE test_case, MultiDieManager* mdm)
{
  int year = 0;
  string input_file;
  std::tie(input_file, year) = fetchInputFileInfo(test_case);

  if (year == 2022) {
    ICCADContest2022(input_file, mdm);
  } else {
    assert(false && "Only ICCAD 2022 is supported in Stage 1.");
  }

  constructDB(mdm);
  is_iccad_parsed_ = true;
}

void TestCaseManager::ICCADContest2022(const string& input_file_name,
                                       MultiDieManager* mdm)
{
  ifstream input_file(input_file_name);
  if (!input_file.is_open()) {
    mdm->getLogger()->error(
        utl::MDM, 8, "Cannot open the input file: {}", input_file_name);
  }

  string info, name1, name2;
  int n1, n2, n3, n4, n5;

  // NumTechnologies <technologyCount>
  input_file >> info >> n1;
  assert(info == "NumTechnologies");
  mdm->setNumberOfDie(n1);

  for (int i = 0; i < mdm->getNumberOfDie(); ++i) {
    // Tech <techName> <libCellCount>
    input_file >> info >> name1 >> n1;
    assert(info == "Tech");

    TechInfo tech_info;
    tech_info.name = name1;
    tech_info.lib_cell_num = n1;

    for (int j = 0; j < tech_info.lib_cell_num; ++j) {
      // LibCell <libCellName> <libCellSizeX> <libCellSizeY> <pinCount>
      input_file >> info >> name1 >> n1 >> n2 >> n3;
      assert(info == "LibCell");

      LibCellInfo lib_cell_info;
      lib_cell_info.is_macro = false;
      lib_cell_info.name = name1;
      lib_cell_info.width = n1;
      lib_cell_info.height = n2;
      lib_cell_info.pin_number = n3;

      for (int k = 0; k < lib_cell_info.pin_number; ++k) {
        // Pin <pinName> <pinLocationX> <pinLocationY>
        input_file >> info >> name1 >> n1 >> n2;
        LibPinInfo lib_pin_info;
        lib_pin_info.pin_name = name1;
        lib_pin_info.pin_location_x = n1;
        lib_pin_info.pin_location_y = n2;
        lib_cell_info.lib_pin_infos.push_back(lib_pin_info);
      }
      tech_info.lib_cell_infos.push_back(lib_cell_info);
    }
    bench_information_.tech_infos.push_back(tech_info);
  }

  // DieSize <lowerLeftX> <lowerLeftY> <upperRightX> <upperRightY>
  input_file >> info >> n1 >> n2 >> n3 >> n4;
  assert(info == "DieSize");
  for (int i = 0; i < 2; ++i) {
    DieInfo die_info;
    die_info.lower_left_x = n1;
    die_info.lower_left_y = n2;
    die_info.upper_right_x = n3;
    die_info.upper_right_y = n4;
    bench_information_.die_infos.push_back(die_info);
  }

  // TopDieMaxUtil / BottomDieMaxUtil
  input_file >> info >> n1;
  assert(info == "TopDieMaxUtil");
  bench_information_.die_infos.at(TOP_DIE).max_util = n1;

  input_file >> info >> n1;
  assert(info == "BottomDieMaxUtil");
  bench_information_.die_infos.at(BOTTOM_DIE).max_util = n1;

  // TopDieRows / BottomDieRows <startX> <startY> <rowLength> <rowHeight>
  // <repeatCount>
  input_file >> info >> n1 >> n2 >> n3 >> n4 >> n5;
  assert(info == "TopDieRows");
  {
    auto& ri = bench_information_.die_infos.at(TOP_DIE).row_info;
    ri.start_x = n1;
    ri.start_y = n2;
    ri.row_width = n3;
    ri.row_height = n4;
    ri.repeat_count = n5;
  }

  input_file >> info >> n1 >> n2 >> n3 >> n4 >> n5;
  assert(info == "BottomDieRows");
  {
    auto& ri = bench_information_.die_infos.at(BOTTOM_DIE).row_info;
    ri.start_x = n1;
    ri.start_y = n2;
    ri.row_width = n3;
    ri.row_height = n4;
    ri.repeat_count = n5;
  }

  // TopDieTech / BottomDieTech
  input_file >> info >> name1;
  assert(info == "TopDieTech");
  bench_information_.die_infos.at(TOP_DIE).tech_name = name1;
  input_file >> info >> name1;
  assert(info == "BottomDieTech");
  bench_information_.die_infos.at(BOTTOM_DIE).tech_name = name1;

  // TerminalSize / TerminalSpacing
  input_file >> info >> n1 >> n2;
  assert(info == "TerminalSize");
  bench_information_.terminal_info.size_x = n1;
  bench_information_.terminal_info.size_y = n2;

  input_file >> info >> n1;
  assert(info == "TerminalSpacing");
  bench_information_.terminal_info.spacing_size = n1;

  bench_information_.terminal_info.cost = 0;

  // NumInstances <N>
  input_file >> info >> n1;
  assert(info == "NumInstances");
  instance_number_ = n1;
  for (int i = 0; i < instance_number_; ++i) {
    // Inst <instName> <libCellName>
    input_file >> info >> name1 >> name2;
    assert(info == "Inst");
    InstanceInfo ii;
    ii.inst_name = name1;
    ii.lib_cell_name = name2;
    bench_information_.instance_infos.push_back(ii);
  }

  // NumNets <N>
  input_file >> info >> n1;
  assert(info == "NumNets");
  net_number_ = n1;
  for (int i = 0; i < net_number_; ++i) {
    // Net <netName> <numPins>
    input_file >> info >> name1 >> n1;
    assert(info == "Net");
    NetInfo net_info;
    net_info.net_name = name1;
    net_info.pin_num = n1;
    for (int j = 0; j < net_info.pin_num; ++j) {
      // Pin <instName>/<libPinName>
      input_file >> info >> name1;
      assert(info == "Pin");
      const auto slash = name1.find('/');
      ConnectedPinInfo pin_info;
      pin_info.instance_name = name1.substr(0, slash);
      pin_info.lib_pin_name = name1.substr(slash + 1);
      net_info.connected_pin_infos.push_back(pin_info);
    }
    bench_information_.net_infos.push_back(net_info);
  }

  for (DieInfo& die_info : bench_information_.die_infos) {
    for (TechInfo& tech_info : bench_information_.tech_infos) {
      if (die_info.tech_name == tech_info.name) {
        die_info.tech_info = &tech_info;
      }
    }
  }

  row_infos_.first = bench_information_.die_infos.at(TOP_DIE).row_info;
  max_utils_.first = bench_information_.die_infos.at(TOP_DIE).max_util;
  row_infos_.second = bench_information_.die_infos.at(BOTTOM_DIE).row_info;
  max_utils_.second = bench_information_.die_infos.at(BOTTOM_DIE).max_util;

  input_file.close();
}

////////////////////////////////////////////////////////////////
// constructDB — build the tech/lib/master/inst/net tree for the ICCAD
// contest flow. The top hierarchical block is created first; child
// sub-blocks for the top/bottom dies come later in Stage 1.3 via
// MultiDieManager::splitInstances().
////////////////////////////////////////////////////////////////

void TestCaseManager::constructDB(MultiDieManager* mdm)
{
  dbDatabase* db = mdm->getDb();

  dbTech* tech_top_hier = dbTech::create(db, "TopHierTech");
  dbTech* tech_top = dbTech::create(db, "Die1Tech");
  dbTech* tech_bottom = dbTech::create(db, "Die2Tech");

  // ICCAD 2022 contest text has no LEF units; pick a DBU-per-micron that
  // matches the scale factor applied to every dimension in constructDB so
  // dbuToMicrons and tool-side micron printing stay finite.
  const int dbu = std::max(1, scale_);
  for (dbTech* t : {tech_top_hier, tech_top, tech_bottom}) {
    t->setLefUnits(dbu);
  }
  // The DBU-per-micron now lives on the dbDatabase itself (dbTech forwards
  // the query). Set it so dbBlock::dbuToMicrons stays finite.
  db->setDbuPerMicron(dbu);

  dbTechLayer* layer_top_hier = dbTechLayer::create(
      tech_top_hier, "layer", dbTechLayerType::MASTERSLICE);
  dbTechLayer* layer_top
      = dbTechLayer::create(tech_top, "layer", dbTechLayerType::MASTERSLICE);
  dbTechLayer* layer_bottom
      = dbTechLayer::create(tech_bottom, "layer", dbTechLayerType::MASTERSLICE);

  // Arbitrary M2 layers so dbTechLayer queries by name never fail.
  for (dbTech* t : {tech_top_hier, tech_top, tech_bottom}) {
    dbTechLayer* m2 = dbTechLayer::create(t, "M2", dbTechLayerType::ROUTING);
    m2->setWidth(10);
    m2->setSpacing(10);
    m2->setDirection(dbTechLayerDir::HORIZONTAL);
  }

  dbLib* lib_top_hier = dbLib::create(db, "TopHierLib", tech_top_hier);
  dbLib* lib_top = dbLib::create(db, "TopLib", tech_top);
  dbLib* lib_bottom = dbLib::create(db, "BottomLib", tech_bottom);
  // Recent ODB requires dbChip to own a dbTech. Child dbBlocks inherit that
  // tech from the parent chip, so we cannot express per-die LEF techs at the
  // block level anymore. Heterogeneous cell geometry is still captured
  // because TopLib / BottomLib live in their own dbTechs and carry distinct
  // masters — placement only consumes the masters' width/height/pins.
  dbChip* chip
      = dbChip::create(db, tech_top_hier, "chip", dbChip::ChipType::DIE);
  dbBlock* block_top_hier = dbBlock::create(chip, "topHierBlock");

  DieInfo* top_die = &bench_information_.die_infos.at(TOP_DIE);
  DieInfo* bottom_die = &bench_information_.die_infos.at(BOTTOM_DIE);

  // Die area (assumed identical for top and bottom).
  Point lower_left(top_die->lower_left_x * scale_,
                   top_die->lower_left_y * scale_);
  Point upper_right(top_die->upper_right_x * scale_,
                    top_die->upper_right_y * scale_);
  Rect die_area(lower_left, upper_right);
  block_top_hier->setDieArea(die_area);
  // ICCAD 2022 benchmarks have no separate core; the whole die is usable.
  block_top_hier->setCoreArea(die_area);

  // Top-hier rows. Child-block rows are built later by rowConstruction().
  dbSite* site = dbSite::create(lib_top_hier, "Site");
  const uint site_width = scale_;
  const int site_height = row_infos_.first.row_height * scale_;
  site->setWidth(site_width);
  site->setHeight(site_height);

  int num_of_sites = static_cast<int>(std::floor(
      static_cast<double>(row_infos_.first.row_width * scale_) / site_width));
  int num_of_rows = row_infos_.first.repeat_count;
  while (num_of_rows * site_height > top_die->upper_right_y * scale_) {
    num_of_rows--;
  }
  for (int i = 0; i < num_of_rows; ++i) {
    dbRow::create(block_top_hier,
                  ("row" + std::to_string(i)).c_str(),
                  site,
                  0,
                  i * site_height,
                  dbOrientType::MX,
                  dbRowDir::HORIZONTAL,
                  num_of_sites,
                  site_width);
  }

  // LibCells — emit a master per tech, per contest lib cell.
  assert(top_die->tech_info->lib_cell_num
         == bottom_die->tech_info->lib_cell_num);
  const int lib_cell_num = top_die->tech_info->lib_cell_num;
  for (int i = 0; i < lib_cell_num; ++i) {
    LibCellInfo* top_lc = &top_die->tech_info->lib_cell_infos.at(i);
    LibCellInfo* bot_lc = &bottom_die->tech_info->lib_cell_infos.at(i);
    assert(top_lc->name == bot_lc->name);
    const int width_top = top_lc->width * scale_;
    const int height_top = top_lc->height * scale_;
    const int width_bot = bot_lc->width * scale_;
    const int height_bot = bot_lc->height * scale_;
    assert(top_lc->pin_number == bot_lc->pin_number);
    const int pin_num = top_lc->pin_number;

    dbMaster* master_top_hier
        = dbMaster::create(lib_top_hier, top_lc->name.c_str());
    dbMaster* master_top = dbMaster::create(lib_top, top_lc->name.c_str());
    dbMaster* master_bottom
        = dbMaster::create(lib_bottom, bot_lc->name.c_str());

    master_top_hier->setWidth(width_top);
    master_top_hier->setHeight(height_top);
    master_top->setWidth(width_top);
    master_top->setHeight(height_top);
    master_bottom->setWidth(width_bot);
    master_bottom->setHeight(height_bot);

    master_top_hier->setType(dbMasterType::CORE);
    master_top_hier->setSite(site);
    master_top->setType(dbMasterType::CORE);
    master_bottom->setType(dbMasterType::CORE);

    for (int j = 0; j < pin_num; ++j) {
      LibPinInfo* top_pin = &top_lc->lib_pin_infos.at(j);
      LibPinInfo* bot_pin = &bot_lc->lib_pin_infos.at(j);
      assert(top_pin->pin_name == bot_pin->pin_name);
      const int px_top = top_pin->pin_location_x * scale_;
      const int py_top = top_pin->pin_location_y * scale_;
      const int px_bot = bot_pin->pin_location_x * scale_;
      const int py_bot = bot_pin->pin_location_y * scale_;

      // The contest format does not distinguish input/output pins: assume
      // the last pin is the output.
      const dbIoType io_type
          = (j == pin_num - 1) ? dbIoType::OUTPUT : dbIoType::INPUT;
      const dbSigType sig_type = dbSigType::SIGNAL;

      dbMTerm* mterm_top_hier = dbMTerm::create(
          master_top_hier, top_pin->pin_name.c_str(), io_type, sig_type);
      dbMTerm* mterm_top = dbMTerm::create(
          master_top, top_pin->pin_name.c_str(), io_type, sig_type);
      dbMTerm* mterm_bottom = dbMTerm::create(
          master_bottom, bot_pin->pin_name.c_str(), io_type, sig_type);

      dbMPin* mpin_top_hier = dbMPin::create(mterm_top_hier);
      dbMPin* mpin_top = dbMPin::create(mterm_top);
      dbMPin* mpin_bottom = dbMPin::create(mterm_bottom);

      dbBox::create(mpin_top_hier,
                    layer_top_hier,
                    px_top,
                    py_top,
                    px_top + scale_,
                    py_top + scale_);
      dbBox::create(mpin_top,
                    layer_top,
                    px_top,
                    py_top,
                    px_top + scale_,
                    py_top + scale_);
      dbBox::create(mpin_bottom,
                    layer_bottom,
                    px_bot,
                    py_bot,
                    px_bot + scale_,
                    py_bot + scale_);
    }
    master_top_hier->setFrozen();
    master_top->setFrozen();
    master_bottom->setFrozen();
  }

  // Instances — all start in the top hier; partitioning shifts them later.
  for (const auto& inst_info : bench_information_.instance_infos) {
    dbMaster* master = db->findLib("TopHierLib")
                           ->findMaster(inst_info.lib_cell_name.c_str());
    assert(master != nullptr);
    dbInst::create(block_top_hier, master, inst_info.inst_name.c_str());
  }

  // Nets.
  for (const auto& net_info : bench_information_.net_infos) {
    dbNet* net = dbNet::create(block_top_hier, net_info.net_name.c_str());
    for (int j = 0; j < net_info.pin_num; ++j) {
      const auto& pin_info = net_info.connected_pin_infos.at(j);
      dbInst* inst = block_top_hier->findInst(pin_info.instance_name.c_str());
      assert(inst != nullptr);
      dbITerm* iterm = inst->findITerm(pin_info.lib_pin_name.c_str());
      assert(iterm != nullptr);
      iterm->connect(net);
    }
  }

  // Hybrid-bond parameters are exported as chip-level properties so that
  // downstream MDM steps and evaluators can consume them.
  const int hbx = bench_information_.terminal_info.size_x * scale_;
  const int hby = bench_information_.terminal_info.size_y * scale_;
  const int hbs = bench_information_.terminal_info.spacing_size * scale_;
  const int hbc = bench_information_.terminal_info.cost;
  dbIntProperty::create(chip, "hybridBondX", hbx);
  dbIntProperty::create(chip, "hybridBondY", hby);
  dbIntProperty::create(chip, "hybridBondSpacing", hbs);
  dbIntProperty::create(chip, "hybridBondCost", hbc);
}

////////////////////////////////////////////////////////////////
// rowConstruction — build rows for the two child blocks. This must be
// called after MultiDieManager::makeSubBlocks() (Stage 1.3) has created
// the child dbBlocks.
////////////////////////////////////////////////////////////////

void TestCaseManager::rowConstruction()
{
  dbDatabase* db = mdm_->getDb();
  auto lib_iter = db->getLibs().begin();
  auto block_iter = db->getChip()->getBlock()->getChildren().begin();
  std::advance(lib_iter, 1);  // skip TopHierLib

  for (int i = 0; i < 2; ++i) {
    dbLib* lib = *lib_iter;
    dbBlock* block = *block_iter;

    dbSite* site;
    uint site_width;
    int site_height, num_of_sites, num_of_rows;
    if (i == 0) {
      site = dbSite::create(lib, "TopSite");
      site_width = getSiteWidth().first * scale_;
      site_height = row_infos_.first.row_height * scale_;
      num_of_sites = static_cast<int>(
          std::floor(static_cast<double>(row_infos_.first.row_width * scale_)
                     / site_width));
      num_of_rows = row_infos_.first.repeat_count;
    } else {
      site = dbSite::create(lib, "BottomSite");
      site_width = getSiteWidth().second * scale_;
      site_height = row_infos_.second.row_height * scale_;
      num_of_sites = static_cast<int>(
          std::floor(static_cast<double>(row_infos_.second.row_width * scale_)
                     / site_width));
      num_of_rows = row_infos_.second.repeat_count;
    }
    site->setWidth(site_width);
    site->setHeight(site_height);

    for (int j = 0; j < num_of_rows; ++j) {
      dbRow::create(block,
                    ("row" + std::to_string(j)).c_str(),
                    site,
                    0,
                    j * site_height,
                    dbOrientType::MX,
                    dbRowDir::HORIZONTAL,
                    num_of_sites,
                    site_width);
    }

    std::advance(lib_iter, 1);
    std::advance(block_iter, 1);
  }
}

void TestCaseManager::parseICCADOutput(const string& file_name,
                                       const char* which_die_char)
{
  ifstream input_file(file_name);
  string which_die{which_die_char};
  if (!input_file.is_open()) {
    mdm_->getLogger()->warn(
        utl::MDM, 11, "Cannot open the output file: {}", file_name);
    return;
  }
  iccad_output_parser_.parseOutput(input_file);
  if (which_die == "top") {
    iccad_output_parser_.applyCoordinates(
        *mdm_->getDb()->getChip()->getBlock()->getChildren().begin());
  } else if (which_die == "bottom") {
    auto it = mdm_->getDb()->getChip()->getBlock()->getChildren().begin();
    std::advance(it, 1);
    iccad_output_parser_.applyCoordinates(*it);
  } else {
    iccad_output_parser_.applyCoordinates();
  }
  iccad_output_parser_.makePartitionFile(file_name + ".par");
  input_file.close();
}

////////////////////////////////////////////////////////////////
// ICCADOutputParser
////////////////////////////////////////////////////////////////

void ICCADOutputParser::parseOutput(ifstream& output_file)
{
  string info, name;
  int n1;
  // TopDiePlacement <instance #>
  output_file >> info >> n1;
  assert(info == "TopDiePlacement");
  for (int i = 0; i < n1; ++i) {
    int x, y;
    output_file >> info >> name >> x >> y;
    assert(info == "Inst");
    inst_list_.push_back(name);
    inst_partition_map_[name] = TOP;
    inst_coordinate_map_[name] = {x, y};
  }
  // BottomDiePlacement <instance #>
  output_file >> info >> n1;
  assert(info == "BottomDiePlacement");
  for (int i = 0; i < n1; ++i) {
    int x, y;
    output_file >> info >> name >> x >> y;
    assert(info == "Inst");
    inst_list_.push_back(name);
    inst_partition_map_[name] = BOTTOM;
    inst_coordinate_map_[name] = {x, y};
  }
  // NumTerminals <terminal #>
  output_file >> info >> n1;
  assert(info == "NumTerminals");
  for (int i = 0; i < n1; ++i) {
    int x, y;
    output_file >> info >> name >> x >> y;
    assert(info == "Terminal");
    terminal_list_.push_back(name);
    terminal_coordinate_map_[name] = {x, y};
  }
  parsed_ = true;
}

void ICCADOutputParser::applyCoordinates(odb::dbBlock* target_block)
{
  vector<dbBlock*> block_set;
  if (target_block == nullptr) {
    block_set.push_back(db_->getChip()->getBlock());
    for (auto block : db_->getChip()->getBlock()->getChildren()) {
      block_set.push_back(block);
    }
  } else {
    block_set.push_back(target_block);
  }
  for (auto block : block_set) {
    for (auto inst : block->getInsts()) {
      const string name = inst->getName();
      auto it = inst_coordinate_map_.find(name);
      if (it == inst_coordinate_map_.end()) {
        continue;
      }
      inst->setLocation(it->second.first * scale_, it->second.second * scale_);
      inst->setPlacementStatus(dbPlacementStatus::PLACED);
    }
  }
}

void ICCADOutputParser::makePartitionFile(const string& file_name)
{
  if (!parsed_) {
    return;
  }
  std::ofstream out_file(file_name);
  if (!out_file.is_open()) {
    return;
  }
  for (const auto& inst_name : inst_list_) {
    out_file << inst_name << "  " << inst_partition_map_[inst_name] << "\n";
  }
}

}  // namespace mdm
