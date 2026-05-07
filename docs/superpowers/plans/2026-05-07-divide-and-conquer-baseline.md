# Divide-and-Conquer Baseline — paper Frontend 차용 + Backend 차별화

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Xueyan 의 post-GP 데이터 (positions + partition) 를 paper-equivalent frontend 로 채택하여 우리 backend (SemiLeg + TermLeg) 단독 성능을 paper Table I "Ours" 컬럼과 비교 가능한 기준점에 둠. 이번 cycle에서 case2/3/4 모두 새 baseline 록인하고, 남은 격차의 진짜 원인 (backend 어느 step) 을 진단하여 다음 cycle에서 paper 추월할 leverage point 확정.

**Architecture:** 기존 regression flow 는 `flat init → GTO → Planar → SemiLeg` 으로 우리 알고리즘 전 구간을 측정. 이 flow는 frontend (GTO) 의 격차가 backend 측정에 섞임. 새 flow `set_3D_IC (Xueyan post-GP load) → [Planar?] → SemiLeg → TermLeg` 는 frontend 를 paper 와 등가로 고정시켜 backend 단독 측정 가능. case2 검증 결과 우리 backend 단독은 paper +0.7% 만 차이 (2,006,967 vs 1,992,499) — 충분히 paper 추월 가능 거리.

**Tech Stack:** OpenROAD MDM (`src/mdm/`), Tcl regression scripts (`src/mdm/test/regression_phase4_*.tcl`), bash regression check (`src/mdm/test/regression_check.sh`), ICCAD 2022 evaluator (`/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`), Xueyan reference data (`/tmp/case2_ipl.out{,.par}`, `/tmp/ref_gp/case3_gp.txt{,.par}`, `/tmp/ref_gp/case4_gp.txt{,.par}`).

**Out of scope:**
- Backend 알고리즘 자체 개선 (CellsLegalizer wirelength-aware refactor 등) — 다음 cycle
- Multi-Tier Placement (paper §IV.F) 신규 구현 — 다음 cycle
- `runFlattenedPlacement` / `BilevelCoordinator` 재추가 — 별개 phase

---

## File Structure

### 새로 만들/덮어쓸 파일

| 경로 | 책임 |
|---|---|
| `src/mdm/test/regression_phase4_case2.tcl` | **덮어쓰기**. Xueyan post-GP load → optional Planar → SemiLeg → write |
| `src/mdm/test/regression_phase4_case3.tcl` | **덮어쓰기**. 위와 같은 패턴 (case3) |
| `src/mdm/test/regression_phase4_case4.tcl` | **신규**. 위와 같은 패턴 (case4) |
| `src/mdm/test/regression_check.sh` | **수정**. case4 baseline 매핑 추가 |
| `docs/agents/phase4-divide-and-conquer.md` | **신규**. divide-and-conquer 전략 + paper 격차 분석 + 다음 leverage points |
| `handoff.md` | **수정**. 이번 cycle 결과 요약, 다음 leverage point 명시 |

### 보존할 파일 (이번 cycle 미변경)
- `src/mdm/src/MultiDieManager.{cpp,h,tcl,i}` — Tcl knobs / API 그대로
- `src/mdm/src/{GlobalTierOptimizer,PlanarCorrecting via runPlanarCorrecting}` — 알고리즘 자체 그대로
- `src/mdm/test/test_phase4_smoke.tcl` — smoke test 그대로

---

## Task 1: case3 post-GP 측정 데이터 수집 (in-progress)

**Files:**
- Read: `/tmp/case3_post_gp_run.log` (Skip Planar 변형, 진행 중)
- Read: `/tmp/case3_post_gp_with_planar_run.log` (with Planar 변형, 진행 중)
- Read: `/tmp/case3_post_gp.out`, `/tmp/case3_post_gp_with_planar.out` (evaluator 입력)

**의도:** case3 에 대해 Xueyan post-GP 출발점에서 SemiLeg-only vs Planar+SemiLeg 결과 측정. paper Table I case3 Ours = **30,234,112** 와 비교.

- [ ] **Step 1: 두 background job 완료 대기 + 결과 추출**

