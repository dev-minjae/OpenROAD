// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

%{
#include "ord/OpenRoad.hh"
#include "thm/Thermal.h"
%}

%include "../../Exception.i"

%inline %{

void
analyze_thermal_cmd()
{
  auto* thermal = ord::OpenRoad::openRoad()->getThermal();
  thermal->analyzeThermal();
}

void
sweep_thermal_cmd(const char* dies,
                  double tsv_diameter_um,
                  double tsv_pitch_um,
                  double bump_diameter_um,
                  double bump_pitch_um,
                  double hybrid_cu_coverage)
{
  auto* thermal = ord::OpenRoad::openRoad()->getThermal();
  thermal->sweepThermal(dies,
                        tsv_diameter_um,
                        tsv_pitch_um,
                        bump_diameter_um,
                        bump_pitch_um,
                        hybrid_cu_coverage);
}

%}  // inline
