// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

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

class MultiDieManager
{
 public:
  MultiDieManager(odb::dbDatabase* db,
                  utl::Logger* logger,
                  gpl::Replace* replace);
  ~MultiDieManager();

  odb::dbDatabase* getDb() const { return db_; }

  // Placeholder for Stage 1.2+ functionality. Intentionally empty in 1.1.
  void set3DIC(int number_of_die, float area_ratio = 0.5);

 private:
  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  gpl::Replace* replace_ = nullptr;

  int number_of_die_ = 0;
  float shrink_area_ratio_ = 0.0f;
};

}  // namespace mdm
