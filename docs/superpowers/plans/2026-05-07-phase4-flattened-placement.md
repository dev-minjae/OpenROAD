# Phase 4.4 — Flattened Placement (paper §IV.A) 자체 구현

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** paper iPL-3D §IV.A "Flattened Placement" 단계를 자체 구현. `run_flattened_placement` Tcl command 부활 + 실제 동작 채우기. 우리 `gpl::Replace` 를 root block (set_3D_IC 전, 모든 cells 가 단일 die 처럼) 에 적용해서 cells 의 2D 좌표 자체 생성. paper coord (Xueyan reference) 빌리는 의존성 제거의 첫 단계.

**Architecture:** 이전 cycle 에서 YAGNI 로 stub 삭제됨 (`22e8df272d`). 이번엔 정식 구현.
- Tcl: `run_flattened_placement [-density d] [-target_density td] [-nesterov_max_iter n] [-no_skip_io_mode]`
- C++: `MultiDieManager::runFlattenedPlacement(...)` 가 `gpl::Replace::doInitialPlace` + `doNesterovPlace` 호출. cells 좌표 db 에 반영.
- 측정: case3 / case4 에서 `parse_iccad2022_output` (paper coord 빌림) 대신 `run_flattened_placement` 사용. HPWL 측정. paper Table I "Flattened GP HPWL" 컬럼 + 우리 set_3D_IC 직후 (Xueyan 빌릴 때) HPWL 과 3-way 비교.

**Tech Stack:** OpenROAD MDM (C++ + Tcl + SWIG), `gpl::Replace::doInitialPlace` / `doNesterovPlace`, ICCAD 2022 evaluator, paper Table I 비교 데이터.

**Spec:** 이번 cycle 은 brainstorming skip + 바로 plan. spec 별도 없음. paper §IV.A + §IV 의 Theorem 1 이 design 정당화 ("Flattened placement is a lower bound of the D2D placement problem; our framework uses it as initial solution").

**Out of scope:**
- §IV.B Global Tier Optimization 알고리즘 자체 quality 개선 — 별도 cycle (이번엔 우리 기존 GTO 그대로 사용)
- §IV.D Planar Correcting algorithm 미세 차이 — 우리 기존 wrapper 그대로
- §IV.F Multi-Tier Placement 신규 구현 — 다음 cycle
- Bilevel Coordinator (Algorithm 1) M=4 alternating — 다음 cycle
- 자체 frontend 의 quality 개선 (이번 cycle 은 *동작* 부터)

**Success criteria:**
1. `run_flattened_placement` Tcl command 가 호출 가능 (smoke test exit 0)
2. case2/3/4 모두에서 `parse_iccad2022_output` 대신 `run_flattened_placement` 로 시작해서 e2e flow (set_3D_IC → GTO → SemiLeg → write) 가 끝까지 돌아감
3. paper Table I 의 "Flattened GP HPWL" 와 비교 가능한 측정값 확보
4. 측정값을 `docs/agents/phase4-self-frontend-measurement.md` 에 정리
5. handoff §4 에 다음 leverage point 명확히 (격차 따라 §IV.F 또는 §IV.B 우선순위 결정)

---

## File Structure

수정 (이전 stub 코드 부활 + 실제 구현):
- `src/mdm/include/mdm/MultiDieManager.h` — `runFlattenedPlacement` declaration 추가
- `src/mdm/src/MultiDieManager.cpp` — 실제 구현
- `src/mdm/src/MultiDieManager.tcl` — `run_flattened_placement` Tcl proc 부활
- `src/mdm/src/MultiDieManager.i` — SWIG binding 부활

신규:
- `src/mdm/test/regression_phase4_self_case2.tcl` — self-frontend 측정 (paper 빌림 없음)
- `src/mdm/test/regression_phase4_self_case3.tcl`
- `src/mdm/test/regression_phase4_self_case4.tcl`
- `docs/agents/phase4-self-frontend-measurement.md` — 측정 결과 + 다음 leverage point

