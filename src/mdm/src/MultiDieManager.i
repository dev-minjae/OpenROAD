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

%}  // inline
