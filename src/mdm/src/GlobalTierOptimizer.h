// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace odb {
class dbBlock;
class dbDatabase;
class dbInst;
class dbLib;
class dbMaster;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// iPL-3D paper §IV.B parameters (Table III defaults).
struct TierOptParams
{
  // Paper Table III defaults — all in normalized HPWL unit (μm). Surrogate
  // converts ΔWL and Δd from raw dbu to μm using `dbu_per_um` so these
  // constants apply directly. Caller may override.
  double rho = 500.0;     // terminal cost (paper Table III)
  double alpha = 100.0;   // overflow cost
  double beta = 0.5;      // overlap cost (Phase 4.2 sets β·Δo = 0)
  double gamma = 0.0;     // 0 normally, 1e4 for high-density relief
  // dbu/μm conversion. Set by caller from ICCAD scale (typically 2000).
  // 1 means the surrogate runs in raw dbu (legacy behavior).
  int dbu_per_um = 1;
  double B_factor = 1.0;   // knapsack: B = B_factor * u_t * A
                           // (paper Table III uses 1.1 assuming a follow-up
                           // detailed tier opt tightens; we run alone, so
                           // 1.0 keeps the partition strictly under cap)
  int max_outer_iter = 1;  // VNS outer iterations (Phase 4.3 uses >1)
  // Skip nets with more than this many pins from ΔWL/ΔTerm evaluation
  // and from the affected-cells re-priority list. Huge nets (clock,
  // power, broadcast) inflate runtime quadratically without changing
  // partition decisions in practice.
  int max_net_fanout = 100;
  // u_t and u_b in percent (0..100), populated by caller from ICCAD case
  // header. Reference values; the *binding* knapsack caps are below in dbu².
  int u_t_percent = 78;
  int u_b_percent = 78;
  // Knapsack capacities in dbu² area, mapped by the caller to from/to
  // sides. If both are 0, run() falls back to u_*_percent × core_area
  // assuming from = top, to = bottom.
  int64_t cap_from_dbu = 0;
  int64_t cap_to_dbu = 0;
};

// Algorithm 2 from paper §IV.B-2/3. Phase 4.2: full body — priority-queue
// based partition decision. Returns the cells that should flip from
// `from_block` to `to_block`. Caller applies the decision (Phase 4.7 path).
class GlobalTierOptimizer
{
 public:
  GlobalTierOptimizer(odb::dbDatabase* db, utl::Logger* logger);

  std::vector<odb::dbInst*> run(odb::dbBlock* from_block,
                                odb::dbBlock* to_block,
                                const TierOptParams& params,
                                odb::dbLib* to_lib = nullptr);

 private:
  // Algorithm 2 dynamic state: which cells have been accepted into the
  // move set so far, plus pre-computed knapsack cap and row height for
  // overflow normalization.
  struct Context
  {
    odb::dbBlock* from_block = nullptr;
    odb::dbBlock* to_block = nullptr;
    odb::dbLib* to_lib = nullptr;        // for target-tech cell area lookup
    std::unordered_set<odb::dbInst*> S;  // cells already accepted to move
    TierOptParams params;
    int64_t cap_t_dbu = 0;          // u_t * A on to-side
    int64_t cap_from_dbu = 0;       // u of from-side * A on from-side
    int64_t from_total_area = 0;    // fixed at run() entry (from-tech areas)
    int64_t to_existing_area = 0;   // fixed at run() entry (to-tech areas)
    int64_t s_total_area_to = 0;    // S cells under to-tech (incremental)
    int64_t s_total_area_from = 0;  // S cells under from-tech (incremental)
    int row_height = 1;             // h_r for d(S) normalization
    // Cache: master name → (from_area, to_area) in dbu²
    mutable std::unordered_map<std::string, std::pair<int64_t, int64_t>>
        master_area_cache;
  };

  int64_t cellAreaInFromTech(odb::dbInst* cell, const Context& ctx) const;
  int64_t cellAreaInToTech(odb::dbInst* cell, const Context& ctx) const;

  // ΔWL and Δ#Term contribution if `cell` flips from from_block to
  // to_block (with S already moved). Pure function w.r.t. ctx.
  struct MoveDelta
  {
    int64_t delta_wl = 0;
    int delta_term = 0;
  };
  MoveDelta evaluateMove(odb::dbInst* cell, const Context& ctx) const;

  // Eq 10a surrogate. Returns Δp = Δ(WL + ρ·#Term + α·d - γ·d_old).
  // β·Δo dropped in Phase 4.2 (overlap is small under flat init's
  // row-aligned cell layout; revisit if QoR demands it).
  double surrogateDelta(odb::dbInst* cell, const Context& ctx) const;

  // d(S) — single-bin overflow at to_block: max((used-cap)/h_r, 0).
  double overflow(const Context& ctx) const;

  // Algorithm 2 line 3: A_t(S_t ∪ S ∪ {cell}) ≤ B.
  bool fitsKnapsack(odb::dbInst* cell, const Context& ctx) const;

  static int64_t cellAreaDbu(odb::dbInst* cell);

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
};

}  // namespace mdm