수정 (smoke test 도 새 command 들어감):
- `src/mdm/test/test_phase4_smoke.tcl` — `run_flattened_placement` 호출 케이스 추가

handoff.md 갱신.

---

## Task 1: Tcl proc + SWIG binding 부활 (이전 stub 의 *interface* 만, 구현 X)

**Files:**
- Modify: `src/mdm/src/MultiDieManager.tcl`
- Modify: `src/mdm/src/MultiDieManager.i`
- Modify: `src/mdm/include/mdm/MultiDieManager.h`

이전 stub 의 interface 를 *그대로* 부활. 단 추가 옵션 (`-target_density`, `-nesterov_max_iter`, `-no_skip_io_mode`) 도 미리 노출.

- [ ] **Step 1: Tcl proc 추가 (`MultiDieManager.tcl` 의 적절한 위치)**

`run_planar_correcting` proc 위, `import_inst_coordinates` 아래에 추가:

```tcl
# Phase 4 — iPL-3D paper §IV.A flattened initial placement.
# Runs gpl::Replace on the root block (single-die assumption) to generate
# cells' 2D coords without depending on paper-equivalent reference output.
sta::define_cmd_args "run_flattened_placement" {\
    [-density density] \
    [-target_density target_density] \
    [-nesterov_max_iter n] \
    [-no_skip_io_mode]}

proc run_flattened_placement { args } {
  sta::parse_key_args "run_flattened_placement" args \
    keys {-density -target_density -nesterov_max_iter} \
    flags {-no_skip_io_mode}

  # paper §IV.A Theorem 1: doubling bin density threshold ≈ target_density 2.0
  set density 1.0
  set target_density 2.0
  set nesterov_max_iter 5000
  set skip_io_mode 1
  if { [info exists keys(-density)] } {
    set density $keys(-density)
  }
  if { [info exists keys(-target_density)] } {
    set target_density $keys(-target_density)
  }
  if { [info exists keys(-nesterov_max_iter)] } {
    set nesterov_max_iter $keys(-nesterov_max_iter)
  }
  if { [info exists flags(-no_skip_io_mode)] } {
    set skip_io_mode 0
  }
  mdm::run_flattened_placement $density $target_density $nesterov_max_iter \
                                $skip_io_mode
}
```

- [ ] **Step 2: SWIG binding (`MultiDieManager.i`) 추가**

`run_planar_correcting` SWIG 함수 아래 또는 위에 추가 (파일에 매뉴얼 편집, clang-format 금지):

```c
void
run_flattened_placement(double density,
                        double target_density,
                        int nesterov_max_iter,
                        bool skip_io_mode)
{
  getMultiDieManager()->runFlattenedPlacement(density,
                                              target_density,
                                              nesterov_max_iter,
                                              skip_io_mode);
}
```

- [ ] **Step 3: header declaration (`MultiDieManager.h`) 추가**

`runPlanarCorrecting` declaration 근처에 추가:

```cpp
  // Phase 4 — iPL-3D paper §IV.A flattened initial placement. Runs
  // gpl::Replace::doInitialPlace + doNesterovPlace on the root block
  // (single-die assumption per Theorem 1: doubling density threshold).
  // Cells' 2D coords get populated in place; subsequent set_3D_IC + GTO
  // can run without parse_iccad2022_output (no paper reference dependency).
  void runFlattenedPlacement(double density,
                             double target_density,
                             int nesterov_max_iter,
                             bool skip_io_mode);
```

- [ ] **Step 4: `MultiDieManager.cpp` 에 stub 구현 추가 (구현 자체는 Task 2 에서 채움)**

`runPlanarCorrecting` 함수 위에 stub 추가:

```cpp
void MultiDieManager::runFlattenedPlacement(double density,
                                            double target_density,
                                            int nesterov_max_iter,
                                            bool skip_io_mode)
{
  logger_->info(utl::MDM,
                304,
                "runFlattenedPlacement: stub (Task 1). density={}, "
                "target_density={}, nesterov_max_iter={}, skip_io_mode={}.",
                density,
                target_density,
                nesterov_max_iter,
                skip_io_mode);
}
```

