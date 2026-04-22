// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "mdm/MakeMultiDieManager.h"

#include "mdm/MultiDieManager.h"
#include "tcl.h"
#include "utl/decode.h"

extern "C" {
extern int Mdm_Init(Tcl_Interp* interp);
}

namespace mdm {

extern const char* mdm_tcl_inits[];

void initMultiDieManager(Tcl_Interp* tcl_interp)
{
  Mdm_Init(tcl_interp);
  utl::evalTclInit(tcl_interp, mdm::mdm_tcl_inits);
}

}  // namespace mdm
