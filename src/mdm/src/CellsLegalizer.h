// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors

#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "odb/geom.h"

namespace odb {
class dbBlock;
class dbDatabase;
class dbInst;
class dbNet;
}  // namespace odb

namespace utl {
class Logger;
}  // namespace utl

namespace mdm {

// Phase 4.7 cross-die move parameters. Filled in when Phase 4.7 implements
// CellsLegalizer::tryMoveCellAcrossDies. Phase 4.1 plumbs the API only.
struct CrossDieMoveParams
{
  int max_displacement_x = 0;  // 0 = unbounded
  int max_displacement_y = 0;
  bool rollback_on_overlap = true;
  bool require_hpwl_decrease = true;
};

// Dynamic row-based legalizer (iPL-3D paper §IV.C.1, Fig. 5). The cluster
// container is a partial-ordered map keyed by the leftmost cell's centre x,
// which lets us insert cells in the middle of a row and merge with neighbours
// in either direction.
//
// Stage 3.3 lifts the legacy "rebuild-row-per-trial" driver: each row keeps
// a persistent cluster map, trials snapshot the affected row (std::map copy)
// and restore on revert, and `insertCell` returns the iterator to inst's
// containing cluster so the driver can predict inst's x without an extra
// scan. Cells still arrive in left-x ascending order in this stage, so the
// behaviour is mathematically equivalent to SemiLegalizer's Abacus path
// (q/e/w invariants are preserved across re-clustering). Subsequent stages
// will exploit the partial-ordered map by feeding cells in non-left-x order
// or by removing the tail-fast-path entirely.
class CellsLegalizer
{
 public:
  CellsLegalizer(odb::dbDatabase* db, utl::Logger* logger);

  // Input shape selector. The default abacus path was tuned for nearly-
  // row-aligned Xueyan-GP input; the tetris path handles free-form
  // Nesterov output (Phase 4.5).
  enum class Mode
  {
    ABACUS,  // legacy: cluster-based row-pack
    TETRIS,  // new: displacement-min row + slot search (advisor 2026-05-01)
  };

  // Legalize every (or one) child die under the top hier block.
  // target_die: "" (all), "top", "bottom".
  // skip_pair_swap: when true, skip the post-legalize pair-swap refinement.
  // mode: ABACUS (default, Xueyan-GP input) or TETRIS (free-form input).
  void run(const std::string& target_die,
           bool skip_pair_swap = false,
           Mode mode = Mode::ABACUS);

  // Phase 4.7 (skeleton in Phase 4.1): try moving `cell` from its current
  // die to the die identified by `new_die_id`. Returns true if applied
  // and accepted, false otherwise. Stub returns false unconditionally.
  bool tryMoveCellAcrossDies(odb::dbInst* cell,
                             int new_die_id,
                             const CrossDieMoveParams& params);

 private:
  struct Cluster
  {
    double xc = 0.0;  // weighted centre x (q/e), clamped to row bounds
    double w = 0.0;   // cumulative cluster width
    double q = 0.0;   // Σ e_i * (x_i − Σ_{j<i} w_j)  (Abacus invariant)
    double e
        = 0.0;  // Σ e_i (uniform 1.0 in this port — no fixed-cell weighting)
    std::vector<odb::dbInst*> cells;
  };
  using Row
      = std::map<int, Cluster>;  // key = leftmost cell's centre x at insert

  void legalizeBlock(odb::dbBlock* block);
  // Tetris row-pack for free-form input. For each cell sorted by left x,
  // tries the candidate rows in order of |row_y − orig_y| and accepts the
  // first row that has free width left. Within a row, cells are appended
  // left-to-right by orig_x order (each cell snaps to max(orig_x, prev
  // cell's right x)). This is a straight NTUplace3-style Tetris:
  // no clusters, no q/e/w, just per-row width tracking.
  void legalizeBlockTetris(odb::dbBlock* block);

