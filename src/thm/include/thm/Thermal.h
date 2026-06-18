// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <string>

#include "odb/db.h"
#include "utl/Logger.h"

namespace thm {

// Adapter between OpenROAD (DB, logger, Tcl) and the pure thm_core engine.
// This is the only layer allowed to see OpenROAD types.
class Thermal
{
 public:
  Thermal(odb::dbDatabase* db, utl::Logger* logger);

  // Single-stack analysis: build a SteadyProblem from the odb 3D stack if one is
  // present (else a synthetic fallback), solve steady + transient (T1-T3).
  void analyzeThermal();

  // T4a: synthetic N-die stack sweep over `dies` (e.g. "12 16 20") using TSV /
  // microbump effective vertical conductivity, and microbump-vs-hybrid bonding.
  // Reproduces qualitative trends only (the stack is synthetic).
  void sweepThermal(const std::string& dies,
                    double tsv_diameter_um,
                    double tsv_pitch_um,
                    double bump_diameter_um,
                    double bump_pitch_um,
                    double hybrid_cu_coverage);

 private:
  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
};

}  // namespace thm
