// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include "odb/db.h"
#include "utl/Logger.h"

namespace thm {

// Adapter between OpenROAD (DB, logger, Tcl) and the pure thm_core engine.
// This is the only layer allowed to see OpenROAD types.
class Thermal
{
 public:
  Thermal(odb::dbDatabase* db, utl::Logger* logger);

  // T0: no physics yet - proves the module is wired and the isolated core lib
  // links. Real analysis (grid build + solve) arrives in T1/T2.
  void analyzeThermal();

 private:
  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
};

}  // namespace thm
