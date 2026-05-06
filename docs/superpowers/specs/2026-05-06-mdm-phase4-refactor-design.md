# Phase 4 (mdm) Refactor — Design

**Date**: 2026-05-06
**Branch**: `feature/mdm/global-tier-opt`
**Scope baseline (HEAD)**: `df11a310a7`
**Author**: minjae

## 1. Goal

Refactor user-authored Phase 4 code in `src/mdm/` to:

1. Remove YAGNI-violating stubs that are not on the immediate execution path.
2. Expose hyperparameters of `run_planar_correcting` and `run_global_tier_optimization` as Tcl knobs so the next leverage point (case3 Planar Correcting Nesterov non-convergence fix) can proceed without C++ rebuilds.
3. Decompose the largest function (`MultiDieManager::runGlobalTierOptimization`, ~155 lines, 5 responsibilities) into named helpers.
4. Replace ad-hoc inline patterns (child-block iteration, idempotency markers, surrogate overflow recomputation) with focused helpers.
5. Audit logger IDs for collisions inside mdm.

Behavior on case2 e2e must remain bit-for-bit identical (numerical lock = 2,694,774 ± 0.5%).

## 2. Out of scope

- Stage 3.3 code (`CellsLegalizer`, `SemiLegalizer`, pair-swap, Tetris) — separate cycle.
- Other src/mdm/ files (`TestCaseManager`, `TerminalLegalizer`, `SwitchInstHelper`, Stage 1/2 portions of `MultiDieManager`).
- Logic changes — pure refactor.
- Phase 4.3 VNS, 4.4 Flattened Init, 4.6 Bilevel, 4.7 Cross-die move (handled in their own phase cycles).

## 3. Regression strategy

### 3.1 Smoke test (cheap, run after every commit)

`src/mdm/test/test_phase4_smoke.tcl` — replaces `test_phase41_skeleton.tcl`.

- Loads case2 (or a small variant), invokes the Phase 4 pipeline once, asserts:
  - `run_global_tier_optimization -apply` reports `applied > 0`, no floating nets after migration.
  - `run_planar_correcting -iterations 1` returns without throwing.
  - `snap_cells_to_rows` returns with snapped count printed.
  - Post-pipeline placement statuses are restored (no leaked FIRM from `ScopedFirmFreeze`).
- Target runtime: < 30 seconds.

### 3.2 Numerical lock (expensive, run after structural commits)

`.workspace/bench/tcl/regression_phase4_case2.tcl` + `.workspace/bench/regression_check.sh`.

- Reuses existing `phase45_e2e_case2.tcl` as baseline.
- Calls `evaluator_0525`, parses output HPWL, compares against `2,694,774 ± 0.5%`.
- Run after commits 1, 4, 5, 6, 7 (commits that touch behavior-adjacent code).

## 4. Refactor items

| ID | Item | Files | Risk |
|----|------|-------|------|
| g | Delete 4 stubs (`BilevelCoordinator.{h,cpp}`, `FastTerminalLegalizer.{h,cpp}`), `runFlattenedPlacement`, `run3DPlacement`, `BilevelParams`, `test_phase41_skeleton.tcl`, Tcl `run_flattened_placement` / `run_3d_placement`, SWIG bindings, `CMakeLists.txt`/`BUILD` entries | 8 files | low (pure delete) |
| h | Tcl knobs for `run_planar_correcting`: `-density`, `-intersected_net_weight`, `-max_iter`, `-skip_io_mode` | MultiDieManager.{h,cpp,tcl,i} | low |
| n | Tcl knobs for `run_global_tier_optimization`: `-b_factor`, `-max_net_fanout`, `-u_t_percent`, `-u_b_percent` | same | low |
| i | Decompose `runGlobalTierOptimization` into `findFromToBlocks()`, `mapKnapsackCaps()`, `findToLib()`, `applyMigration()` | MultiDieManager.{h,cpp} | medium (numerical lock guards behavior) |
| j | `getChildBlocks()` (vector return), `dieIndexOf(dbBlock*)` helpers; replace 3+ inline iteration loops | MultiDieManager.{h,cpp} | low |
| k | `GlobalTierOptimizer::overflowAfterAddingCell()` helper; unify `overflow()` / `surrogateDelta` recomputation | GlobalTierOptimizer.cpp | low |
| l | `GlobalTierOptimizer::evaluateMoveCoarse()` helper for huge-net branch (35-line in-place block) | GlobalTierOptimizer.cpp | low |
| m | Replace `dbBoolProperty "intersected"` marker with BTerm-name lookup (`findBTerm(interconnect_name)`); idempotency becomes function-local | MultiDieManager.cpp | medium (DB state change) |
| o | Logger ID audit: grep all `utl::MDM` IDs, ensure no duplicates, add range comment block in `MultiDieManager.h` | grep + comment | very low |

