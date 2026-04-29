// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include "GlobalTierOptimizer.h"

namespace gpl {
class Replace;
}  // namespace gpl

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

class MultiDieManager;

// iPL-3D paper Algorithm 1: M-iteration alternation between
// SP-1 (tier optimization) and SP-2 (planar correcting). Phase 4.1:
// skeleton — run() loops M times calling stubs and logs each iteration.
struct BilevelParams
{
  int M = 4;                    // alternation iterations
  TierOptParams tier;           // forwarded to GlobalTierOptimizer
  double eps = 1e-3;            // convergence threshold (HPWL fraction)
  bool no_alternating = false;  // Table IV "w/o alternating" ablation
};

class BilevelCoordinator
{
 public:
  BilevelCoordinator(MultiDieManager* mdm,
                     gpl::Replace* replace,
                     utl::Logger* logger);

  void run(const BilevelParams& params);

 private:
  MultiDieManager* mdm_ = nullptr;
  gpl::Replace* replace_ = nullptr;
  utl::Logger* logger_ = nullptr;
};

}  // namespace mdm
