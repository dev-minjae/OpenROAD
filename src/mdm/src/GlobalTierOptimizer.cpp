// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "GlobalTierOptimizer.h"

#include "utl/Logger.h"

namespace mdm {

GlobalTierOptimizer::GlobalTierOptimizer(odb::dbDatabase* db,
                                         utl::Logger* logger)
    : db_(db), logger_(logger)
{
}

std::vector<odb::dbInst*> GlobalTierOptimizer::run(
    odb::dbBlock* /*from_block*/,
    odb::dbBlock* /*to_block*/,
    const TierOptParams& /*params*/)
{
  logger_->info(utl::MDM,
                300,
                "GlobalTierOptimizer: skeleton stub. Returning empty "
                "partition delta. Implemented in Phase 4.2.");
  return {};
}

}  // namespace mdm
