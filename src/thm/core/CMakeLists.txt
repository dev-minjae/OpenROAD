# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors
#
# thm_core - pure numerical thermal engine.
#
# HARD BOUNDARY: this target MUST NOT depend on any OpenROAD module
# (no odb / utl / gui / ord includes or links). Its inputs and outputs are plain
# C++ structs/arrays, so the engine stays independently buildable and testable,
# GPU (Kokkos) work can drop in here, and it can later be extracted to the
# standalone therm3d repo unchanged. External math libraries (Eigen now, Kokkos
# at T5) are allowed; OpenROAD modules are not.

add_library(thm_core STATIC
  src/ThermalCore.cpp
  src/SteadySolver.cpp
  src/TransientSolver.cpp
)

target_include_directories(thm_core
  PUBLIC
    include
)

target_compile_features(thm_core PUBLIC cxx_std_17)

# Numerical backend for T1: Eigen (header-only). Linked PRIVATE so Eigen types
# never leak into thm_core's public API; swappable for a GPU backend at T5.
find_package(Eigen3 REQUIRED)
target_link_libraries(thm_core PRIVATE Eigen3::Eigen)

# Standalone correctness test. Links ONLY thm_core, so it builds and runs
# independently of OpenROAD's test framework / ENABLE_TESTS; run the binary
# directly (build/src/thm/core/thm_core_test).
add_executable(thm_core_test test/test_steady.cpp)
target_link_libraries(thm_core_test PRIVATE thm_core)