```bash
# 두 job 모두 ps에서 사라질 때까지 대기 후
EVALUATOR=/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525
CASE3=/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
echo "=== case3 post-GP Skip ===" && grep -E "HPWL is" /tmp/case3_post_gp_run.log
echo "=== case3 post-GP Skip evaluator ===" && $EVALUATOR $CASE3 /tmp/case3_post_gp.out | tail -3
echo "=== case3 post-GP +Planar ===" && grep -E "HPWL is" /tmp/case3_post_gp_with_planar_run.log
echo "=== case3 post-GP +Planar evaluator ===" && $EVALUATOR $CASE3 /tmp/case3_post_gp_with_planar.out | tail -3
```

Expected: 두 변형 모두 evaluator HPWL 가 paper Ours (30,234,112) 의 ±15% 안에 들어와야 함. 더 크면 frontend 차용에도 불구하고 backend 가 paper 후반부보다 훨씬 못한다는 신호 → 다음 cycle leverage point 재정의.

- [ ] **Step 2: 두 변형 중 paper 에 더 가까운 path 결정**

| 결정 기준 | 우선순위 |
|---|---|
| evaluator HPWL 작은 쪽 | 1순위 |
| 동률이면 단순한 쪽 (Skip) | 2순위 |

결과를 `docs/agents/phase4-divide-and-conquer.md` Table 1 에 기록.

- [ ] **Step 3: case2 도 같은 비교 (이미 완료)**

case2 post-GP Skip 결과 (이미 측정): evaluator HPWL = **2,006,967**. case2 post-GP +Planar 도 추가 측정 필요?

```bash
cat <<'EOF' > /tmp/case2_post_gp_with_planar.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
set_mdm_partition_file -file /tmp/case2_ipl.out.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="; get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="; get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="; get_3d_hpwl
write_iccad2022_output -out /tmp/case2_post_gp_with_planar.out
exit
EOF
/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit /tmp/case2_post_gp_with_planar.tcl > /tmp/case2_post_gp_with_planar_run.log 2>&1
```

Expected: HPWL evaluator output ≤ Skip 변형 (2,006,967). Planar 가 case2 에서 항상 도움 가설 검증.

---

## Task 2: case4 post-GP 측정 (Skip + with-Planar 둘 다)

**Files:**
- Create: `/tmp/case4_post_gp.tcl` (Skip 변형)
- Create: `/tmp/case4_post_gp_with_planar.tcl` (Planar 포함)

**의도:** case4 (sparser 66/70) 에서 Skip vs +Planar 결정. case2/3 데이터로는 dense (78/78) → Skip, sparse (70/75) → +Planar 패턴 추정 가능. case4 (66/70, 가장 sparse) 는 +Planar 가 도움일 가설.

- [ ] **Step 1: Skip 변형 작성**

```tcl
# /tmp/case4_post_gp.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case4_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/case4_post_gp.out
exit
```

- [ ] **Step 2: with-Planar 변형 작성**

```tcl
# /tmp/case4_post_gp_with_planar.tcl
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case4_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/case4_post_gp_with_planar.out
exit
```

- [ ] **Step 3: 두 변형 병렬 실행**

```bash
OPENROAD=/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad
$OPENROAD -no_init -no_splash -exit /tmp/case4_post_gp.tcl > /tmp/case4_post_gp_run.log 2>&1 &
$OPENROAD -no_init -no_splash -exit /tmp/case4_post_gp_with_planar.tcl > /tmp/case4_post_gp_with_planar_run.log 2>&1 &
wait
```

Expected: 두 job 완료 (case4 가 가장 큰 디자인이라 ~30분 소요 예상). Skip 변형은 SemiLeg 만이라 ~10분, +Planar 는 ~30분.

- [ ] **Step 4: evaluator 로 검증 + paper 와 비교**

```bash
EVALUATOR=/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525
CASE4=/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
echo "=== case4 Skip ==="; $EVALUATOR $CASE4 /tmp/case4_post_gp.out | tail -3
echo "=== case4 +Planar ==="; $EVALUATOR $CASE4 /tmp/case4_post_gp_with_planar.out | tail -3
```

Expected: paper case4 Ours HPWL = **267,381,744**. 우리 둘 중 작은 쪽 ≤ 270M (paper +1% 안) 이면 frontend 차용 검증 완료. 큰 차이 (>5%) 나면 case4 frontend 차용에도 backend 격차 큼 → leverage point 식별.

---

## Task 3: regression script 3개 case 모두 새 형식으로 덮어쓰기

**Files:**
- Modify: `src/mdm/test/regression_phase4_case2.tcl`
- Modify: `src/mdm/test/regression_phase4_case3.tcl`
- Create: `src/mdm/test/regression_phase4_case4.tcl`

