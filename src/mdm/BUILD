# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//bazel:tcl_encode_or.bzl", "tcl_encode")
load("//bazel:tcl_wrap_cc.bzl", "tcl_wrap_cc")
load("//test:regression.bzl", "messages_txt")

package(
    default_visibility = ["//:__subpackages__"],
    features = ["layering_check"],
)

cc_library(
    name = "mdm",
    srcs = [
        "src/CellsLegalizer.cpp",
        "src/CellsLegalizer.h",
        "src/GlobalTierOptimizer.cpp",
        "src/GlobalTierOptimizer.h",
        "src/MakeMultiDieManager.cpp",
        "src/MultiDieManager.cpp",
        "src/SemiLegalizer.cpp",
        "src/SemiLegalizer.h",
        "src/SwitchInstHelper.cpp",
        "src/TerminalLegalizer.cpp",
        "src/TerminalLegalizer.h",
        "src/TestCaseManager.cpp",
        "src/TestCaseManager.h",
        ":swig",
        ":tcl",
    ],
    hdrs = [
        "include/mdm/MakeMultiDieManager.h",
        "include/mdm/MultiDieManager.h",
    ],
    includes = [
        "include",
    ],
    deps = [
        "//:ord",
        "//src/dpl",
        "//src/gpl",
        "//src/odb",
        "//src/utl",
        "@coin-or-lemon//:lemon",
        "@tcl_lang//:tcl",
    ],
)

tcl_encode(
    name = "tcl",
    srcs = [
        "src/MultiDieManager.tcl",
    ],
    char_array_name = "mdm_tcl_inits",
    namespace = "mdm",
    visibility = ["//visibility:public"],
)

tcl_wrap_cc(
    name = "swig",
    srcs = [
        "src/MultiDieManager.i",
        "//:error_swig",
    ],
    module = "mdm",
    namespace_prefix = "mdm",
    root_swig_src = "src/MultiDieManager.i",
    swig_includes = [
        "src/mdm/src",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/odb:swig",
    ],
)

messages_txt(
    visibility = ["//src/mdm/test:__pkg__"],
)
