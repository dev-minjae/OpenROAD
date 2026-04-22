// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

#include <memory>
#include <string>

namespace odb {
class dbDatabase;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace gpl {
class Replace;
}  // namespace gpl

namespace mdm {

class TestCaseManager;

class MultiDieManager
{
 public:
  MultiDieManager(odb::dbDatabase* db,
                  utl::Logger* logger,
                  gpl::Replace* replace);
  ~MultiDieManager();

  odb::dbDatabase* getDb() const { return db_; }
  utl::Logger* getLogger() const { return logger_; }
  gpl::Replace* getReplace() const { return replace_; }

  // 3D IC configuration.
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

 private:
  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  gpl::Replace* replace_ = nullptr;

  int number_of_die_ = 0;
  float shrink_area_ratio_ = 0.0f;

  std::string partition_file_;

  std::unique_ptr<TestCaseManager> test_case_manager_;
};

}  // namespace mdm
