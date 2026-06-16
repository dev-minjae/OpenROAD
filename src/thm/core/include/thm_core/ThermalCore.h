// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <string>

// Pure numerical thermal engine. This header and its implementation must NOT
// include any OpenROAD header (odb / utl / gui / ord). All coupling to OpenROAD
// lives in the adapter layer (src/thm/src). T0: placeholder only - the FDM RC
// solver (C*dT/dt + G*T = P) lands in T1.
namespace thm::core {

// Build/identity banner for the engine. Lets the adapter prove the isolated
// static lib is linked, without any solver existing yet.
std::string version();

}  // namespace thm::core