**의도:** 모든 case 의 numerical lock 을 "Xueyan post-GP load → [Planar 또는 Skip per Task 1/2 결정] → SemiLeg → write" 형식으로 통일. 기존 flow (flat init + GTO) 는 historical reference 로만 보존 (필요시 별도 script).

- [ ] **Step 1: case2 script 덮어쓰기 (Task 1 Step 3 결과로 with-Planar 가 winner 가정)**

```tcl
# src/mdm/test/regression_phase4_case2.tcl
# Phase 4 case2 e2e numerical lock — Xueyan post-GP frontend + our backend.
# Baseline: HPWL = <fill from Task 1 Step 3 evaluator output>
# Tolerance: +/- 0.5%.
#
# Strategy: divide-and-conquer. Frontend (partition + GP) is paper-equivalent
# (Xueyan reference data). Our differentiation = backend (Planar/SemiLeg/TermLeg).
# This script measures the full e2e starting from paper-equivalent state.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
set_mdm_partition_file -file /tmp/case2_ipl.out.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case2.out
exit
```

만약 Task 1 Step 3 결과가 Skip 이 winner 면 `run_planar_correcting -iterations 1` 줄 + 그 위 puts 줄 제거.

- [ ] **Step 2: case3 script 덮어쓰기 (Task 1 Step 1 결과 따라)**

```tcl
# src/mdm/test/regression_phase4_case3.tcl
# Phase 4 case3 e2e numerical lock — Xueyan post-GP frontend + our backend.
# Baseline: HPWL = <fill from Task 1 Step 1 evaluator output>
# Tolerance: +/- 0.5%.
#
# Strategy: same as case2 (see comment there).

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case3_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
# IF Task 1 Step 1 winner is +Planar:
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl
# END IF
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case3.out
exit
```

Task 1 Step 1 결과 따라 `run_planar_correcting` 줄 + 그 위 puts 포함/제외.

- [ ] **Step 3: case4 script 신규 작성 (Task 2 결과 따라)**

```tcl
# src/mdm/test/regression_phase4_case4.tcl
# Phase 4 case4 e2e numerical lock — Xueyan post-GP frontend + our backend.
# Baseline: HPWL = <fill from Task 2 Step 4>
# Tolerance: +/- 0.5%.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case4_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
# IF Task 2 winner is +Planar:
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl
# END IF
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case4.out
exit
```

---

## Task 4: regression_check.sh 에 case4 baseline 추가 + case별 baseline 갱신

**Files:**
- Modify: `src/mdm/test/regression_check.sh:13-16`

**의도:** 새 baseline (Xueyan post-GP 출발) 으로 케이스 매핑 갱신, case4 추가.

- [ ] **Step 1: BASELINE_HPWL 매핑 갱신**

```bash
# 현재 src/mdm/test/regression_check.sh:13-16
case "$CASE" in
    case2) BASELINE_HPWL=2694774 ;;       # 이전 baseline (flat + GTO + Planar)
    case3) BASELINE_HPWL=52217337 ;;      # 이전 baseline (flat + GTO + Skip Planar)
    *) echo "FAIL: unknown case '$CASE' (expected case2 or case3)"; exit 2 ;;
esac

# 변경 후
case "$CASE" in
    case2) BASELINE_HPWL=<from Task 1 Step 3> ;;
    case3) BASELINE_HPWL=<from Task 1 Step 1> ;;
    case4) BASELINE_HPWL=<from Task 2 Step 4> ;;
    *) echo "FAIL: unknown case '$CASE' (expected case2|case3|case4)"; exit 2 ;;
esac
```

- [ ] **Step 2: usage 메시지 갱신**

```bash
# 라인 4 의 주석:
# Usage: regression_check.sh [case2|case3]   (default: case2)
# 로 변경:
# Usage: regression_check.sh [case2|case3|case4]   (default: case2)
```

---

## Task 5: 세 case 모두 numerical lock 확인 (0% drift)

**Files:**
- Run: `src/mdm/test/regression_check.sh case2`
- Run: `src/mdm/test/regression_check.sh case3`
- Run: `src/mdm/test/regression_check.sh case4`

**의도:** Task 3+4 결과 가 일관되게 reproducible 한지 검증. 각 case 모두 0% drift 로 PASS.

- [ ] **Step 1: case2 lock 확인**

```bash
bash src/mdm/test/regression_check.sh case2
```

Expected: `Diff: 0 % (tolerance +/- 0.5 %)` `PASS: HPWL within tolerance.`

