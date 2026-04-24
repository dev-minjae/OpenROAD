// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// Hybrid-bond terminal legalizer for ICCAD 2022 Problem B output.
//
// Solves the terminal-placement subproblem of iPL-3D (ICCAD 2023) §IV.E:
//   min_{x_t, y_t}  Σ_j HPWL(e_j⁻ ∪ {t_j}) + HPWL(e_j⁺ ∪ {t_j})
//   s.t.            min(|x_ti - x_tj|, |y_ti - y_tj|) ≥ C  for all i ≠ j
//
// Each terminal carries a Terminal Optimal Region (TOR) — the rectangle
// (paper Definition 2) inside which the combined top/bottom HPWL is
// minimized, not a single point. The legalization cost is the Manhattan
// distance from the assigned grid-cell centre to the TOR rectangle (zero
// when inside). Grid spacing is C on each axis.
class TerminalLegalizer
{
 public:
  struct Config
  {
    int origin_x = 0;  // grid (0,0) centre X in DBU
    int origin_y = 0;  // grid (0,0) centre Y in DBU
    int step_x = 1;    // grid pitch X = terminal_size + spacing
    int step_y = 1;    // grid pitch Y
    int grid_w = 1;    // number of columns
    int grid_h = 1;    // number of rows

    // Phase 1b: when true, solve the terminal-to-grid assignment as a
    // weighted bipartite matching (min-cost flow via LEMON network
    // simplex, iPL-3D paper §IV.E). Otherwise fall back to the greedy
    // sort + spiral + pair-swap path.
    bool use_bipartite_matching = true;
    // Half-width (in grid cells) of the candidate window around each
    // terminal's TOR centre. r=2 gives a 5x5 = 25 candidate set,
    // matching paper Table III δ=25. The legalizer grows r up to
    // max_candidate_radius if the matching is infeasible.
    int candidate_radius = 2;
    int max_candidate_radius = 16;
  };

  struct Terminal
  {
    std::string name;
    // Raw TOR rectangle (paper Definition 2) in DBU.
    int tor_xlo = 0;
    int tor_xhi = 0;
    int tor_ylo = 0;
    int tor_yhi = 0;
    // Assigned grid index (filled by legalize()).
    int gi = 0;
    int gj = 0;
  };

  TerminalLegalizer(utl::Logger* logger, const Config& cfg);

  // Register a terminal with its TOR bounding rectangle. Must be called
  // before legalize().
  void addTerminal(std::string name,
                   int tor_xlo,
                   int tor_ylo,
                   int tor_xhi,
                   int tor_yhi);

  // Assign a distinct grid cell to every registered terminal.
  // Phase 1a: greedy sort-by-cost + Chebyshev-ring spiral + pair-swap
  // (behaviour-preserving port of the previous inline implementation,
  // but with the TOR region as the cost reference instead of a point).
  void legalize();

  const std::vector<Terminal>& terminals() const { return terminals_; }

  // Grid-cell centre in DBU.
  int gridCenterX(int gi) const { return cfg_.origin_x + gi * cfg_.step_x; }
  int gridCenterY(int gj) const { return cfg_.origin_y + gj * cfg_.step_y; }

 private:
  // Manhattan distance from a point to the TOR rectangle. Zero when the
  // point is inside. This is the exact ΔHPWL/2 for moving the terminal
  // onto (gx, gy): both the top and bottom sub-nets stretch by the same
  // amount per unit of displacement outside the region.
  int64_t costToTor(int gx, int gy, const Terminal& t) const;
  int64_t costOnGrid(const Terminal& t, int gi, int gj) const;

  // Nearest grid index to the TOR centre (used as the "raw" assignment
  // before collisions are resolved).
  int torCenterGi(const Terminal& t) const;
  int torCenterGj(const Terminal& t) const;

  // Paper §IV.E bipartite matching. Returns true iff every terminal was
  // matched; on false, all assignments are left in their initial (TOR
  // centre) state so the caller can fall back to the greedy path.
  bool legalizeByMatching();
  // Greedy port of the previous inline implementation. Also doubles as
  // the fallback when matching is infeasible.
  void legalizeGreedy();
  // Pair-swap refinement. Safe on any valid assignment (no-ops if already
  // locally optimal) and used after matching or greedy placement.
  void runPairSwap();

  utl::Logger* logger_ = nullptr;
  Config cfg_;
  std::vector<Terminal> terminals_;
};

}  // namespace mdm
