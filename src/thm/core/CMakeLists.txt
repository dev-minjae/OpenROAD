# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors
#
# thm_core - pure numerical thermal engine.
#
# HARD BOUNDARY: this target MUST NOT depend on any OpenROAD module
# (no odb / utl / gui / ord includes or links). Its inputs and outputs are plain
# C++ structs/arrays, so the engine stays independently buildable and testable,
# GPU (Kokkos) work can drop in here, and it can later be extracted to the
# standalone therm3d repo unchanged. The boundary is enforced simply by linking
# nothing here.

add_library(thm_core STATIC
  src/ThermalCore.cpp
)

target_include_directories(thm_core
  PUBLIC
    include
)

target_compile_features(thm_core PUBLIC cxx_std_17)