- [ ] **Step 5: 빌드 확인**

```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
cmake --build build -j --target openroad 2>&1 | tail -10
```
Expected: build clean, 마지막 줄 `[100%] Built target openroad`.

- [ ] **Step 6: smoke check — Tcl command 가 호출되는지**

```bash
cat <<'EOF' > /tmp/test_flat_smoke.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
run_flattened_placement
exit
EOF
build/bin/openroad -no_init -no_splash -exit /tmp/test_flat_smoke.tcl 2>&1 | tail -10
```
Expected: stub log line `[INFO MDM-0304] runFlattenedPlacement: stub (Task 1)...` 가 보임. exit 0.

- [ ] **Step 7: Commit**

```bash
git add src/mdm/include/mdm/MultiDieManager.h
git add src/mdm/src/MultiDieManager.cpp
git add src/mdm/src/MultiDieManager.tcl
git add src/mdm/src/MultiDieManager.i
git -c commit.gpgsign=false commit -s -m "feat(mdm): Phase 4.4 stub — runFlattenedPlacement Tcl/SWIG/header"
```

---

## Task 2: `runFlattenedPlacement` 실제 구현 (gpl::Replace 호출)

**Files:**
- Modify: `src/mdm/src/MultiDieManager.cpp:[runFlattenedPlacement function]`

paper §IV.A Theorem 1 의 핵심: "assigning all cells into one die and doubling the threshold of bin density". 즉 단일 die 가정 + density threshold × 2.

`gpl::PlaceOptions` 의 멤버:
- `density` (default 0.7) — bin density target. 우리는 *2.0* 으로 설정 (single-die 시 doubling)
- `nesterovPlaceMaxIter` (default 5000)
- `skipIoMode` (default false) — 우리 다른 호출처 처럼 true 추천 (post-place 가 아닌 placement 단계)
- `forceCenterInitialPlace` 등 — default 사용

`gpl::Replace::doInitialPlace` 가 unplaced cells 에 random + CG-based initial placement 부여. 그 후 `doNesterovPlace` 가 Nesterov global placement.

- [ ] **Step 1: `runFlattenedPlacement` 구현**

`MultiDieManager.cpp` 의 stub 을 다음으로 교체:

```cpp
void MultiDieManager::runFlattenedPlacement(double density,
                                            double target_density,
                                            int nesterov_max_iter,
                                            bool skip_io_mode)
{
  if (!replace_) {
    logger_->error(
        utl::MDM, 304, "runFlattenedPlacement: gpl::Replace pointer is null.");
    return;
  }
  // Per paper §IV.A Theorem 1: single-die assumption with doubled bin
  // density threshold. We map this to gpl::PlaceOptions.density (target
  // bin density). Default target_density=2.0 reflects the doubling.
  // density (param) is the design-level density target, currently passed
  // through but unused by gpl::PlaceOptions directly (kept for future).
  (void) density;
  gpl::PlaceOptions opts;
  opts.density = target_density;
  opts.nesterovPlaceMaxIter = nesterov_max_iter;
  opts.skipIoMode = skip_io_mode;
  // Initial placement (CG-based) gives cells starting coords. doNesterovPlace
  // then refines via Nesterov gradient descent. Cells' coords are written
  // back to the dbBlock in place.
  logger_->info(utl::MDM,
                304,
                "runFlattenedPlacement: starting (target_density={}, "
                "nesterov_max_iter={}, skip_io_mode={}).",
                target_density,
                nesterov_max_iter,
                skip_io_mode);
  replace_->doInitialPlace(/*threads=*/1, opts);
  const int final_iter = replace_->doNesterovPlace(/*threads=*/1, opts);
  logger_->info(utl::MDM,
                305,
                "runFlattenedPlacement: done. Nesterov final iter={}.",
                final_iter);
}
```

- [ ] **Step 2: 빌드 확인**

