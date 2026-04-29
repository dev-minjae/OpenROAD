// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "BilevelCoordinator.h"

#include "mdm/MultiDieManager.h"
#include "utl/Logger.h"

namespace mdm {

BilevelCoordinator::BilevelCoordinator(MultiDieManager* mdm,
                                       gpl::Replace* replace,
                                       utl::Logger* logger)
    : mdm_(mdm), replace_(replace), logger_(logger)
{
}

void BilevelCoordinator::run(const BilevelParams& params)
{
  logger_->info(utl::MDM,
                301,
                "BilevelCoordinator: skeleton stub. M={}, no_alternating={}. "
                "Phase 4.6 implements Algorithm 1.",
                params.M,
                params.no_alternating);

  for (int k = 0; k < params.M; ++k) {
    logger_->info(utl::MDM, 302, "BilevelCoordinator: iter {} (stub)", k);
  }
}

}  // namespace mdm
