// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

%module mdm

%{
#include "mdm/MultiDieManager.h"
#include "ord/OpenRoad.hh"

static mdm::MultiDieManager*
getMultiDieManager()
{
  return ord::OpenRoad::openRoad()->getMultiDieManager();
}
%}

%inline %{

void
set_3D_IC(int number_of_die)
{
  getMultiDieManager()->set3DIC(number_of_die);
}

%} // inline
