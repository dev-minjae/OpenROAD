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

%}  // inline
