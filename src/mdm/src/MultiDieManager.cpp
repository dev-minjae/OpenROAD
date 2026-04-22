// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "mdm/MultiDieManager.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "TestCaseManager.h"
#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

MultiDieManager::MultiDieManager(odb::dbDatabase* db,
                                 utl::Logger* logger,
                                 gpl::Replace* replace)
    : db_(db),
      logger_(logger),
      replace_(replace),
      test_case_manager_(std::make_unique<TestCaseManager>())
{
  test_case_manager_->setMDM(this);
}

MultiDieManager::~MultiDieManager() = default;

void MultiDieManager::set3DIC(int number_of_die, float area_ratio)
{
  number_of_die_ = number_of_die;
  shrink_area_ratio_ = area_ratio;
  logger_->info(utl::MDM, 1, "Set number of die to {}", number_of_die_);
  // Stage 1.3 will add splitInstances() and related logic here.
}

void MultiDieManager::setICCADScale(int scale)
{
  test_case_manager_->setScale(scale);
}

void MultiDieManager::readICCAD2022(const std::string& case_file)
{
  // Use the case file's directory as the benchmark lookup root so that
  // TestCaseManager can resolve aliases like "2022-test2" back to a real
  // file name (case2.txt). If the user passes a direct file path we still
  // honour it via the lookupTestCase fallback below.
  const std::filesystem::path p(case_file);
  const std::string stem = p.stem().string();  // case2, case4_hidden, etc.

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

  // Point the parser at the directory containing the benchmark so
  // fetchInputFileInfo can find the file (it uses case<N>.txt naming).
  test_case_manager_->setBenchmarkDir(p.parent_path().string());
  test_case_manager_->ICCADContest(test_case, this);
  logger_->info(utl::MDM, 10, "Successfully parsed ICCAD 2022: {}", case_file);
}

void MultiDieManager::writeICCAD2022Output(const std::string& file_name)
{
  std::ofstream out(file_name);
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

  // Per-die instance coordinates. If child dies haven't been created yet
  // (e.g. placement on the top hier only), fall back to the top hier block.
  auto emit_die = [&](const std::string& header, odb::dbBlock* block) {
    std::vector<odb::dbInst*> insts;
    if (block) {
      for (auto inst : block->getInsts()) {
        insts.push_back(inst);
      }
    }
    out << header << " " << insts.size() << "\n";
    for (auto inst : insts) {
      odb::Point loc = inst->getLocation();
      out << "Inst " << inst->getName() << " " << loc.getX() / scale << " "
          << loc.getY() / scale << "\n";
    }
  };

  if (top_die == nullptr) {
    // Fallback: everything is still on the top hier. Report as TopDie and
    // leave BottomDie empty (evaluators will accept but the score is poor).
    emit_die("TopDiePlacement", top_block);
    out << "BottomDiePlacement 0\n";
  } else {
    emit_die("TopDiePlacement", top_die);
    emit_die("BottomDiePlacement", bottom_die);
  }

  // Terminals (hybrid bonds). Until makeInterconnections() lands in
  // Stage 1.3 there are no terminals to emit, so we write zero.
  out << "NumTerminals 0\n";
}

void MultiDieManager::parseICCADOutput(const std::string& file_name,
                                       const char* which_die)
{
  test_case_manager_->parseICCADOutput(file_name, which_die);
}

}  // namespace mdm
