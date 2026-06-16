// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "thm/Thermal.h"

#include "thm_core/ThermalCore.h"

namespace thm {

Thermal::Thermal(odb::dbDatabase* db, utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

void Thermal::analyzeThermal()
{
  // T0 sanity command: touch the DB seam (read-only) and the isolated core lib,
  // then log one INFO line. No thermal math here - that is T1.
  odb::dbChip* chip = db_->getChip();
  const char* design = (chip != nullptr) ? "loaded" : "none";

  logger_->info(utl::THM,
                1,
                "thermal module alive (core {}); design: {}",
                core::version(),
                design);
}

}  // namespace thm
