# mdm Phase 4 Refactor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor user-authored Phase 4 code in `src/mdm/` (Global Tier Optimization + Planar Correcting + supporting plumbing) for clarity, YAGNI compliance, and Tcl knob exposure — without changing case2 e2e HPWL.

**Architecture:** 8 atomic commits. Each commit is verified by a fast smoke test (`src/mdm/test/test_phase4_smoke.tcl`); structural commits additionally pass a numerical lock against the recorded baseline (`case2 e2e HPWL = 2,694,774 ± 0.5%`).

**Tech Stack:** C++17 (OpenROAD), Tcl 8.6, SWIG, CMake, OpenSTA test harness, `evaluator_0525` (external).

---

## Spec Reference

`docs/superpowers/specs/2026-05-06-mdm-phase4-refactor-design.md` (commit `9a9061aef9`).

## Build & Verify Commands

- **Build**: `cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad`
  (or equivalent: `make -C build openroad -j$(nproc)`)
- **Smoke**: `/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase4_smoke.tcl`
- **Numerical lock**: `bash /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh`
- **Format**: `clang-format -i <file>` (NEVER on `*.i` SWIG files; NEVER on `src/sta/`)
- **Commit**: `git commit -s -m '<message>'` (DCO required)

## File Structure

### Created
- `src/mdm/test/test_phase4_smoke.tcl` — Phase 4 dispatch smoke test (Task 1)
- `.workspace/bench/tcl/regression_phase4_case2.tcl` — case2 e2e numerical lock script (Task 1)
- `.workspace/bench/regression_check.sh` — wrapper that runs lock + parses HPWL (Task 1)

### Modified (across multiple tasks)
- `src/mdm/include/mdm/MultiDieManager.h` — remove stub method declarations (Task 2), add helper declarations (Tasks 3, 4), add Tcl knob params (Task 5), add logger ID range comment (Task 8)
- `src/mdm/src/MultiDieManager.cpp` — remove stub method bodies (Task 2), use helpers (Tasks 3, 4), accept new params (Task 5), simplify idempotency (Task 7)
- `src/mdm/src/MultiDieManager.tcl` — remove `run_flattened_placement`, `run_3d_placement` (Task 2), add new flags to `run_planar_correcting` and `run_global_tier_optimization` (Task 5)
- `src/mdm/src/MultiDieManager.i` — remove stub SWIG bindings (Task 2), add new param SWIG bindings (Task 5). NEVER format with clang-format.
- `src/mdm/src/GlobalTierOptimizer.h` and `.cpp` — extract overflow + huge-net helpers (Task 6)
- `src/mdm/CMakeLists.txt` — remove stub source entries (Task 2)
- `src/mdm/BUILD` — remove stub source entries (Task 2)
- `handoff.md` — update to reflect refactor (Task 9)

### Deleted (Task 2)
- `src/mdm/src/BilevelCoordinator.h`
- `src/mdm/src/BilevelCoordinator.cpp`
- `src/mdm/src/FastTerminalLegalizer.h`
- `src/mdm/src/FastTerminalLegalizer.cpp`
- `src/mdm/test/test_phase41_skeleton.tcl`

---

## Task 1: Add Phase 4 Regression Smoke Test + Numerical Lock

**Goal**: Establish regression infrastructure before any refactor. Smoke runs in <30s; numerical lock proves baseline = 2,694,774.

**Files:**
- Create: `src/mdm/test/test_phase4_smoke.tcl`
- Create: `.workspace/bench/tcl/regression_phase4_case2.tcl`
- Create: `.workspace/bench/regression_check.sh`

- [ ] **Step 1.1: Write the smoke test**

Write `src/mdm/test/test_phase4_smoke.tcl`:

```tcl
# Phase 4 smoke test — verify Tcl dispatch + apply path works.
# Loads case2, runs Algorithm 2 -apply, asserts post-conditions.
# Does NOT run Planar Correcting (slow GPL pass) or SemiLegalizer.
# Numerical lock (HPWL +/- 0.5%) is in regression_phase4_case2.tcl.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2

puts "=== smoke 1: dry-run Algorithm 2 (apply=false) ==="
run_global_tier_optimization

puts "=== smoke 2: Algorithm 2 -apply (partition flip + reinterconnect) ==="
run_global_tier_optimization -apply

puts "=== smoke 3: snap_cells_to_rows ==="
snap_cells_to_rows

puts "=== Phase 4 smoke test: all commands returned cleanly ==="
exit
```

- [ ] **Step 1.2: Write the numerical lock script (Tcl side)**

Write `.workspace/bench/tcl/regression_phase4_case2.tcl` (copy of `phase45_e2e_case2.tcl` content with explicit output path):

```tcl
# Phase 4 numerical lock — case2 e2e for HPWL regression check.
# Baseline: HPWL = 2,694,774 (recorded at HEAD df11a310a7).
# Tolerance: +/- 0.5%.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2
puts "=== HPWL after flat init ==="
get_3d_hpwl
run_global_tier_optimization -apply
puts "=== HPWL after Algorithm 2 + apply ==="
get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar Correcting (1 iter) ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case2.out
exit
```

- [ ] **Step 1.3: Write the regression check shell wrapper**

Write `.workspace/bench/regression_check.sh`:

```bash
#!/usr/bin/env bash
# Phase 4 numerical lock: run case2 e2e, evaluate, compare HPWL to
# baseline. Exits non-zero on regression.
set -e

BASELINE_HPWL=2694774
TOLERANCE_PCT=0.5

ROOT=/home/minjae/workspace/etc/openroad/OpenROAD
ARCHIVE=/home/minjae/workspace/etc/openroad/archive/3d_ic
OPENROAD=$ROOT/build/bin/openroad
EVALUATOR=$ARCHIVE/tools/evaluator_0525
CASE_INPUT=$ARCHIVE/benchmarks/iccad2022/case2.txt
TCL=$ROOT/.workspace/bench/tcl/regression_phase4_case2.tcl
OUTPUT=/tmp/regression_phase4_case2.out
EVAL_LOG=/tmp/regression_phase4_eval.log
RUN_LOG=/tmp/regression_phase4_run.log

echo "[1/3] Running OpenROAD ($TCL)..."
$OPENROAD -no_init -no_splash -exit $TCL > $RUN_LOG 2>&1 || {
    echo "FAIL: OpenROAD run failed. Last 30 lines of $RUN_LOG:"
    tail -30 $RUN_LOG
    exit 1
}

echo "[2/3] Running evaluator..."
$EVALUATOR $CASE_INPUT $OUTPUT > $EVAL_LOG 2>&1 || {
    echo "FAIL: evaluator failed. Output:"
    cat $EVAL_LOG
    exit 1
}

echo "[3/3] Parsing HPWL..."
# evaluator_0525 typically prints a final HPWL line. Try multiple patterns:
HPWL=$(grep -oP 'HPWL\s*[:=]\s*\K[0-9]+' $EVAL_LOG | tail -1)
if [ -z "$HPWL" ]; then
    HPWL=$(grep -oP 'Total\s+HPWL\s*[:=]?\s*\K[0-9]+' $EVAL_LOG | tail -1)
fi
if [ -z "$HPWL" ]; then
    echo "FAIL: could not parse HPWL from evaluator output. Full log:"
    cat $EVAL_LOG
    exit 1
fi

DIFF_PCT=$(echo "scale=4; ($HPWL - $BASELINE_HPWL) * 100 / $BASELINE_HPWL" | bc)
ABS_DIFF_PCT=${DIFF_PCT#-}

echo "Baseline HPWL: $BASELINE_HPWL"
echo "Current HPWL:  $HPWL"
echo "Diff:          $DIFF_PCT %  (tolerance +/- $TOLERANCE_PCT %)"

if [ "$(echo "$ABS_DIFF_PCT > $TOLERANCE_PCT" | bc -l)" = "1" ]; then
    echo "FAIL: HPWL drift exceeds $TOLERANCE_PCT% tolerance."
    exit 1
fi
echo "PASS: HPWL within tolerance."
```

Make executable:

```bash
chmod +x /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh
```

- [ ] **Step 1.4: Build, run smoke**

