// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#pragma once

#include "tcl.h"

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

class MultiDieManager;

void initMultiDieManager(Tcl_Interp* tcl_interp);

}  // namespace mdm
