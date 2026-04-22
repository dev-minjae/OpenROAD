// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2021-2026, The OpenROAD Authors

#include "mdm/MultiDieManager.h"

#include "odb/db.h"
#include "utl/Logger.h"

namespace mdm {

MultiDieManager::MultiDieManager(odb::dbDatabase* db,
                                 utl::Logger* logger,
                                 gpl::Replace* replace)
    : db_(db), logger_(logger), replace_(replace)
{
}

MultiDieManager::~MultiDieManager() = default;

void MultiDieManager::set3DIC(int number_of_die, float area_ratio)
{
  number_of_die_ = number_of_die;
  shrink_area_ratio_ = area_ratio;
  // Stage 1.3 will implement splitInstances() and related logic.
}

}  // namespace mdm
