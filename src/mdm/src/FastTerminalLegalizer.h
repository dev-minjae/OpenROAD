// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <cstdint>

namespace odb {
class dbBlock;
class dbInst;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// iPL-3D paper §IV.B-1: in-loop terminal placement that returns ΔWL
// quickly without hitting the heavyweight TerminalLegalizer. Phase 4.1:
// skeleton — evaluateMove() returns 0, commitLastMove() is a no-op.
// Phase 4.2 implements the C×C grid + BFS body.
class FastTerminalLegalizer
{
 public:
  FastTerminalLegalizer(odb::dbBlock* parent_block,
                        int spacing_C,
                        utl::Logger* logger);

  // Hypothetical move of `cell` from from_die to to_die: place a terminal
  // for each newly-crossing net via BFS on a C×C grid; return cumulative
  // ΔWL (positive = wirelength increase).
  int64_t evaluateMove(odb::dbInst* cell);

  // Persist the placements found for the last evaluateMove call.
  void commitLastMove();

 private:
  odb::dbBlock* parent_block_ = nullptr;
  int spacing_C_ = 0;
  utl::Logger* logger_ = nullptr;
};

}  // namespace mdm
