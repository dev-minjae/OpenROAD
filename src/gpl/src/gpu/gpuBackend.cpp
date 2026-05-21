// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

// Shared GPU-backend helpers for the gpl GPU kernel series.
//
// Compiled only when ENABLE_GPU=ON. This is plain C++ (no device code): it
// only calls getenv and the Kokkos lifecycle API, so it does NOT need
// CUDA/HIP-language compilation.

#include "gpuBackend.h"

#include <Kokkos_Core.hpp>
#include <cstdlib>
#include <mutex>
#include <string>

namespace gpl {

namespace {

// Lower-case a copy of the string for case-insensitive comparison.
std::string toLower(const char* s)
{
  std::string out(s);
  for (char& c : out) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

}  // namespace

bool gpuEnabled()
{
  // Magic-static: the environment is read exactly once per process.
  static const bool enabled = [] {
    const char* env = std::getenv("ENABLE_GPU");
    if (env == nullptr) {
      // GPU is the default backend when compiled in.
      return true;
    }
    const std::string value = toLower(env);
    if (value.empty() || value == "0" || value == "off" || value == "false"
        || value == "no") {
      return false;
    }
    return true;
  }();
  return enabled;
}

// Lazy Kokkos lifecycle owned by gpl_lib so that the host application
// (the openroad binary, regression drivers, etc.) does not need to know
// Kokkos exists. The first GPU kernel call initializes Kokkos and registers
// an atexit handler that finalizes it once at process shutdown — this is
// the upstream-safe pattern for opt-in CUDA backends without disrupting
// OpenROAD's existing main(). std::call_once keeps the initialization
// safe if a future caller drops the master-thread invariant.
void ensureKokkosInitialized()
{
  static std::once_flag once;
  std::call_once(once, [] {
    if (Kokkos::is_initialized()) {
      return;
    }
    Kokkos::InitializationSettings settings;
    settings.set_disable_warnings(true);
    Kokkos::initialize(settings);
    std::atexit([] {
      if (Kokkos::is_initialized() && !Kokkos::is_finalized()) {
        Kokkos::finalize();
      }
    });
  });
}

}  // namespace gpl