```bash
cmake --build build -j --target openroad 2>&1 | tail -5
```
Expected: build clean.

- [ ] **Step 3: smoke check — case2 에서 실제 실행 확인**

```bash
cat <<'EOF' > /tmp/test_flat_smoke.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
run_flattened_placement -nesterov_max_iter 200
puts "=== HPWL after flattened placement ==="
get_3d_hpwl
exit
EOF
build/bin/openroad -no_init -no_splash -exit /tmp/test_flat_smoke.tcl > /tmp/test_flat_smoke.log 2>&1
echo "exit=$?"
tail -30 /tmp/test_flat_smoke.log
```
Expected: exit 0. log 에 `[INFO MDM-0304] runFlattenedPlacement: starting`, `[INFO GPL-0007] ---- Execute Nesterov Global Placement.`, Nesterov iteration log, `[INFO MDM-0305] runFlattenedPlacement: done. Nesterov final iter=...`, 마지막 `HPWL is: <number>`.

이 단계에서 gpl::Replace 가 우리 mdm root block 에서 *실제 동작하는지* 확인. 만약 fail 하면 (예: "movable instances: 0") 우리 read_iccad2022 가 db 를 어떻게 만드는지 추가 디버그 필요. 그 경우 step 4 로 진행 전에 stop.

- [ ] **Step 4: HPWL sanity check — paper Flattened GP HPWL 와 자릿수 비교**

case2 의 경우 paper Flattened GP HPWL = 1,758,214. 우리 측정값이 1.5M~5M 범위 안이면 OK. 너무 작거나 (ex: 0) 너무 큼 (ex: 100M) 은 비정상.

이 단계 PASS 면 다음 task. FAIL 면 stop, gpl::Replace 와 우리 root block 의 호환성 추가 디버그.

- [ ] **Step 5: Commit**

```bash
git add src/mdm/src/MultiDieManager.cpp
git -c commit.gpgsign=false commit -s -m "feat(mdm): Phase 4.4 implement runFlattenedPlacement (gpl::Replace wrapper)"
```

---

## Task 3: case2/3/4 self-frontend regression scripts 작성

**Files:**
- Create: `src/mdm/test/regression_phase4_self_case2.tcl`
- Create: `src/mdm/test/regression_phase4_self_case3.tcl`
- Create: `src/mdm/test/regression_phase4_self_case4.tcl`

기존 regression script 와 다른 점: `parse_iccad2022_output` (paper coord 빌림) 대신 `run_flattened_placement` 사용. partition 도 자체 결정 (`set_mdm_partition_file` 안 씀; `run_global_tier_optimization` 가 partition 결정).

근데 단순화 위해 — *partition 은* `set_mdm_partition_file -file /tmp/flat_caseN.par` (모든 cells die 0) 으로 강제 + GTO 가 일부 migrate. 즉 self-flat-init + 우리 GTO. 이게 *완전* 자체 frontend.

- [ ] **Step 1: case2 self-frontend script 작성**

```tcl
# src/mdm/test/regression_phase4_self_case2.tcl
# Phase 4.4 self-frontend — no parse_iccad2022_output (paper coord borrow).
# Self-contained pipeline: read → flat 2D GP → flat partition → GTO → SemiLeg.
# No baseline locked yet (this script is for measurement, not a numerical
# lock). Compare against paper Table I "Flattened GP HPWL" and the existing
# Xueyan-borrowed regression baseline.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

# All cells start on die 0; GTO migrates some to die 1.
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_self_case2.par
set_mdm_partition_file -file /tmp/flat_self_case2.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case2.out
exit
```

awk 의 source 가 `/tmp/case2_ipl.out.par` 인 이유: cell ID 목록만 빌리고 partition 값은 0 으로 강제. paper coord 자체는 빌리지 않음 (parse_iccad2022_output 호출 X).

- [ ] **Step 2: case3 self-frontend script 작성**

같은 패턴, case3 paths:

