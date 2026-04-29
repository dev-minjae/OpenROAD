// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

%module mdm

%{
#include <string>

#include "mdm/MultiDieManager.h"
#include "ord/OpenRoad.hh"

static mdm::MultiDieManager*
getMultiDieManager()
{
  return ord::OpenRoad::openRoad()->getMultiDieManager();
}
%}

%include <std_string.i>

%inline %{

void
set_3D_IC(int number_of_die)
{
  getMultiDieManager()->set3DIC(number_of_die);
}

void
read_iccad2022(const std::string& case_file)
{
  getMultiDieManager()->readICCAD2022(case_file);
}

void
write_iccad2022_output(const std::string& file_name)
{
  getMultiDieManager()->writeICCAD2022Output(file_name);
}

void
parse_iccad2022_output(const std::string& file_name,
                      const std::string& which_die)
{
  getMultiDieManager()->parseICCADOutput(file_name, which_die.c_str());
}

void
set_iccad_scale(int scale)
{
  getMultiDieManager()->setICCADScale(scale);
}

void
set_mdm_partition_file(const std::string& path)
{
  getMultiDieManager()->setPartitionFile(path);
}

void
get_3d_hpwl(bool approximate = true)
{
  getMultiDieManager()->get3DHPWL(approximate);
}

void
multi_die_detail_placement(int max_displacement_x = 0,
                           int max_displacement_y = 0)
{
  getMultiDieManager()->multiDieDetailPlacement(max_displacement_x,
                                                max_displacement_y);
}

void
run_semi_legalizer(const std::string& target_die = "",
                   bool use_abacus = true,
                   bool use_cells_dynamic_row = true)
{
  getMultiDieManager()->runSemiLegalizer(
      target_die, use_abacus, use_cells_dynamic_row);
}

void
get_hpwl(const std::string& die_info = "")
{
  getMultiDieManager()->getHPWL(die_info.empty() ? nullptr : die_info.c_str());
}

void
export_coordinates(const std::string& file_name)
{
  getMultiDieManager()->exportCoordinates(file_name);
}

void
import_coordinates(const std::string& file_name)
{
  getMultiDieManager()->importCoordinates(file_name);
}

void
run_flattened_placement(double density = 1.0)
{
  getMultiDieManager()->runFlattenedPlacement(density);
}

void
run_global_tier_optimization(double rho = 500.0,
                             double alpha = 100.0,
                             double beta = 0.5,
                             double gamma = 0.0,
                             bool apply = false)
{
  getMultiDieManager()->runGlobalTierOptimization(
      rho, alpha, beta, gamma, apply);
}

void
run_3d_placement(int iterations = 4, bool no_alternating = false)
{
  getMultiDieManager()->run3DPlacement(iterations, no_alternating);
}

%}  // inline
