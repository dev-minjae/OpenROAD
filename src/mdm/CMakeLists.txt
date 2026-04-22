# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021-2026, The OpenROAD Authors

include("openroad")

swig_lib(NAME      mdm
         NAMESPACE mdm
         I_FILE    src/MultiDieManager.i
         SCRIPTS   src/MultiDieManager.tcl
)

target_sources(mdm
  PRIVATE
    src/MakeMultiDieManager.cpp
    src/MultiDieManager.cpp
    src/SwitchInstHelper.cpp
    src/TestCaseManager.cpp
)

messages(TARGET mdm)

target_include_directories(mdm
  PUBLIC
    include
  PRIVATE
    src
)

target_link_libraries(mdm
  PRIVATE
    utl_lib
    odb
    gpl_lib
)