```tcl
# src/mdm/test/regression_phase4_self_case3.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_self_case3.par
set_mdm_partition_file -file /tmp/flat_self_case3.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

# Skip Planar for case3 (dense regime — see prior regression_phase4_case3.tcl)
puts "=== Skipping Planar Correcting (case3 dense) ==="

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case3.out
exit
```

- [ ] **Step 3: case4 self-frontend script 작성**

같은 패턴, case4 paths. case3 처럼 Skip Planar (case4 도 Skip 이 winner 라 이전 cycle 측정).

```tcl
# src/mdm/test/regression_phase4_self_case4.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

exec awk "{print \$1, 0}" /tmp/ref_gp/case4_gp.txt.par > /tmp/flat_self_case4.par
set_mdm_partition_file -file /tmp/flat_self_case4.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

puts "=== Skipping Planar Correcting (case4 — Skip winner per prior cycle) ==="

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case4.out
exit
```

- [ ] **Step 4: Commit (스크립트 3개 함께)**

```bash
git add src/mdm/test/regression_phase4_self_case2.tcl
git add src/mdm/test/regression_phase4_self_case3.tcl
git add src/mdm/test/regression_phase4_self_case4.tcl
git -c commit.gpgsign=false commit -s -m "test(mdm): Phase 4.4 self-frontend regression scripts (case2/3/4)"
```

---

## Task 4: 3 케이스 self-frontend 측정 + evaluator 결과

**Files:**
- Run: `regression_phase4_self_case{2,3,4}.tcl`

- [ ] **Step 1: 3 케이스 병렬 실행**

```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
OPENROAD=$(pwd)/build/bin/openroad
for case in case2 case3 case4; do
    $OPENROAD -no_init -no_splash -exit src/mdm/test/regression_phase4_self_${case}.tcl > /tmp/regression_phase4_self_${case}_run.log 2>&1 &
done
wait
echo "all 3 done"
ls -la /tmp/regression_phase4_self_*.out
```
Expected: 3 `.out` files. case2 ~5 분, case3 ~10 분, case4 ~30 분 (자체 GP + GTO + SemiLeg 모두 돌림).

만약 어느 case 가 fail 하면 (gpl::Replace divergence, set_3D_IC 후 cells 정합 issue 등) stop. 그 case 의 log 분석 + Task 2 의 implementation 디버그 필요.

- [ ] **Step 2: evaluator 로 final HPWL 측정**

```bash
EVALUATOR=/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525
ARCHIVE=/home/minjae/workspace/etc/openroad/archive/3d_ic
for case in case2 case3 case4; do
    echo "=== $case self-frontend ==="
    $EVALUATOR $ARCHIVE/benchmarks/iccad2022/$case.txt /tmp/regression_phase4_self_$case.out 2>&1 | tail -3
done
```
Expected: 각 case 마다 `Total HPWL for this design is <N>` 출력. 절대 오류 (Util > Max Util, overlap 등) 없어야.

- [ ] **Step 3: trajectory 추출 (각 단계 별 HPWL)**

```bash
for case in case2 case3 case4; do
    echo "=== $case trajectory ==="
    grep -E "HPWL after|HPWL is|interconnect|TerminalLegalizer" /tmp/regression_phase4_self_${case}_run.log
done
```

각 case 의 4-5 단계 HPWL 모음:
1. flattened placement 후
2. set_3D_IC 후
3. GTO 후
4. (Planar 후, case2 만)
5. SemiLeg 후
6. evaluator 후 (TermLeg 포함)

---

## Task 5: 측정 결과 문서 작성

**Files:**
- Create: `docs/agents/phase4-self-frontend-measurement.md`

- [ ] **Step 1: 비교 표 작성**