  // Insert `inst` into the partial-ordered row map as a singleton cluster
  // keyed by its centre-x, then cascadeMerge to absorb any overlap with
  // neighbours in either direction. Returns the iterator to the cluster
  // that now contains `inst`. For left-x ascending input this collapses
  // to a left-merge into the existing tail and is mathematically
  // identical to SemiLegalizer's addCell + collapse path.
  Row::iterator insertCell(Row& row,
                           odb::dbInst* inst,
                           int row_xmin,
                           int row_xmax);

  // Fixed-point cascade: from `it`, alternately merge an overlapping right
  // neighbour into `it` or merge `it` into an overlapping left neighbour
  // (substituting `it` with the predecessor in that case). Iterates until
  // both sides are clear. Erasing one cluster per iteration guarantees
  // termination. Returns the (possibly substituted) iterator pointing to
  // the cluster that holds the cells originally inserted by the caller.
  Row::iterator cascadeMerge(Row& row,
                             Row::iterator it,
                             int row_xmin,
                             int row_xmax);

  // Predict inst's x in `row` after a successful insertCell, without
  // mutating any cell positions. Mirrors commitPlacement's left-to-right
  // packing inside `inst`'s cluster. `inst_cluster` must be the iterator
  // returned by insertCell.
  int predictX(Row::iterator inst_cluster, odb::dbInst* inst) const;

  // Recompute xc = q/e and clamp to [row_xmin, row_xmax - w].
  void recomputeCenter(Cluster& cluster, int row_xmin, int row_xmax) const;

  // Walk the row in key order, packing cells left-to-right inside their
  // cluster. Y is preserved (set when the caller committed each cell).
  void commitPlacement(Row& row);

  // Post-legalization HPWL refinement. Sweeps each row left-to-right
  // and tries swapping adjacent cells (within their tight pair span).
  // A swap is accepted if the total HPWL of the two cells' incident
  // nets goes down. Multiple passes until no swap is accepted (or the
  // pass cap kicks in). Cluster invariants are preserved because the
  // post-swap pair occupies the same combined slot.
  void pairSwap(odb::dbBlock* block);

  // 3D-HPWL contribution of all distinct nets touching either cell.
  // Non-intersected nets contribute their 2D bbox HPWL within `block`.
  // Intersected nets use the same TOR-adjusted formula as get3DHPWL:
  // both dies' bboxes are merged with the centre of their shared inner
  // box, and HPWL = box1_with_centre + box2_with_centre. The sibling
  // (other-die) bbox is read from sibling_bbox_cache_, which is built
  // by buildSiblingCache before the swap loop and stays valid because
  // only `block`'s own cells move during this pairSwap call.
  int64_t pairNetsHPWL(odb::dbInst* a, odb::dbInst* b) const;

  // Generalisation of pairNetsHPWL to N cells. Aggregates the distinct
  // nets touching any of `insts` and applies the same 3D-aware HPWL
  // formula. Used by the triple-cell rotation pass to score multi-cell
  // moves under one consistent cost model.
  int64_t groupNetsHPWL(std::initializer_list<odb::dbInst*> insts) const;

  // Build sibling_bbox_cache_ for every intersected net in `block`.
  // Sibling lookup walks the BTerm/iTerm/BTerm chain that get3DHPWL
  // uses to pair up the two dies' representations of the same logical
  // net. The cache stays valid until the caller next moves cells in a
  // sibling block, so we only need to (re)build it at the start of
  // each pairSwap call.
  void buildSiblingCache(odb::dbBlock* block);

  // Walk the BTerm chain to find `net`'s twin in the sibling die.
  // Returns nullptr if the net is not paired (e.g., a non-intersected
  // net or a malformed interconnect).
  static odb::dbNet* findSiblingNet(odb::dbNet* net);

  static int instWidth(odb::dbInst* inst);

  odb::dbDatabase* db_ = nullptr;
  utl::Logger* logger_ = nullptr;
  odb::dbBlock* target_block_ = nullptr;
  bool skip_pair_swap_ = false;
  std::unordered_map<odb::dbNet*, odb::Rect> sibling_bbox_cache_;
};

}  // namespace mdm