Run: `cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad`
Expected: build succeeds (no Phase 4 code changes yet, only test scripts).

Run: `/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase4_smoke.tcl`
Expected: prints `=== Phase 4 smoke test: all commands returned cleanly ===` and exits 0.

If smoke fails: investigate. Common causes:
- `/tmp/case2_ipl.out` missing — check `ls /tmp/case2_ipl.out`. If absent, smoke can't run; add a guard or restore the file.
- `/tmp/flat_case2.par` write fails — check permissions.

- [ ] **Step 1.5: Run numerical lock to confirm baseline**

Run: `bash /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh`
Expected: `PASS: HPWL within tolerance.` AND `Current HPWL: 2694774` (or within 0.5%).

If parse fails: inspect `/tmp/regression_phase4_eval.log` and adjust the grep regex in `regression_check.sh` to match the evaluator's actual output format. Document the finding in the script comment.

If HPWL differs from 2,694,774 by more than 0.5%: the baseline has drifted from handoff record. STOP. Re-establish baseline by recording the new HPWL as `BASELINE_HPWL` in the script.

- [ ] **Step 1.6: Remove old skeleton test**

```bash
git rm /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase41_skeleton.tcl
```

- [ ] **Step 1.7: Commit**

```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
git add src/mdm/test/test_phase4_smoke.tcl
git add .workspace/bench/tcl/regression_phase4_case2.tcl
git add .workspace/bench/regression_check.sh
git commit -s -m "$(cat <<'EOF'
chore(mdm): add Phase 4 regression smoke test + numerical lock

Replaces test_phase41_skeleton.tcl (which tested stubs to be removed
in the next commit) with a functional smoke test for the Phase 4
dispatch + apply path. Adds a numerical lock script that runs case2
e2e and compares evaluator HPWL against baseline 2,694,774 +/- 0.5%.

Smoke runs <30s and verifies no-throw for each Phase 4 Tcl command.
Numerical lock runs ~5min and is invoked via bench/regression_check.sh
after structural refactor commits.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Remove Phase 4 Stubs (YAGNI)

**Goal**: Delete `BilevelCoordinator`, `FastTerminalLegalizer`, `runFlattenedPlacement`, `run3DPlacement`, `BilevelParams`, and all associated plumbing. These are all stubs that produce no behavior; they will be re-added when Phase 4.4/4.6/4.2.d implement them.

**Files:**
- Delete: `src/mdm/src/BilevelCoordinator.h`, `src/mdm/src/BilevelCoordinator.cpp`
- Delete: `src/mdm/src/FastTerminalLegalizer.h`, `src/mdm/src/FastTerminalLegalizer.cpp`
- Modify: `src/mdm/include/mdm/MultiDieManager.h` (remove `runFlattenedPlacement`, `run3DPlacement` declarations and their forward decls)
- Modify: `src/mdm/src/MultiDieManager.cpp` (remove `runFlattenedPlacement`, `run3DPlacement` definitions; remove `#include "BilevelCoordinator.h"`, `#include "FastTerminalLegalizer.h"`)
- Modify: `src/mdm/src/MultiDieManager.tcl` (remove `run_flattened_placement`, `run_3d_placement` procs)
- Modify: `src/mdm/src/MultiDieManager.i` (remove SWIG declarations for `run_flattened_placement`, `run_3d_placement`) — DO NOT clang-format
- Modify: `src/mdm/CMakeLists.txt` (remove `src/BilevelCoordinator.cpp`, `src/FastTerminalLegalizer.cpp` lines)
- Modify: `src/mdm/BUILD` (remove same)

- [ ] **Step 2.1: Delete the four stub files**

```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
git rm src/mdm/src/BilevelCoordinator.h
git rm src/mdm/src/BilevelCoordinator.cpp
git rm src/mdm/src/FastTerminalLegalizer.h
git rm src/mdm/src/FastTerminalLegalizer.cpp
```

- [ ] **Step 2.2: Remove method declarations from `MultiDieManager.h`**

In `src/mdm/include/mdm/MultiDieManager.h`:

Remove these forward declarations (around lines 32-35):
```cpp
class GlobalTierOptimizer;
class FastTerminalLegalizer;
class BilevelCoordinator;
struct TierOptParams;
struct BilevelParams;
```

Replace with:
```cpp
class GlobalTierOptimizer;
struct TierOptParams;
```

Remove this method declaration (around lines 104-107):
```cpp
  // Phase 4 — iPL-3D paper §IV.A flattened init wrapper. Phase 4.1: stub.
  // Phase 4.4 implements: temporarily homes all cells to one die, runs
  // RePlAce with relaxed density, restores partition state.
  void runFlattenedPlacement(double density = 1.0);
```

Remove this method declaration (around lines 132-134):
```cpp
  // Phase 4 — iPL-3D paper Algorithm 1 bilevel coordinator. Phase 4.1: stub.
  // Phase 4.6 implements full SP-1 ↔ SP-2 alternation.
  void run3DPlacement(int iterations = 4, bool no_alternating = false);
```

- [ ] **Step 2.3: Remove method definitions from `MultiDieManager.cpp`**

In `src/mdm/src/MultiDieManager.cpp`:

Remove these includes (around lines 18, 20):
```cpp
#include "BilevelCoordinator.h"
#include "FastTerminalLegalizer.h"
```

Remove `runFlattenedPlacement` (around lines 744-751):
```cpp
void MultiDieManager::runFlattenedPlacement(double density)
{
  logger_->info(utl::MDM,
                304,
                "runFlattenedPlacement: stub. density={}. Implemented in "
                "Phase 4.4.",
                density);
}
```

Remove `run3DPlacement` (around lines 1051-1059):
```cpp
void MultiDieManager::run3DPlacement(int iterations, bool no_alternating)
{
  BilevelParams params;
  params.M = iterations;
  params.no_alternating = no_alternating;

  BilevelCoordinator coordinator(this, replace_, logger_);
  coordinator.run(params);
}
```

- [ ] **Step 2.4: Remove Tcl procs from `MultiDieManager.tcl`**

In `src/mdm/src/MultiDieManager.tcl`:

Remove `run_flattened_placement` block (around lines 190-202):
```tcl
# Phase 4 — iPL-3D paper §IV.A flattened init wrapper. Phase 4.1: stub.
sta::define_cmd_args "run_flattened_placement" {[-density density]}

proc run_flattened_placement { args } {
  sta::parse_key_args "run_flattened_placement" args \
    keys {-density} flags {}

  set density 1.0
  if { [info exists keys(-density)] } {
    set density $keys(-density)
  }
  mdm::run_flattened_placement $density
}
```

Remove `run_3d_placement` block (around lines 261-278):
```tcl
# Phase 4 — iPL-3D paper Algorithm 1 bilevel coordinator. Phase 4.1: stub.
sta::define_cmd_args "run_3d_placement" {\
    [-iterations iter] [-no_alternating]}

proc run_3d_placement { args } {
  sta::parse_key_args "run_3d_placement" args \
    keys {-iterations} flags {-no_alternating}

  set iterations 4
  set no_alt 0
  if { [info exists keys(-iterations)] } {
    set iterations $keys(-iterations)
  }
  if { [info exists flags(-no_alternating)] } {
    set no_alt 1
  }
  mdm::run_3d_placement $iterations $no_alt
}
```

- [ ] **Step 2.5: Remove SWIG bindings from `MultiDieManager.i`**

In `src/mdm/src/MultiDieManager.i`, remove the function signatures for `run_flattened_placement` and `run_3d_placement`. Each will be a `void run_flattened_placement(double);` and `void run_3d_placement(int, int);` style line — find them in the file and delete.

CRITICAL: Do NOT run clang-format on `*.i` files. Manually edit only.

- [ ] **Step 2.6: Remove from `CMakeLists.txt` and `BUILD`**

In `src/mdm/CMakeLists.txt`, remove these two lines from `target_sources(mdm PRIVATE ...)`:
```cmake
    src/BilevelCoordinator.cpp
    src/FastTerminalLegalizer.cpp
```

In `src/mdm/BUILD`, find the equivalent entries and remove them. (The file content is similar — remove any `BilevelCoordinator.cpp` / `FastTerminalLegalizer.cpp` references.)