```markdown
# Phase 4.4 — Self-Frontend Measurement

작성일: 2026-05-07.

paper §IV.A Flattened Placement 자체 구현 (`run_flattened_placement`) 후
case2/3/4 e2e 측정. paper Table I "Flattened GP HPWL" + 이전 cycle Xueyan-
borrowed baseline 과 3-way 비교.

## Trajectory 표

| 단계 | case2 self | case2 W1 (Xueyan) | case3 self | case3 W1 (Xueyan) | case4 self | case4 W1 (Xueyan) |
|---|---|---|---|---|---|---|
| flattened placement 후 | <Step 3 결과> | (parse 결과 1,986,626) | <...> | (27,272,257) | <...> | (~245M) |
| set_3D_IC 후 | <...> | 2,000,861 | <...> | 27,479,632 | <...> | 247,400,369 |
| GTO 후 | <...> | (Skip GTO in W1) | <...> | (Skip GTO in W1) | <...> | (Skip GTO in W1) |
| Planar 후 (case2 only) | <...> | 1,866,948 | — | — | — | — |
| SemiLeg 후 | <...> | 1,991,164 | <...> | 30,123,098 | <...> | 265,088,413 |
| evaluator (TermLeg 포함) | <...> | 2,004,424 | <...> | 30,229,424 | <...> | 265,734,241 |

## paper Table I 와의 비교

| Case | self-frontend final | Xueyan-borrowed final | paper Flattened GP | paper "Ours" |
|---|---|---|---|---|
| case2 | <Step 2 결과> | 2,004,424 | 1,758,214 | 1,992,499 |
| case3 | <...> | 30,229,424 | 26,474,613 | 30,234,112 |
| case4 | <...> | 265,734,241 | 248,129,463 | 267,381,744 |

self-frontend 의 paper 격차 = (self-frontend final ÷ paper Ours − 1) × 100%.

## Conclusion

(Step 1 채우면서 데이터 분석 결과 작성. 3 case 모두 measurable 면 다음 leverage point 결정:
1. self-frontend final 이 paper Ours 의 +X% 범위 — X 값 따라 다음 단계 우선순위
2. 우리 flattened placement 의 quality (paper Flattened GP 와 비교) — gpl::Replace 가 paper 의 자체 GP algorithm 과 얼마나 비슷한가
3. SemiLeg overhead 가 self-frontend 에서 어떻게 변하는가 — 이전 cycle 진단의 die cluster 가 reproduce 되는지 검증)
```

- [ ] **Step 2: Step 1 의 표를 Task 4 의 측정값으로 채움**

각 `<...>` placeholder 를 실제 측정값으로 대체. handoff §4 갱신 권고 작성.

---

## Task 6: handoff.md 갱신 + 최종 commit

**Files:**
- Modify: `handoff.md`
- Create: `docs/agents/phase4-self-frontend-measurement.md` (Task 5 결과)

- [ ] **Step 1: handoff.md `§2 검증 상태` 에 self-frontend 측정 추가**

기존 W1 baseline (paper-borrowed) 표 옆에 self-frontend 표 추가. 두 측정 모두 보존 (W1 = upper bound 의 reference, self = 자력 quality).

- [ ] **Step 2: handoff.md `§4 다음 leverage point` 갱신**

Task 5 conclusion 기반 우선순위 재정렬. 후보:
- self-frontend 가 paper Ours +5% 안: paper §IV.F Multi-Tier Placement 신규 (paper 추월 잡기 위한 자력 마지막 단계)
- self-frontend 가 paper Ours +10-30% 차이: §IV.B GTO quality 개선 (archive reference_impl 비교 + 우리 cluster 줄이기)
- self-frontend 가 paper Ours +30% 이상: gpl::Replace 의 single-block 호환성 디버그 (우리 flat 2D GP 의 starting quality 가 paper Flattened GP 와 너무 다름)

- [ ] **Step 3: smoke test 갱신**

`src/mdm/test/test_phase4_smoke.tcl` 에 `run_flattened_placement` 호출 추가:

```tcl
# Append to test_phase4_smoke.tcl, before exit:
puts "=== smoke 4: run_flattened_placement ==="
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
run_flattened_placement -nesterov_max_iter 100
puts "=== HPWL after flat ==="
get_3d_hpwl
```

