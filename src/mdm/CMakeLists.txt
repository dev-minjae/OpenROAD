# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021-2026, The OpenROAD Authors

include("openroad")

find_package(LEMON NAMES LEMON lemon REQUIRED)

swig_lib(NAME      mdm
         NAMESPACE mdm
         I_FILE    src/MultiDieManager.i
         SCRIPTS   src/MultiDieManager.tcl
)

target_sources(mdm
  PRIVATE
    src/CellsLegalizer.cpp
    src/MakeMultiDieManager.cpp
    src/MultiDieManager.cpp
    src/SemiLegalizer.cpp
    src/SwitchInstHelper.cpp
    src/TerminalLegalizer.cpp
    src/TestCaseManager.cpp
)

messages(TARGET mdm)

target_include_directories(mdm
  PUBLIC
    include
  PRIVATE
    src
    ${LEMON_INCLUDE_DIRS}
)

target_link_libraries(mdm
  PRIVATE
    utl_lib
    odb
    gpl_lib
    dpl_lib
)