## 5. Commit sequence

Each commit is atomic and verified by smoke. Numerical lock runs at commits marked **(N)**.

1. `chore(mdm): add Phase 4 regression smoke test`
   - Adds smoke test, numerical lock script.
   - Removes `test_phase41_skeleton.tcl`.
   - **(N)** baseline lock = 2,694,774.

2. `refactor(mdm): remove Phase 4 stubs (YAGNI)`
   - Item (g).

3. `refactor(mdm): extract child-block helpers`
   - Item (j).

4. `refactor(mdm): decompose runGlobalTierOptimization`
   - Item (i). **(N)** behavior-equivalence check.

5. `refactor(mdm): expose Tcl knobs for Planar Correcting and Global Tier Optimization`
   - Items (h)+(n). Defaults preserve current hardcoded values. **(N)** behavioral no-op.

6. `refactor(mdm): simplify GlobalTierOptimizer overflow / huge-net branch`
   - Items (k)+(l). **(N)** numerical equivalence.

7. `refactor(mdm): scope makeInterconnections idempotency to function-local signal`
   - Item (m). Replaces `dbBoolProperty` marker with `findBTerm` lookup. **(N)** behavioral equivalence.

8. `chore(mdm): logger ID audit and range documentation`
   - Item (o).

After commit 8: handoff.md update.

## 6. Acceptance criteria

- 8 commits build cleanly with `./build.sh` (or project's build script).
- 8 commits all pass `test_phase4_smoke.tcl`.
- Numerical lock at commit 8 = `2,694,774 ± 0.5%` (case2 e2e via `evaluator_0525`).
- No files outside `src/mdm/` (and one new `.workspace/bench/`, one new `docs/superpowers/specs/`) modified.
- All commits use `git commit -s` (DCO).
- C++ changes formatted with `clang-format -i` (excluding `*.i` per CLAUDE.md).
- handoff.md updated with refactor completion + new Tcl knob inventory.

## 7. Open risks and mitigations

| Risk | Mitigation |
|------|-----------|
| Item (m) DB state change — old DBs may have stale `intersected` properties | New code ignores marker; cross-call dedup uses BTerm presence (a property of net topology, not DB metadata). Document in commit message. |
| Item (i) decomposition could subtly change knapsack cap mapping | Numerical lock at commit 4 catches any HPWL drift. |
| Smoke runtime drift over the session | If smoke exceeds 30s, switch to a smaller fixture. Acceptable upper bound: 2 minutes per smoke run. |
| Phase 4.4 / 4.6 / 4.2.d implementation work later has to recreate stub plumbing | Adding plumbing back when actually needed is ~1 hour each; carrying YAGNI burden in the meantime is worse. |

## 8. References

- Paper iPL-3D §IV.B (Algorithm 2), §IV.D (Planar Correcting). Local: `paper.pdf`.
- handoff.md (Phase 4 series state at HEAD).
- `.workspace/phase4-progress-log.md` (advisor reviews + decisions timeline).
- `.workspace/phase4-research-notes.md` (Phase 4.0 hand-trace).
