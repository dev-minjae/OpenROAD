// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

// HPWL (half-perimeter wirelength) OpenMP backend.
//
// Compiled and linked when ENABLE_GPU=OFF (the default); the Kokkos
// equivalent in gpu/hpwl.cpp is linked otherwise. Exactly one translation
// unit defines NesterovBaseCommon::getHpwl() per build (CMake-enforced ODR).

#include <cassert>
#include <cstdint>

#include "nesterovBase.h"
#include "omp.h"

namespace gpl {

int64_t NesterovBaseCommon::getHpwl()
{
  assert(omp_get_thread_num() == 0);
  int64_t hpwl = 0;
#pragma omp parallel for num_threads(num_threads_) reduction(+ : hpwl)
  for (auto gNet = gNetStor_.begin(); gNet < gNetStor_.end(); ++gNet) {
    // old-style loop for old OpenMP
    gNet->updateBox();
    hpwl += gNet->getHpwl();
  }
  return hpwl;
}

}  // namespace gpl