기존 case3 smoke section 수정해야 할 수도. 현재 smoke 가 어떻게 구성되었는지 확인 필요. 현재 smoke 가 read_iccad2022 호출 안 하면 새 case 를 위 처럼 추가, 호출 하면 기존 read 후에 reset 필요. (smoke test 가 새로운 case 를 다시 read 하는 것은 OpenROAD 가 multi-design 을 지원하는지 의존; 일단 같은 design 위에 호출만 추가, 또는 별개 OpenROAD 호출로 분리.)

`test_phase4_smoke.tcl` 직접 확인 후 적절히 통합.

- [ ] **Step 4: 모든 변경 stage + 최종 commit**

```bash
git add docs/agents/phase4-self-frontend-measurement.md
git add handoff.md
git add src/mdm/test/test_phase4_smoke.tcl
git -c commit.gpgsign=false commit -s -m "$(cat <<'EOF'
feat(mdm): Phase 4.4 measurement — self-frontend HPWL across case2/3/4

Self-frontend = run_flattened_placement instead of parse_iccad2022_output.
First measurement of our own pipeline without paper coord borrow. Compared
against paper Table I "Flattened GP HPWL" + Xueyan-borrowed W1 baseline.

  case2 self: <X> (paper Ours 1,992,499; gap +<Y>%)
  case3 self: <X> (paper Ours 30,234,112; gap +<Y>%)
  case4 self: <X> (paper Ours 267,381,744; gap +<Y>%)

Smoke test extended with run_flattened_placement call. handoff §4 next
leverage point refreshed based on gap distribution: <decision>.
EOF
)"
git log --oneline -8
```

---

## Self-Review

**Spec coverage:**
- ✅ paper §IV.A Flattened Placement 자체 구현 — Task 2
- ✅ Tcl/SWIG/header interface 부활 — Task 1
- ✅ case2/3/4 self-frontend 측정 — Task 3, 4
- ✅ paper Table I 와의 비교 — Task 5
- ✅ Out of scope (§IV.B/D quality 개선, §IV.F, Bilevel) 미포함 — 다음 cycle
- ✅ Success criteria 5개 모두 — Task 1 Step 6 (criterion 1), Task 4 Step 1 (criterion 2), Task 4 Step 2 + Task 5 (criterion 3, 4), Task 6 Step 2 (criterion 5)

**Placeholder scan:**
- Task 5 Step 1, Task 6 Step 4 의 `<...>` placeholder 는 *측정 결과를 채워야 할 자리* — 의도된 빈자리. Plan 자체의 placeholder 아님.
- 모든 step 에 actual code/command/file path 포함. "TBD" 없음.

**Type consistency:**
- `runFlattenedPlacement` signature 는 `(double, double, int, bool)` — Task 1, 2 에서 일치
- `gpl::PlaceOptions::density` (target bin density), `nesterovPlaceMaxIter`, `skipIoMode` — Task 2 의 필드 모두 검증된 PlaceOptions 멤버
- Tcl `-density` / `-target_density` / `-nesterov_max_iter` / `-no_skip_io_mode` — Task 1 Step 1 의 proc 와 Task 1 Step 2 의 SWIG signature 일치

**Risks:**
- gpl::Replace 가 우리 mdm root block 에서 *동작 안 할 수도*. 이전 cycle W2 시도 (raw coord + paper partition + Planar) 가 "Movable instances: 0" crash 했음. 그 case 의 root cause = `parse_iccad2022_output` 안 했음 + cells unplaced. 이번엔 `run_flattened_placement` 자체가 placement 만들므로 회피 가능. 단 *gpl::Replace::doInitialPlace* 가 unplaced cells 에서 시작해 placement 만드는지 확인 필요. 확인 못 됐으면 Task 2 Step 3 에서 발견될 것 — 그 때 stop + 디버그.
- gpl::PlaceOptions 의 `density` 필드 의미 확인. paper 의 "doubling bin density threshold" 와 정확히 매칭 안 될 수도. 해석 차이 있으면 Task 2 Step 4 의 HPWL sanity check 결과로 발견.

수정 필요 없음. 진행 가능.