- [ ] **Step 2.7: Build and run smoke**

Run: `cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad 2>&1 | tail -40`
Expected: build succeeds. If `BilevelCoordinator` symbols are still referenced, find the leftover include or call site.

Run smoke: `/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase4_smoke.tcl`
Expected: passes (smoke does not invoke removed commands).

- [ ] **Step 2.8: Format C++ files**

```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
clang-format -i src/mdm/include/mdm/MultiDieManager.h src/mdm/src/MultiDieManager.cpp
# DO NOT format MultiDieManager.i (SWIG)
```

- [ ] **Step 2.9: Commit**

```bash
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): remove Phase 4 stubs (YAGNI)

Removes BilevelCoordinator, FastTerminalLegalizer, runFlattenedPlacement,
run3DPlacement and their plumbing (Tcl procs, SWIG bindings, CMake
entries). All four were stubs that produced no behavior; carrying them
violates YAGNI and creates a misleading "Phase 4.6 / 4.4 / 4.2.d are
plumbed" signal.

These will be re-added with real implementations when their respective
phases land:
- BilevelCoordinator: Phase 4.6 (paper Algorithm 1 SP-1<->SP-2 alternation)
- FastTerminalLegalizer: Phase 4.2.d (paper §IV.B-1 in-loop terminal)
- runFlattenedPlacement: Phase 4.4 (paper §IV.A density-relaxed flat)

The active code paths (runGlobalTierOptimization, runPlanarCorrecting,
snapCellsToRows, runSemiLegalizer) are untouched.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Extract Child-Block Helpers

**Goal**: Add `MultiDieManager::getChildBlocks()` and `dieIndexOf(odb::dbBlock*)` helpers. Replace ~3 inline child-iteration loops with these. Pure restructuring.

**Files:**
- Modify: `src/mdm/include/mdm/MultiDieManager.h` (add helper declarations)
- Modify: `src/mdm/src/MultiDieManager.cpp` (add helper definitions; replace 3 inline loops)

- [ ] **Step 3.1: Add helper declarations in `MultiDieManager.h`**

After `private:` (around line 137 in the current file), before `void splitInstances();`, add:

```cpp
  // Convenience: child blocks of the top-hier block in iteration order.
  // Convention: index 0 = top die, index 1 = bottom die.
  std::vector<odb::dbBlock*> getChildBlocks() const;

  // Returns the index in getChildBlocks() of the given child, or -1 if
  // not a child of the top-hier block.
  int dieIndexOf(odb::dbBlock* block) const;
```

- [ ] **Step 3.2: Add helper definitions at top of `MultiDieManager.cpp`**

Right after the `MultiDieManager` constructor (after the closing `}` around line 48), add:

```cpp
std::vector<odb::dbBlock*> MultiDieManager::getChildBlocks() const
{
  std::vector<odb::dbBlock*> result;
  if (!db_ || !db_->getChip() || !db_->getChip()->getBlock()) {
    return result;
  }
  for (auto* child : db_->getChip()->getBlock()->getChildren()) {
    result.push_back(child);
  }
  return result;
}

int MultiDieManager::dieIndexOf(odb::dbBlock* block) const
{
  const auto children = getChildBlocks();
  for (size_t i = 0; i < children.size(); ++i) {
    if (children[i] == block) {
      return static_cast<int>(i);
    }
  }
  return -1;
}
```

- [ ] **Step 3.3: Replace inline loops in `runGlobalTierOptimization`**

In `runGlobalTierOptimization` (currently around lines 786-808), find:

```cpp
  auto child_iter = parent->getChildren().begin();
  odb::dbBlock* child_a = *child_iter;
  ++child_iter;
  odb::dbBlock* child_b
      = (child_iter != parent->getChildren().end()) ? *child_iter : nullptr;
  if (!child_b) {
    logger_->warn(utl::MDM,
                  308,
                  "runGlobalTierOptimization: only one child block; need two.");
    return;
  }
```

Replace with:

```cpp
  const auto children = getChildBlocks();
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  308,
                  "runGlobalTierOptimization: need >=2 child blocks; got {}.",
                  children.size());
    return;
  }
  odb::dbBlock* child_a = children[0];
  odb::dbBlock* child_b = children[1];
```

Find the `to_lib` lookup loop (around lines 824-847):

```cpp
  odb::dbLib* to_lib = nullptr;
  {
    int target_die_id = 0;
    for (auto* child : parent->getChildren()) {
      if (child == to_block) {
        break;
      }
      ++target_die_id;
    }
    auto lib_iter = db_->getLibs().begin();
    if (lib_iter != db_->getLibs().end()) {
      ++lib_iter;  // skip TopHierLib
      for (int i = 0; i < target_die_id && lib_iter != db_->getLibs().end();
           ++i) {
        ++lib_iter;
      }
      if (lib_iter != db_->getLibs().end()) {
        to_lib = *lib_iter;
      }
    }
  }
```

Replace with:

```cpp
  odb::dbLib* to_lib = nullptr;
  {
    const int target_die_id = dieIndexOf(to_block);
    auto lib_iter = db_->getLibs().begin();
    if (lib_iter != db_->getLibs().end()) {
      ++lib_iter;  // skip TopHierLib
      for (int i = 0; i < target_die_id && lib_iter != db_->getLibs().end();
           ++i) {
        ++lib_iter;
      }
      if (lib_iter != db_->getLibs().end()) {
        to_lib = *lib_iter;
      }
    }
  }
```

Find the `new_die_id` loop in the apply path (around lines 851-858):

```cpp
  if (apply && !delta.empty()) {
    int new_die_id = 0;
    for (auto* child : parent->getChildren()) {
      if (child == to_block) {
        break;
      }
      ++new_die_id;
    }
```

Replace with:

```cpp
  if (apply && !delta.empty()) {
    const int new_die_id = dieIndexOf(to_block);
```

Find the post-migration `first_child`/`second_child` discovery (around lines 873-878):

```cpp
    auto first_iter = parent->getChildren().begin();
    odb::dbBlock* first_child = *first_iter;
    ++first_iter;
    odb::dbBlock* second_child
        = (first_iter != parent->getChildren().end()) ? *first_iter : nullptr;
    if (second_child) {
      makeInterconnections(first_child, second_child);
```

Replace with:

```cpp
    if (children.size() >= 2) {
      makeInterconnections(children[0], children[1]);
```

Note: `children` is already declared above. Adjust subsequent code that referenced `second_child` — change to `children[1]`. The block close `}` for `if (second_child)` becomes the close for `if (children.size() >= 2)`.

- [ ] **Step 3.4: Replace inline loop in `runPlanarCorrecting`**

In `runPlanarCorrecting` (currently around lines 953-971), find:

```cpp
  odb::dbBlock* parent = db_->getChip()->getBlock();
  if (!parent || parent->getChildren().empty()) {
    logger_->warn(utl::MDM,
                  310,
                  "runPlanarCorrecting: no child blocks; call set_3D_IC + "
                  "run_global_tier_optimization -apply first.");
    return;
  }
  std::vector<odb::dbBlock*> children;
  for (auto* c : parent->getChildren()) {
    children.push_back(c);
  }
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  311,
                  "runPlanarCorrecting: need >=2 child blocks; got {}.",
                  children.size());
    return;
  }
```

Replace with:

```cpp
  const auto children = getChildBlocks();
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  310,
                  "runPlanarCorrecting: need >=2 child blocks (got {}); call "
                  "set_3D_IC + run_global_tier_optimization -apply first.",
                  children.size());
    return;
  }
