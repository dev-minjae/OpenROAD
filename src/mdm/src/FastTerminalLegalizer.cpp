// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#include "FastTerminalLegalizer.h"

#include "utl/Logger.h"

namespace mdm {

FastTerminalLegalizer::FastTerminalLegalizer(odb::dbBlock* parent_block,
                                             int spacing_C,
                                             utl::Logger* logger)
    : parent_block_(parent_block), spacing_C_(spacing_C), logger_(logger)
{
}

int64_t FastTerminalLegalizer::evaluateMove(odb::dbInst* /*cell*/)
{
  // Phase 4.1 stub. Phase 4.2.d implements C×C grid + BFS.
  return 0;
}

void FastTerminalLegalizer::commitLastMove()
{
  // Phase 4.1 stub.
}

}  // namespace mdm