- [ ] **Step 2: case3 lock 확인**

```bash
bash src/mdm/test/regression_check.sh case3
```

Expected: `Diff: 0 %` `PASS`.

- [ ] **Step 3: case4 lock 확인**

```bash
bash src/mdm/test/regression_check.sh case4
```

Expected: `Diff: 0 %` `PASS`. case4 가 큰 디자인이라 ~10분 또는 그 이상 소요.

만약 어느 하나라도 0% drift 가 아니면: random seed/threading 비결정성 추적. CellsLegalizer pairSwap 이 비결정적일 가능성 — 확인 후 fix 또는 tolerance 0.1% 로 완화 (단 0.5% 절대 금지 — 큰 변동 가리는 둔감 baseline).

---

## Task 6: paper 격차 분석 문서 작성

**Files:**
- Create: `docs/agents/phase4-divide-and-conquer.md`

**의도:** 3 case 모두에 대해 paper Table I 와의 격차 분석. 어느 backend step 이 격차 원인인지 식별. 다음 cycle leverage point 명시.

- [ ] **Step 1: 격차 표 작성**

```markdown
# Phase 4 — Divide-and-Conquer Baseline + Paper 격차 분석

작성일: 2026-05-07.

## 1. 전략

frontend (partition + GP) 은 Xueyan reference data 로 paper-equivalent 고정.
backend (Planar/SemiLeg/TermLeg) 가 우리 차별화. 새 regression baseline 은
"set_3D_IC (Xueyan post-GP) → backend → write" e2e 측정.

## 2. paper Table I 와의 격차 (이번 cycle 측정)

| Case | 우리 baseline (post-GP backend) | paper Ours | gap | paper Flattened GP | 우리 set_3D_IC HPWL |
|---|---|---|---|---|---|
| case2 | <Task 1 Step 3 결과> | 1,992,499 | <%> | 1,758,214 | <run_log> |
| case3 | <Task 1 Step 1 결과> | 30,234,112 | <%> | 26,474,613 | 27,479,632 |
| case4 | <Task 2 결과> | 267,381,744 | <%> | 248,129,463 | <run_log> |

## 3. backend step 별 wirelength 누적 (case3 기준)

| step | HPWL | delta |
|---|---|---|
| set_3D_IC (Xueyan post-GP) | 27,479,632 | — |
| Planar Correcting (옵션) | <run_log> | <%> |
| SemiLeg | <run_log> | <%> |
| TermLeg | <evaluator final> | <%> |

## 4. 다음 leverage point 후보 (격차 큰 case 기준)

격차 ≤ +5% 면: Phase 4.6 Bilevel + Phase 4.4 Multi-Tier 신규 구현 (paper §IV.F)
격차 5% < ≤ 20% 면: CellsLegalizer wirelength-aware refactor (현재 거리 swap 만, displacement-aware 로 확장)
격차 > 20% 면: backend 알고리즘 자체 재검토 (DREAMPlace LG 포팅, 등)
```

- [ ] **Step 2: 표 채우기 + 결론 1 paragraph**

Task 1, 2 의 측정값을 위 표에 기재. 결론: 우리 backend 가 paper 후반부와 등가인지/얼마나 떨어지는지 1-paragraph 요약.

---

## Task 7: handoff.md 갱신 + commit

**Files:**
- Modify: `handoff.md`
- Modify: `src/mdm/test/regression_phase4_case{2,3}.tcl`
- Create: `src/mdm/test/regression_phase4_case4.tcl`
- Modify: `src/mdm/test/regression_check.sh`
- Create: `docs/agents/phase4-divide-and-conquer.md`

- [ ] **Step 1: handoff.md 의 §1, §2, §4 업데이트**

§1 작업 목표: divide-and-conquer baseline 도입, paper-equivalent frontend 차용.
§2 검증 상태: 새 baseline 수치 (3 case) + 격차 (vs paper Table I).
§4 다음 leverage point: 격차 분포에 따른 우선순위 (Task 6 §4 결론 반영).

- [ ] **Step 2: 모두 stage + commit (1 commit)**

