// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "odb/db.h"

namespace mdm {
class MultiDieManager;

class ICCADOutputParser
{
  enum DIE
  {
    TOP,
    BOTTOM
  };

 public:
  void setDb(odb::dbDatabase* db) { db_ = db; }
  void parseOutput(std::ifstream& output_file);
  void applyCoordinates(odb::dbBlock* target_block = nullptr);
  void makePartitionFile(const std::string& file_name);
  void setScale(int scale) { scale_ = scale; }

 private:
  std::pair<int, int> getTerminalCoordinate(const std::string& terminal_name)
  {
    return terminal_coordinate_map_[terminal_name];
  }

  std::pair<int, int> getCellCoordinate(const std::string& inst_name)
  {
    return inst_coordinate_map_[inst_name];
  }

  bool parsed_ = false;
  std::vector<std::string> inst_list_;
  std::vector<std::string> terminal_list_;
  std::unordered_map<std::string, DIE> inst_partition_map_;
  std::unordered_map<std::string, std::pair<int, int>> inst_coordinate_map_;
  std::unordered_map<std::string, std::pair<int, int>> terminal_coordinate_map_;
  odb::dbDatabase* db_ = nullptr;
  int scale_ = 1;
};

class TestCaseManager
{
 public:
  struct LibPinInfo
  {
    std::string pin_name;
    int pin_location_x;
    int pin_location_y;
  };

  struct LibCellInfo
  {
    std::string name;
    int width;
    int height;
    int pin_number;
    bool is_macro;
    std::vector<LibPinInfo> lib_pin_infos;
  };

  struct TechInfo
  {
    std::string name;
    int lib_cell_num;
    std::vector<LibCellInfo> lib_cell_infos;
  };

  struct RowInfo
  {
    int start_x = 0;
    int start_y = 0;
    int row_width = 0;
    int row_height = 0;
    int repeat_count = 0;
  };

  struct DieInfo
  {
    std::string tech_name;
    int lower_left_x = 0;
    int lower_left_y = 0;
    int upper_right_x = 0;
    int upper_right_y = 0;
    int max_util = 0;
    RowInfo row_info;
    TechInfo* tech_info = nullptr;
  };

  struct TerminalInfo
  {
    int size_x = 0;
    int size_y = 0;
    int spacing_size = 0;
    int cost = 0;
  };

  struct InstanceInfo
  {
    std::string inst_name;
    std::string lib_cell_name;
  };

  struct ConnectedPinInfo
  {
    std::string instance_name;
    std::string lib_pin_name;
  };

  struct NetInfo
  {
    std::string net_name;
    int pin_num;
    std::vector<ConnectedPinInfo> connected_pin_infos;
  };

  struct BenchInformation
  {
    std::vector<TechInfo> tech_infos;
    std::vector<InstanceInfo> instance_infos;
    std::vector<NetInfo> net_infos;
    std::vector<DieInfo> die_infos;
    TerminalInfo terminal_info;
  };

  enum DIE_ID
  {
    TOP_DIE,
    BOTTOM_DIE
  };

  enum TESTCASE
  {
    ICCAD2022_CASE1,
    ICCAD2022_CASE2,
    ICCAD2022_CASE3,
    ICCAD2022_CASE4,
    ICCAD2022_CASE2_H,
    ICCAD2022_CASE3_H,
    ICCAD2022_CASE4_H,
    NA,
  };

  void setMDM(MultiDieManager* mdm);
  void ICCADContest(TESTCASE test_case, MultiDieManager* mdm_ptr);

  void parseICCADOutput(const std::string& file_name,
                        const char* which_die_char = "");
  void setScale(int scale);
  int getScale() const { return scale_; }
  bool isICCADParsed() const { return is_iccad_parsed_; }
  void rowConstruction();
  void setBenchmarkDir(const std::string& dir) { benchmark_dir_ = dir; }
  const std::string& getBenchmarkDir() const { return benchmark_dir_; }
  void setSiteDefined(bool site_defined);
  std::pair<int, int> getSiteWidth();

  // Mapping from the Tcl-friendly test-case string to the enum.
  static TESTCASE lookupTestCase(const std::string& name);

  // (top_max_util, bottom_max_util) in percent, populated by ICCAD 2022
  // parser. {0, 0} if no ICCAD case parsed yet. Phase 4.2 reads these
  // for the GlobalTierOptimizer knapsack constraint.
  std::pair<int, int> getMaxUtils() const { return max_utils_; }

 private:
  void ICCADContest2022(const std::string& input_file_name,
                        MultiDieManager* mdm);
  void constructDB(MultiDieManager* mdm);
  std::pair<std::string, int> fetchInputFileInfo(TESTCASE test_case);

  ICCADOutputParser iccad_output_parser_;
  bool is_iccad_parsed_ = false;
  BenchInformation bench_information_;
  int instance_number_ = 0;
  int net_number_ = 0;
  std::pair<RowInfo, RowInfo> row_infos_;
  std::pair<int, int> max_utils_;
  MultiDieManager* mdm_ = nullptr;
  TESTCASE testcase_ = NA;
  int scale_ = 1;
  bool site_defined_ = false;
  std::string benchmark_dir_;
};

}  // namespace mdm