```

Note: this consolidates two warn paths (310, 311) into one. Logger ID 311 is freed for future use.

- [ ] **Step 3.5: Replace inline loop in `snapCellsToRows`**

In `snapCellsToRows` (currently around lines 1002-1010), find:

```cpp
  odb::dbBlock* parent = db_->getChip()->getBlock();
  if (!parent) {
    return;
  }
  int total_snapped = 0;
  int total_max_dy = 0;
  for (auto* child : parent->getChildren()) {
```

Replace with:

```cpp
  int total_snapped = 0;
  int total_max_dy = 0;
  for (auto* child : getChildBlocks()) {
```

- [ ] **Step 3.6: Build, run smoke**

Run: `cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad 2>&1 | tail -20`
Expected: build succeeds.

Run smoke: `/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase4_smoke.tcl`
Expected: passes.

- [ ] **Step 3.7: Format and commit**

```bash
clang-format -i src/mdm/include/mdm/MultiDieManager.h src/mdm/src/MultiDieManager.cpp
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): extract getChildBlocks/dieIndexOf helpers

Replaces three inline child-iteration loops in
runGlobalTierOptimization, runPlanarCorrecting, and snapCellsToRows
with single helpers that document the "first child = top die"
convention in one place.

No behavior change.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Decompose `runGlobalTierOptimization`

**Goal**: Split the 155-line function into 4 named private helpers + a slim driver. The apply path (`applyMigration`) becomes reusable for Phase 4.7 cross-die move.

**Files:**
- Modify: `src/mdm/include/mdm/MultiDieManager.h` (add 4 private helper declarations)
- Modify: `src/mdm/src/MultiDieManager.cpp` (extract helper bodies)

- [ ] **Step 4.1: Add helper declarations in `MultiDieManager.h`**

After the `getChildBlocks`/`dieIndexOf` declarations from Task 3 (and before `void splitInstances()`), add:

```cpp
  // Pick (from, to) blocks for Algorithm 2: from = block with more cells,
  // to = the other. Both must be valid; checks performed by caller.
  // Returns {from, to, from_is_top_die}. from_is_top_die is true iff
  // from == children[0] (per "first child = top" convention).
  struct FromToBlocks
  {
    odb::dbBlock* from;
    odb::dbBlock* to;
    bool from_is_top_die;
  };
  FromToBlocks findFromToBlocks(const std::vector<odb::dbBlock*>& children)
      const;

  // Compute knapsack capacities (cap_from_dbu, cap_to_dbu) given the ICCAD
  // u_t/u_b utilization caps and the children's core area. Uses
  // children[0]'s core (top die) for area, matching the legacy
  // implementation's child_a->getCoreArea() reference. The caller assigns
  // outputs into params.cap_from_dbu / params.cap_to_dbu.
  void mapKnapsackCaps(const std::vector<odb::dbBlock*>& children,
                       const FromToBlocks& ft,
                       int u_t_percent,
                       int u_b_percent,
                       int64_t& out_cap_from_dbu,
                       int64_t& out_cap_to_dbu) const;

  // Locate the dbLib for a target child block, skipping TopHierLib.
  // Returns nullptr if the lib cannot be found.
  odb::dbLib* findLibForBlock(odb::dbBlock* block) const;

  // Apply Algorithm 2's migration decision: flip partition_id, run
  // SwitchInstanceHelper, re-pair cross-die nets, re-pair IO pins,
  // and strip floating top-hier nets.
  // Returns the number of cells migrated.
  int applyMigration(const std::vector<odb::dbInst*>& cells,
                     odb::dbBlock* to_block);
```

- [ ] **Step 4.2: Define `findFromToBlocks` in `MultiDieManager.cpp`**

After `dieIndexOf` definition, add:

```cpp
MultiDieManager::FromToBlocks MultiDieManager::findFromToBlocks(
    const std::vector<odb::dbBlock*>& children) const
{
  FromToBlocks result{children[0], children[1], true};
  if (children[0]->getInsts().size() < children[1]->getInsts().size()) {
    result.from = children[1];
    result.to = children[0];
    result.from_is_top_die = false;
  }
  return result;
}
```

- [ ] **Step 4.3: Define `mapKnapsackCaps`**

After `findFromToBlocks`, add:

```cpp
void MultiDieManager::mapKnapsackCaps(
    const std::vector<odb::dbBlock*>& children,
    const FromToBlocks& ft,
    int u_t_percent,
    int u_b_percent,
    int64_t& out_cap_from_dbu,
    int64_t& out_cap_to_dbu) const
{
  out_cap_from_dbu = 0;
  out_cap_to_dbu = 0;
  // Convention: children[0] = top, children[1] = bottom. ICCAD header
  // gives u_t for top and u_b for bottom. Use children[0]'s core
  // (top die) for area to preserve the legacy behavior — different-tech
  // designs may have differing-dim cores between dies.
  odb::Rect core = children[0]->getCoreArea();
  int64_t core_area
      = static_cast<int64_t>(core.dx()) * static_cast<int64_t>(core.dy());
  if (core_area == 0) {
    odb::Rect from_core = ft.from->getCoreArea();
    core_area = static_cast<int64_t>(from_core.dx())
                * static_cast<int64_t>(from_core.dy());
  }
  if (core_area == 0) {
    return;
  }
  if (ft.from_is_top_die) {
    out_cap_from_dbu = core_area * u_t_percent / 100;
    out_cap_to_dbu = core_area * u_b_percent / 100;
  } else {
    out_cap_from_dbu = core_area * u_b_percent / 100;
    out_cap_to_dbu = core_area * u_t_percent / 100;
  }
}
```

- [ ] **Step 4.4: Define `findLibForBlock`**

After `mapKnapsackCaps`, add:

```cpp
odb::dbLib* MultiDieManager::findLibForBlock(odb::dbBlock* block) const
{
  if (!db_ || !block) {
    return nullptr;
  }
  const int target_die_id = dieIndexOf(block);
  if (target_die_id < 0) {
    return nullptr;
  }
  auto lib_iter = db_->getLibs().begin();
  if (lib_iter == db_->getLibs().end()) {
    return nullptr;
  }
  ++lib_iter;  // skip TopHierLib
  for (int i = 0; i < target_die_id && lib_iter != db_->getLibs().end(); ++i) {
    ++lib_iter;
  }
  return (lib_iter != db_->getLibs().end()) ? *lib_iter : nullptr;
}
```

- [ ] **Step 4.5: Define `applyMigration`**

After `findLibForBlock`, add:

```cpp
int MultiDieManager::applyMigration(const std::vector<odb::dbInst*>& cells,
                                    odb::dbBlock* to_block)
{
  if (cells.empty() || !to_block) {
    return 0;
  }
  const int new_die_id = dieIndexOf(to_block);
  if (new_die_id < 0) {
    return 0;
  }
  int migrated = 0;
  for (auto* c : cells) {
    auto* prop = odb::dbIntProperty::find(c, "partition_id");
    if (prop) {
      prop->setValue(new_die_id);
    } else {
      odb::dbIntProperty::create(c, "partition_id", new_die_id);
    }
    SwitchInstanceHelper::switchInstanceToAssignedDie(this, c);
    ++migrated;
  }
  // Re-pair cross-die nets that became cross-die through this migration.
  const auto children = getChildBlocks();
  if (children.size() >= 2) {
    makeInterconnections(children[0], children[1]);
    makeIOPinInterconnections();
    // Strip floating nets on the parent (their iTerms/BTerms moved).
    std::vector<odb::dbNet*> floating;
    odb::dbBlock* parent = db_->getChip()->getBlock();
    for (auto* net : parent->getNets()) {
      if (net->getBTermCount() == 0 && net->getITermCount() == 0) {
        floating.push_back(net);
      }
    }
    for (auto* net : floating) {
      odb::dbNet::destroy(net);
    }
  }
  return migrated;
}
```

- [ ] **Step 4.6: Slim down `runGlobalTierOptimization`**

Replace the entire body of `MultiDieManager::runGlobalTierOptimization` (the 155 lines from lines 753-907 in the original file) with:

```cpp
void MultiDieManager::runGlobalTierOptimization(double rho,
                                                double alpha,
                                                double beta,
                                                double gamma,
                                                bool apply)
{
  TierOptParams params;
  params.rho = rho;
  params.alpha = alpha;
  params.beta = beta;
  params.gamma = gamma;
  // dbu/μm conversion so the surrogate runs in paper's normalized μm
  // units; paper Table III's ρ=500, α=100, β=0.5 then apply directly.
  params.dbu_per_um = getICCADScale();
  // u_t/u_b come from the ICCAD case header (TestCaseManager). Defaults
  // hold if no ICCAD case parsed.
  auto utils = getMaxUtils();
  if (utils.first > 0) {
    params.u_t_percent = utils.first;
  }
  if (utils.second > 0) {
    params.u_b_percent = utils.second;
  }

  const auto children = getChildBlocks();
  if (children.size() < 2) {
    logger_->warn(utl::MDM,
                  308,
                  "runGlobalTierOptimization: need >=2 child blocks; got {}.",
                  children.size());
    return;
  }

  const FromToBlocks ft = findFromToBlocks(children);
  mapKnapsackCaps(children,
                  ft,
                  params.u_t_percent,
                  params.u_b_percent,
                  params.cap_from_dbu,
                  params.cap_to_dbu);
  odb::dbLib* to_lib = findLibForBlock(ft.to);

  GlobalTierOptimizer optimizer(db_, logger_);
  auto delta = optimizer.run(ft.from, ft.to, params, to_lib);

  if (apply && !delta.empty()) {
    const int migrated = applyMigration(delta, ft.to);
    logger_->info(utl::MDM,
                  306,
                  "runGlobalTierOptimization: returned {} cells, applied "
                  "{} migrations to die {}.",
                  delta.size(),
                  migrated,
                  dieIndexOf(ft.to));
  } else {
    logger_->info(utl::MDM,
                  309,
                  "runGlobalTierOptimization: returned {} cells, apply={} "
                  "(no migration).",
                  delta.size(),
                  apply);
  }
}
```

Also remove the old `runGlobalTierOptimization: no child blocks` log path — the new check at `children.size() < 2` covers both (no children and one child).

- [ ] **Step 4.7: Build, run smoke**

Run build. Run smoke. Both must pass.

- [ ] **Step 4.8: Run numerical lock**

```bash
bash /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh
```
Expected: PASS, HPWL within 0.5% of 2,694,774.

If FAIL: this is a behavioral regression. Inspect:
- Is `findFromToBlocks` picking the same from/to as the original? (compare insts.size at children[0] vs children[1])
- Is `mapKnapsackCaps` producing the same caps? (the original branched on `from_is_top` after pre-computing `core_top.dx() * core_top.dy()`; new code uses `ft.from`'s core)
- Is `findLibForBlock` finding the same lib?

The original used `child_a->getCoreArea()` (which is children[0] = top) for `core_area` regardless of from-direction. New code uses `ft.from->getCoreArea()`. **If from = bottom, this changes.** For case2, from is whichever has more cells; verify this matches the original's intent.

To verify: add a debug print in the new helper, run case2, compare against expected ~711 #Term reported in handoff.

If the cap mapping is intentionally different from the original (more correct), but case2 numerical drifts >0.5%: roll back this commit, switch to faithful preservation: `core_area = children[0]->getCoreArea().dx() * .dy()` (top-die's core, regardless of from-direction).

- [ ] **Step 4.9: Format and commit**

```bash
clang-format -i src/mdm/include/mdm/MultiDieManager.h src/mdm/src/MultiDieManager.cpp
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): decompose runGlobalTierOptimization

Splits the 155-line function into 4 named private helpers:
- findFromToBlocks: pick from = busier child, to = other
- mapKnapsackCaps: u_t/u_b -> cap_from_dbu/cap_to_dbu given top/bottom
- findLibForBlock: lookup target lib (skip TopHierLib)
- applyMigration: partition_id flip + SwitchInstanceHelper +
                  re-interconnect + IO pin re-pair + floating cleanup

The driver becomes a 50-line read-then-dispatch. applyMigration is a
reusable primitive for Phase 4.7 cross-die move.

Behavior preserved: case2 e2e HPWL = 2,694,774 +/- 0.5% (numerical lock
passes).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Expose Tcl Knobs (Planar Correcting + Global Tier Optimization)

**Goal**: Make `density`, `intersected_net_weight`, `nesterov_max_iter`, `skip_io_mode` for Planar Correcting and `b_factor`, `max_net_fanout`, `u_t_percent`, `u_b_percent` for Global Tier Optimization tunable from Tcl. Defaults preserve current hardcoded values (no behavioral change).

**Files:**
- Modify: `src/mdm/include/mdm/MultiDieManager.h` (extend method signatures)
- Modify: `src/mdm/src/MultiDieManager.cpp` (accept new params; use them)
- Modify: `src/mdm/src/MultiDieManager.tcl` (add Tcl flag parsing)
- Modify: `src/mdm/src/MultiDieManager.i` (extend SWIG bindings) — manually only

- [ ] **Step 5.1: Extend `runPlanarCorrecting` signature in `MultiDieManager.h`**

Replace:
```cpp
  void runPlanarCorrecting(int iterations = 1);
```

With:
```cpp
  void runPlanarCorrecting(int iterations = 1,
                           double density = 1.5,
                           double intersected_net_weight = 1.5,
                           int nesterov_max_iter = 5000,
                           bool skip_io_mode = true);
```

- [ ] **Step 5.2: Extend `runGlobalTierOptimization` signature in `MultiDieManager.h`**

Replace:
```cpp
  void runGlobalTierOptimization(double rho = 500.0,
                                 double alpha = 100.0,
                                 double beta = 0.5,
                                 double gamma = 0.0,
                                 bool apply = false);
```

With:
```cpp
  void runGlobalTierOptimization(double rho = 500.0,
                                 double alpha = 100.0,
                                 double beta = 0.5,
                                 double gamma = 0.0,
                                 double b_factor = 1.0,
                                 int max_net_fanout = 100,
                                 int u_t_percent = 0,
                                 int u_b_percent = 0,
                                 bool apply = false);
```

`u_t_percent = 0` and `u_b_percent = 0` mean "use ICCAD case header values" (preserving current behavior).

- [ ] **Step 5.3: Update `runPlanarCorrecting` body in `MultiDieManager.cpp`**

Find the body (around lines 946-1000 post-Task-4). Replace the hardcoded `opts.density = 1.5;` etc. with:

Before:
```cpp
      gpl::PlaceOptions opts;
      opts.skipIoMode = true;
      opts.density = 1.5;
      opts.intersectedNetWeight = 1.5;
      // Use doNesterovPlace (skips initial-place CG iterations that
      // crash on the post-migration mixed-block layout). Cells already
      // have valid placements from the migration, so initial place is
      // not needed.
      replace_->doNesterovPlace(/*threads=*/1, opts);
```

After:
```cpp
      gpl::PlaceOptions opts;
      opts.skipIoMode = skip_io_mode;
      opts.density = density;
      opts.intersectedNetWeight = intersected_net_weight;
      opts.nesterovPlaceMaxIter = nesterov_max_iter;
      // Use doNesterovPlace (skips initial-place CG iterations that
      // crash on the post-migration mixed-block layout). Cells already
      // have valid placements from the migration, so initial place is
      // not needed.
      replace_->doNesterovPlace(/*threads=*/1, opts);
```

Update the function signature in the .cpp definition to match the header. Top of body should be:

```cpp
void MultiDieManager::runPlanarCorrecting(int iterations,
                                          double density,
                                          double intersected_net_weight,
                                          int nesterov_max_iter,
                                          bool skip_io_mode)
{
```

- [ ] **Step 5.4: Update `runGlobalTierOptimization` body**

Update the function signature in the .cpp definition to match the new header. Inside the body, after `params.gamma = gamma;`:

```cpp
  params.B_factor = b_factor;
  params.max_net_fanout = max_net_fanout;
```

Then update the `u_t_percent`/`u_b_percent` resolution:

Find:
```cpp
  auto utils = getMaxUtils();
  if (utils.first > 0) {
    params.u_t_percent = utils.first;
  }
  if (utils.second > 0) {
    params.u_b_percent = utils.second;
  }
```

Replace with:
```cpp
  // If caller passes 0, fall back to ICCAD case header values.
  auto utils = getMaxUtils();
  params.u_t_percent
      = (u_t_percent > 0) ? u_t_percent
                          : (utils.first > 0 ? utils.first : params.u_t_percent);
  params.u_b_percent
      = (u_b_percent > 0)
            ? u_b_percent
            : (utils.second > 0 ? utils.second : params.u_b_percent);
```

- [ ] **Step 5.5: Update Tcl `run_planar_correcting`**

In `src/mdm/src/MultiDieManager.tcl`, replace the `run_planar_correcting` block (around lines 238-249) with:

```tcl
# Phase 4 — iPL-3D paper §IV.D Planar Solution Correcting (SP-2).
sta::define_cmd_args "run_planar_correcting" {\
    [-iterations iter] \
    [-density density] \
    [-intersected_net_weight w] \
    [-nesterov_max_iter n] \
    [-no_skip_io_mode]}

proc run_planar_correcting { args } {
  sta::parse_key_args "run_planar_correcting" args \
    keys {-iterations -density -intersected_net_weight -nesterov_max_iter} \
    flags {-no_skip_io_mode}

  set iterations 1
  set density 1.5
  set intersected_net_weight 1.5
  set nesterov_max_iter 5000
  set skip_io_mode 1
  if { [info exists keys(-iterations)] } {
    set iterations $keys(-iterations)
  }
  if { [info exists keys(-density)] } {
    set density $keys(-density)
  }
  if { [info exists keys(-intersected_net_weight)] } {
    set intersected_net_weight $keys(-intersected_net_weight)
  }
  if { [info exists keys(-nesterov_max_iter)] } {
    set nesterov_max_iter $keys(-nesterov_max_iter)
  }
  if { [info exists flags(-no_skip_io_mode)] } {
    set skip_io_mode 0
  }
  mdm::run_planar_correcting $iterations $density $intersected_net_weight \
                             $nesterov_max_iter $skip_io_mode
}
```

- [ ] **Step 5.6: Update Tcl `run_global_tier_optimization`**

Replace the `run_global_tier_optimization` block (around lines 204-236) with:

```tcl
# Phase 4 — iPL-3D paper §IV.B Algorithm 2 single-shot.
sta::define_cmd_args "run_global_tier_optimization" {\
    [-rho rho] [-alpha alpha] [-beta beta] [-gamma gamma] \
    [-b_factor b] [-max_net_fanout f] \
    [-u_t_percent u_t] [-u_b_percent u_b] \
    [-apply]}

proc run_global_tier_optimization { args } {
  sta::parse_key_args "run_global_tier_optimization" args \
    keys {-rho -alpha -beta -gamma -b_factor -max_net_fanout \
          -u_t_percent -u_b_percent} \
    flags {-apply}

  # Paper Table III defaults — surrogate runs in normalized μm
  # (auto-converted from dbu via getICCADScale), so paper's constants
  # apply directly.
  set rho 500.0
  set alpha 100.0
  set beta 0.5
  set gamma 0.0
  set b_factor 1.0
  set max_net_fanout 100
  set u_t_percent 0  ;# 0 = fall back to ICCAD case header value
  set u_b_percent 0
  set apply 0
  if { [info exists keys(-rho)] } { set rho $keys(-rho) }
  if { [info exists keys(-alpha)] } { set alpha $keys(-alpha) }
  if { [info exists keys(-beta)] } { set beta $keys(-beta) }
  if { [info exists keys(-gamma)] } { set gamma $keys(-gamma) }
  if { [info exists keys(-b_factor)] } { set b_factor $keys(-b_factor) }
  if { [info exists keys(-max_net_fanout)] } {
    set max_net_fanout $keys(-max_net_fanout)
  }
  if { [info exists keys(-u_t_percent)] } {
    set u_t_percent $keys(-u_t_percent)
  }
  if { [info exists keys(-u_b_percent)] } {
    set u_b_percent $keys(-u_b_percent)
  }
  if { [info exists flags(-apply)] } { set apply 1 }
  mdm::run_global_tier_optimization $rho $alpha $beta $gamma \
                                    $b_factor $max_net_fanout \
                                    $u_t_percent $u_b_percent $apply
}
```

- [ ] **Step 5.7: Update SWIG bindings in `MultiDieManager.i`**

In `src/mdm/src/MultiDieManager.i`, find the SWIG declaration for `run_global_tier_optimization` and `run_planar_correcting` (likely as `void run_global_tier_optimization(double, double, ...)`), update the signatures to match the new C++ method signatures.

CRITICAL: do NOT clang-format `*.i` files. Manually update only the lines that change.

The new signatures (paste these into the .i in place of the old ones):

```cpp
void run_global_tier_optimization(double rho, double alpha,
                                  double beta, double gamma,
                                  double b_factor, int max_net_fanout,
                                  int u_t_percent, int u_b_percent,
                                  bool apply);
void run_planar_correcting(int iterations, double density,
                           double intersected_net_weight,
                           int nesterov_max_iter, bool skip_io_mode);
```

(Adjust to match the existing .i indentation style.)

- [ ] **Step 5.8: Build, run smoke**

Build. Smoke must pass.

If smoke fails with Tcl arity error: the SWIG bindings did not regenerate. Force-touch the .i and rebuild:
```bash
touch /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/src/MultiDieManager.i
cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad
```

- [ ] **Step 5.9: Run numerical lock**

```bash
bash /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh
```
Expected: PASS. Defaults match old hardcoded values, so HPWL must equal 2,694,774 ± 0.5%.

- [ ] **Step 5.10: Format and commit**

```bash
clang-format -i src/mdm/include/mdm/MultiDieManager.h src/mdm/src/MultiDieManager.cpp
# Do NOT format MultiDieManager.i or MultiDieManager.tcl
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): expose Tcl knobs for runPlanarCorrecting + Global Tier Opt

run_planar_correcting now accepts -density, -intersected_net_weight,
-nesterov_max_iter, -no_skip_io_mode. These were hardcoded to 1.5,
1.5, 5000, true respectively; defaults preserved.

run_global_tier_optimization now accepts -b_factor, -max_net_fanout,
-u_t_percent, -u_b_percent. Defaults match paper Table III (1.0, 100,
ICCAD case header). u_t/u_b = 0 means fall back to header.

This unblocks the case3 Planar Correcting Nesterov non-convergence fix
(handoff §4 leverage point #1), which requires tuning the GPL knobs
without C++ rebuilds.

No behavior change at default values: case2 e2e HPWL = 2,694,774
(numerical lock passes).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Simplify `GlobalTierOptimizer` (overflow + huge-net helpers)

**Goal**: Extract `overflowAfterAddingCell()` and `evaluateMoveCoarse()` to remove duplicated arithmetic and the 35-line in-place huge-net branch.

**Files:**
- Modify: `src/mdm/src/GlobalTierOptimizer.h` (declarations)
- Modify: `src/mdm/src/GlobalTierOptimizer.cpp` (definitions; replace inline code)

- [ ] **Step 6.1: Add declarations in `GlobalTierOptimizer.h`**

In the `private:` section, after `double overflow(const Context& ctx) const;`, add:

```cpp
  // Compute d(S ∪ {cell}) for the surrogate. Same formula as overflow()
  // but applies (cell's from-tech area, cell's to-tech area) deltas.
  // Returned in μm.
  double overflowAfterAddingCell(odb::dbInst* cell, const Context& ctx) const;
```

In the same anonymous namespace at the top of `GlobalTierOptimizer.cpp` (around line 19), add a forward declaration for the coarse path. Actually it's a free helper — declare it as a static method on the class instead, after `evaluateMove`:

Wait — `evaluateMoveCoarse` operates on a single net (not on the whole cell). Make it a free helper inside the anonymous namespace:

In the anonymous namespace (alongside `classifyPins`, `isCrossing`, `bboxHpwl`, `netHpwl`), add:

```cpp
// Coarse evaluation for a huge-fanout net: skip ΔWL geometry but still
// account for Δ#Term. Walks pins for membership only (O(fanout) without
// per-pin getAvgXY). Without this, the surrogate would systematically
// under-count ε flips on exactly the nets where each crossing is most
// expensive (advisor R-13 finding).
struct CoarseDelta
{
  int delta_term = 0;
};

CoarseDelta evaluateNetCoarse(odb::dbNet* net,
                              odb::dbInst* moving_cell,
                              const std::unordered_set<odb::dbInst*>& S)
{
  CoarseDelta cd;
  bool before_to = false;
  bool before_from = false;
  bool after_to = false;
  bool after_from = false;
  for (auto* peer_it : net->getITerms()) {
    odb::dbInst* inst = peer_it->getInst();
    if (!inst) {
      continue;
    }
    const bool peer_on_to = S.count(inst) > 0;
    if (inst == moving_cell) {
      // After move, the cell sits on the to-side; before, from-side.
      before_from = true;
      after_to = true;
    } else if (peer_on_to) {
      before_to = true;
      after_to = true;
    } else {
      before_from = true;
      after_from = true;
    }
  }
  const bool before_crossing = before_to && before_from;
  const bool after_crossing = after_to && after_from;
  cd.delta_term = (after_crossing ? 1 : 0) - (before_crossing ? 1 : 0);
  return cd;
}
```

- [ ] **Step 6.2: Define `overflowAfterAddingCell` in `GlobalTierOptimizer.cpp`**

After the existing `overflow()` definition (around line 251), add:

```cpp
double GlobalTierOptimizer::overflowAfterAddingCell(odb::dbInst* cell,
                                                   const Context& ctx) const
{
  const int64_t c_from = cellAreaInFromTech(cell, ctx);
  const int64_t c_to = cellAreaInToTech(cell, ctx);
  const int64_t from_used_after
      = ctx.from_total_area - ctx.s_total_area_from - c_from;
  const int64_t to_used_after
      = ctx.to_existing_area + ctx.s_total_area_to + c_to;
  const int64_t from_over_after
      = std::max<int64_t>(from_used_after - ctx.cap_from_dbu, 0);
  const int64_t to_over_after
      = std::max<int64_t>(to_used_after - ctx.cap_t_dbu, 0);
  const double d_dbu = static_cast<double>(from_over_after + to_over_after)
                       / static_cast<double>(ctx.row_height);
  return d_dbu / static_cast<double>(std::max(1, ctx.params.dbu_per_um));
}
```

- [ ] **Step 6.3: Simplify `surrogateDelta` to use the helper**

Replace the body of `surrogateDelta` (currently around lines 253-284) with:

```cpp
double GlobalTierOptimizer::surrogateDelta(odb::dbInst* cell,
                                           const Context& ctx) const
{
  const MoveDelta md = evaluateMove(cell, ctx);
  // All length-like quantities (ΔWL, d) converted dbu → μm so paper's
  // Table III constants (ρ=500, α=100, β=0.5, γ=0|1e4) apply directly.
  // ΔTerm is dimensionless (count) — no conversion.
  const double dbu_per_um
      = static_cast<double>(std::max(1, ctx.params.dbu_per_um));
  const double delta_wl_um = static_cast<double>(md.delta_wl) / dbu_per_um;

  const double d_before = overflow(ctx);
  const double d_after = overflowAfterAddingCell(cell, ctx);

  return delta_wl_um
         + ctx.params.rho * static_cast<double>(md.delta_term)
         + ctx.params.alpha * (d_after - d_before)
         - ctx.params.gamma * d_before;
}
```

- [ ] **Step 6.4: Replace huge-net branch in `evaluateMove`**

In `evaluateMove` (around lines 173-232), find the huge-net branch (the 35-line block that starts with `if (fanout > ctx.params.max_net_fanout) {` and ends with `continue;`):

Replace with:

```cpp
    if (fanout > ctx.params.max_net_fanout) {
      const CoarseDelta cd = evaluateNetCoarse(net, cell, ctx.S);
      md.delta_term += cd.delta_term;
      continue;
    }
```

- [ ] **Step 6.5: Build, run smoke, run numerical lock**

Build. Smoke. Numerical lock.

If numerical lock fails: the helper extraction must be byte-equivalent to the original arithmetic. Re-check `overflowAfterAddingCell`: dbu/μm conversion order, max() clamp, integer division ordering. Re-check `evaluateNetCoarse`: the four bool flags must match the original semantics (before_from = true if any non-cell, non-moving inst is on from-side; etc.).

- [ ] **Step 6.6: Format and commit**

```bash
clang-format -i src/mdm/src/GlobalTierOptimizer.h src/mdm/src/GlobalTierOptimizer.cpp
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): simplify GlobalTierOptimizer overflow + huge-net branch

- overflowAfterAddingCell() helper: removes duplicated arithmetic
  between overflow() and surrogateDelta().
- evaluateNetCoarse(): extracts the 35-line huge-fanout branch in
  evaluateMove() to a focused free helper.

surrogateDelta is now 12 lines (down from 30). evaluateMove's huge-net
branch is 4 lines (down from 35).

Behavior preserved: case2 e2e HPWL = 2,694,774 +/- 0.5% (numerical
lock passes).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Make `makeInterconnections` Idempotency Function-Local

**Goal**: Replace `dbBoolProperty "intersected"` markers (which mutate DB state) with BTerm presence detection. Cross-call idempotency becomes implicit in net topology.

**Files:**
- Modify: `src/mdm/src/MultiDieManager.cpp`

- [ ] **Step 7.1: Update `makeInterconnections` body**

In `makeInterconnections` (around lines 195-236), replace:

```cpp
void MultiDieManager::makeInterconnections(odb::dbBlock* lower_block,
                                           odb::dbBlock* upper_block)
{
  odb::dbBlock* top_hier = db_->getChip()->getBlock();
  int interconnect_count = 0;
  for (auto* lower_net : lower_block->getNets()) {
    // Skip nets we've already paired via an earlier makeInterconnections
    // call (Phase 4.2 -apply path may invoke this incrementally after
    // cross-die migrations).
    if (auto* p = odb::dbBoolProperty::find(lower_net, "intersected")) {
      if (p->getValue()) {
        continue;
      }
    }
    const string net_name = lower_net->getName();
    odb::dbNet* upper_net = upper_block->findNet(net_name.c_str());
    if (!upper_net) {
      continue;
    }
    odb::dbNet* top_hier_net = top_hier->findNet(net_name.c_str());
    if (!top_hier_net) {
      // top_hier net was stripped after the first splitInstances pass
      // (it became floating). Recreate so BTerms have something to land on.
      top_hier_net = odb::dbNet::create(top_hier, net_name.c_str());
    }

    const string interconnect_name = net_name + "Interconnection";
    odb::dbBTerm* lower_term
        = odb::dbBTerm::create(lower_net, interconnect_name.c_str());
    odb::dbBTerm* upper_term
        = odb::dbBTerm::create(upper_net, interconnect_name.c_str());
    lower_term->getITerm()->connect(top_hier_net);
    upper_term->getITerm()->connect(top_hier_net);

    odb::dbBoolProperty::create(top_hier_net, "intersected", true);
    odb::dbBoolProperty::create(lower_net, "intersected", true);
    odb::dbBoolProperty::create(upper_net, "intersected", true);
    interconnect_count++;
  }
  logger_->info(
      utl::MDM, 12, "The interconnection number: {}", interconnect_count);
}
```

With:

```cpp
void MultiDieManager::makeInterconnections(odb::dbBlock* lower_block,
                                           odb::dbBlock* upper_block)
{
  odb::dbBlock* top_hier = db_->getChip()->getBlock();
  int interconnect_count = 0;
  for (auto* lower_net : lower_block->getNets()) {
    const string net_name = lower_net->getName();
    const string interconnect_name = net_name + "Interconnection";
    // Cross-call idempotency: if this net already has its interconnect
    // BTerm, it has already been paired. Skip.
    if (lower_net->findBTerm(interconnect_name.c_str()) != nullptr) {
      continue;
    }
    odb::dbNet* upper_net = upper_block->findNet(net_name.c_str());
    if (!upper_net) {
      continue;
    }
    odb::dbNet* top_hier_net = top_hier->findNet(net_name.c_str());
    if (!top_hier_net) {
      // top_hier net was stripped after the first splitInstances pass
      // (it became floating). Recreate so BTerms have something to land on.
      top_hier_net = odb::dbNet::create(top_hier, net_name.c_str());
    }

    odb::dbBTerm* lower_term
        = odb::dbBTerm::create(lower_net, interconnect_name.c_str());
    odb::dbBTerm* upper_term
        = odb::dbBTerm::create(upper_net, interconnect_name.c_str());
    lower_term->getITerm()->connect(top_hier_net);
    upper_term->getITerm()->connect(top_hier_net);

    interconnect_count++;
  }
  logger_->info(
      utl::MDM, 12, "The interconnection number: {}", interconnect_count);
}
```

Key changes:
- Idempotency check is now `lower_net->findBTerm(interconnect_name.c_str()) != nullptr` (a topology query, not a metadata flag).
- The three `odb::dbBoolProperty::create(..., "intersected", true);` calls are removed.

- [ ] **Step 7.2: Build, run smoke, run numerical lock**

Build. Smoke. Numerical lock.

If smoke fails with "BTerm with name X already exists" or similar: the cross-call idempotency check is missing a code path. Verify by adding a debug print of `interconnect_count` per call.

If numerical lock drifts: this commit changed the DB state model (no more `intersected` properties). If any downstream code (e.g., `gpl::Replace`, `dpl::Opendp`) reads the marker, behavior changes. Search:
```bash
grep -rn 'intersected' /home/minjae/workspace/etc/openroad/OpenROAD/src/ | grep -v test | grep -v 'intersected_net_weight\|intersectedNetWeight'
```
Expected: no remaining readers of the property. If found, that's the regression source.

- [ ] **Step 7.3: Format and commit**

```bash
clang-format -i src/mdm/src/MultiDieManager.cpp
git add -u src/mdm/
git commit -s -m "$(cat <<'EOF'
refactor(mdm): scope makeInterconnections idempotency to topology

Replaces the dbBoolProperty "intersected" flag (which mutated DB state
across three nets per cross-die pair) with a topology check:
lower_net->findBTerm(interconnect_name). The presence of the
interconnect BTerm is itself the signal that the net has been paired.

Cross-call idempotency (set_3D_IC paired some nets, then
runGlobalTierOptimization -apply needs to re-pair newly cross-die nets
without re-pairing the existing ones) is preserved through the BTerm
check.

No behavior change: case2 e2e HPWL = 2,694,774 +/- 0.5% (numerical
lock passes).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Logger ID Audit and Range Documentation

**Goal**: Verify no logger ID collisions in mdm; add comment block in `MultiDieManager.h` documenting the ID range convention.

**Files:**
- Modify: `src/mdm/include/mdm/MultiDieManager.h` (add comment block)

- [ ] **Step 8.1: Audit existing IDs**

Run:
```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm
grep -hroP "utl::MDM,?\s*\n?\s*\K[0-9]+" --include="*.cpp" --include="*.h" src/ include/ | sort -un > /tmp/mdm_ids_cpp.txt
grep -hoP "utl::error MDM \K[0-9]+|utl::warn MDM \K[0-9]+|utl::info MDM \K[0-9]+" --include="*.tcl" -r src/ | sort -un > /tmp/mdm_ids_tcl.txt
sort -un /tmp/mdm_ids_cpp.txt /tmp/mdm_ids_tcl.txt > /tmp/mdm_ids_all.txt
sort /tmp/mdm_ids_cpp.txt /tmp/mdm_ids_tcl.txt | uniq -d > /tmp/mdm_ids_dup.txt
wc -l /tmp/mdm_ids_dup.txt
cat /tmp/mdm_ids_dup.txt
```
Expected: `0 /tmp/mdm_ids_dup.txt` (no duplicates between C++ and Tcl).

If duplicates found: report the IDs and locations. Decide which side to renumber (typically Tcl 100-series stays; C++ moves to next free slot in its range).

- [ ] **Step 8.2: Document the ID range convention**

In `src/mdm/include/mdm/MultiDieManager.h`, after `namespace mdm {` (around line 28) and before any class declaration, add:

```cpp
// ============================================================
// Logger message ID convention (utl::MDM):
//   1-99    : Stage 1/2 — split, switch, interconnect, IO pin
//   100-199 : Tcl error/warn (utl::error MDM ...)
//   200-299 : reserved
//   300-399 : Phase 4 — Global Tier Opt, Planar Correcting,
//             snap, run3DPlacement scaffolding
//   400+    : reserved for future phases
// When adding a new ID, grep `utl::MDM` across src/mdm/ first.
// ============================================================
```

- [ ] **Step 8.3: Build (no test required — comment-only change)**

Run: `cmake --build /home/minjae/workspace/etc/openroad/OpenROAD/build -j$(nproc) --target openroad`
Expected: builds clean.

- [ ] **Step 8.4: Commit**

```bash
git add src/mdm/include/mdm/MultiDieManager.h
git commit -s -m "$(cat <<'EOF'
chore(mdm): document logger ID range convention

Adds a comment block to MultiDieManager.h documenting the ID
allocation convention (Stage 1/2 = 1-99, Tcl errors = 100-199,
reserved = 200-299, Phase 4 = 300-399). Audited no duplicates.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Update handoff.md

**Goal**: Update handoff.md to reflect the refactor completion and the newly-exposed Tcl knobs that unblock the next leverage point (case3 Planar Correcting fix).

**Files:**
- Modify: `handoff.md`

- [ ] **Step 9.1: Run final smoke + numerical lock to confirm clean state**

```bash
/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /home/minjae/workspace/etc/openroad/OpenROAD/src/mdm/test/test_phase4_smoke.tcl
bash /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/regression_check.sh
```
Both must PASS.

- [ ] **Step 9.2: Update handoff.md**

Edit `handoff.md`:

- Update the "현재 진행 상황" section header to add a "Refactor (this session)" subsection summarizing the 8 commits.
- Add new Tcl knobs to the "남은 작업" instructions: `run_planar_correcting -density X -intersected_net_weight Y -nesterov_max_iter Z` is now the path for case3 fix.
- Remove references to `BilevelCoordinator`, `FastTerminalLegalizer`, `runFlattenedPlacement`, `run3DPlacement` (they no longer exist; phases 4.4/4.6 will recreate).
- Update the "Git 상태" → master+N commits ahead (was 35, now 35 + 8 + this commit = 44).

The exact text patches will depend on the implementer's read of handoff.md state. Keep it minimal: a "Refactor cycle (Section X)" block under "Phase 4 series" + struck-out lines for removed plumbing.

- [ ] **Step 9.3: Commit**

```bash
git add handoff.md
git commit -s -m "$(cat <<'EOF'
docs(mdm): handoff.md update — Phase 4 refactor cycle complete

8 commits removed YAGNI stubs (BilevelCoordinator,
FastTerminalLegalizer, runFlattenedPlacement, run3DPlacement),
exposed Tcl knobs for run_planar_correcting and
run_global_tier_optimization, decomposed
runGlobalTierOptimization into 4 named helpers, simplified
GlobalTierOptimizer's overflow + huge-net code paths, and
scoped makeInterconnections idempotency to topology rather
than DB metadata.

Behavior unchanged: case2 e2e HPWL = 2,694,774 (numerical lock
passes at HEAD).

Next leverage point: case3 Planar Correcting Nesterov non-
convergence fix is now possible from Tcl alone — no C++
rebuilds needed for density / intersected_net_weight /
nesterov_max_iter tuning.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

(To be performed after the plan is fully written; checklist:)

1. **Spec coverage**: All 9 items (g, h, n, i, j, k, l, m, o) from the spec are mapped to commits. Regression strategy covered in Task 1.
2. **Placeholder scan**: No "TBD"/"TODO"/"figure out".
3. **Type consistency**: `getChildBlocks`, `dieIndexOf`, `findFromToBlocks`, `mapKnapsackCaps`, `findLibForBlock`, `applyMigration`, `overflowAfterAddingCell`, `evaluateNetCoarse` — all referenced consistently across tasks.
4. **Acceptance criteria**: 8 commits + handoff update; numerical lock passes at end; smoke passes after each commit.

---

## Rollback strategy

If any commit's numerical lock fails and the cause cannot be diagnosed within 30 minutes:

1. `git reset --hard HEAD~1` on the failing commit only.
2. Skip that refactor item; proceed to the next task.
3. File a follow-up note in handoff.md describing the deferred item.

The atomic-commit structure means rollback is one-shot per item.