```bash
git add src/mdm/test/regression_phase4_case2.tcl
git add src/mdm/test/regression_phase4_case3.tcl
git add src/mdm/test/regression_phase4_case4.tcl
git add src/mdm/test/regression_check.sh
git add docs/agents/phase4-divide-and-conquer.md
git add handoff.md
git -c commit.gpgsign=false commit -s -m "$(cat <<'EOF'
test(mdm): switch Phase 4 baselines to paper-equivalent post-GP frontend

Use Xueyan reference data (positions + partition) as the divide-and-conquer
boundary. Frontend (partition + GP) becomes paper-equivalent, so regression
isolates our backend (Planar/SemiLeg/TermLeg). New baselines:
  case2: <H2> (paper Ours 1,992,499, gap +<X>%)
  case3: <H3> (paper Ours 30,234,112, gap +<Y>%)
  case4: <H4> (paper Ours 267,381,744, gap +<Z>%)

Previous baselines (flat init + GTO + Planar) are deprecated; the
frontend-quality term they captured is now constant across runs. Documented
gap analysis and next leverage points in
docs/agents/phase4-divide-and-conquer.md.
EOF
)"
git log --oneline -3
```

Expected: commit 성공, log 에 새 commit 표시.

---

## Task 8: 다음 cycle 입구 정리

- [ ] **Step 1: 격차 분포 따라 다음 cycle 결정**

Task 6 의 결론을 handoff.md §4 에 단일 leverage point 로 명시. 예:
- "case3 격차 +<Y>%, 거의 전부 SemiLeg 단계. 다음 cycle: CellsLegalizer wirelength-aware refactor"
- 또는: "case2/3 모두 paper Ours +5% 안. paper 추월 가능. 다음 cycle: paper §IV.F Multi-Tier Placement 신규 구현"

복수 leverage 가 동률이면 사용자에게 결정 위임 (handoff §4 의 "사용자와 결정" 노트).

---

## Self-Review

**Spec coverage:**
- [x] case2/3/4 모두 새 baseline 측정 — Task 1, 2
- [x] regression script 통일 (post-GP 출발) — Task 3
- [x] regression_check.sh case4 추가 — Task 4
- [x] 모든 case 0% drift 검증 — Task 5
- [x] paper 격차 분석 문서 — Task 6
- [x] handoff + commit — Task 7
- [x] 다음 cycle 입구 — Task 8

**Placeholder scan:**
- Task 1 Step 1, Task 2, Task 3 의 baseline 숫자가 `<...>` 로 비어 있음. 이건 placeholder 가 아니라 *측정 결과를 채워야 할 자리* — execution 단계에서 실제 값으로 채움. 의도된 빈자리.
- Task 6 Step 1 결론 1-paragraph 도 결과 의존. 의도된 빈자리.

**Type consistency:**
- `BASELINE_HPWL`, `CASE`, `regression_phase4_$CASE.tcl` 패턴이 Task 3, 4, 5 일관.
- TCL command 이름 (`run_planar_correcting`, `run_semi_legalizer`, `set_3D_IC`, `set_mdm_partition_file`, `parse_iccad2022_output`, `read_iccad2022`, `write_iccad2022_output`, `get_3d_hpwl`) 모두 검증된 기존 명령어.

**Missing tasks:**
- 처음 작성 시 case4 measurement 가 미진했지만 Task 2 로 보강함.
- backend step 별 contribution 분리 (Planar 만 vs SemiLeg 만) 는 Task 6 §3 표에서 각 step 후 HPWL 출력 분석으로 커버.

---

## 격차 분포에 따른 다음 cycle 분기

이번 plan 의 결과로 다음 cycle 의 leverage point 가 결정됨:

| 격차 (paper Ours 대비) | 다음 cycle | 예상 작업량 |
|---|---|---|
| ≤ +5% | paper §IV.F Multi-Tier Placement 신규 구현 (paper 추월용) | 중 (~1 week) |
| +5% ~ +20% | CellsLegalizer wirelength-aware refactor + SemiLeg displacement penalty 추가 | 대 (~2 weeks, 32K LOC 분석) |
| > +20% | backend 전면 재검토 — DREAMPlace LG 포팅 검토 / OpenROAD `dpl` 직접 사용 검토 | 미정 (pre-research 필요) |

case 별로 격차 분포가 다르면 (예: case2 ≤5%, case3 +20%) **dense-design-specific backend tuning 분기** 추가 가능 — `runSemiLegalizer` 에 wirelength-aware mode flag 추가, 케이스별 최적 mode 선택. 이건 Phase 4.2 simplification 잔여 항목 (β·Δo, multi-bin overflow) 과 묶어서 처리 가능.

---

## Plan complete

저장 위치: `docs/superpowers/plans/2026-05-07-divide-and-conquer-baseline.md`.
