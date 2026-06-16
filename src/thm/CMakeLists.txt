# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors
#
# thm - 3D-IC thermal analysis module (therm3d).
#
# Two-layer design:
#   core/    pure numerical engine, NO OpenROAD dependencies (target thm_core).
#   <here>   adapter: the only OpenROAD touchpoint (Tcl / init / logging / DB read).

include("openroad")

# Pure engine first. Built as an isolated static lib that links nothing from
# OpenROAD; see core/CMakeLists.txt for the rationale and the hard boundary.
add_subdirectory(core)

swig_lib(NAME      thm
         NAMESPACE thm
         I_FILE    src/thermal.i
         SCRIPTS   src/thermal.tcl
)

target_sources(thm
  PRIVATE
    src/Thermal.cpp
    src/MakeThermal.cpp
)

target_include_directories(thm
  PUBLIC
    include
)

target_link_libraries(thm
  PRIVATE
    odb
    utl_lib
    thm_core
)

messages(
  TARGET thm
)
