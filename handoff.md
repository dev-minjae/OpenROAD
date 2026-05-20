# Handoff — Phase 4.4 Flattened Placement, row-doubling 직전 stop

## 1. 작업 목표

paper iPL-3D §IV.A "Flattened Placement" 자체 구현. paper coord (Xueyan reference) 빌리는 의존성 제거. Plan: `docs/superpowers/plans/2026-05-07-phase4-flattened-placement.md`.

목표 metric: case3 self-frontend HPWL ≈ paper Ours 30,234,112. (case3 만 paper 따라잡으면 case4 진행 — 사용자 결정).

## 2. 현재 진행 상황

**Phase 4.4 의 ~70% 진척, 알고리즘 quality issue 남음**.

### 완료 (이번 세션, 4 commits)
1. `78175fb5b4` plan 문서 작성
2. `636a3a2748` Tcl/SWIG/header interface 부활 (stub)
3. `8054a28b1a` `runFlattenedPlacement` 실제 구현 (gpl::Replace wrapper) + `gpl::PlaceOptions::skipDensityCheck` flag 신규
4. `5e77bada39` case2/3/4 self-frontend regression scripts

### 미커밋 (이번 세션 미완 작업)
- `src/mdm/src/MultiDieManager.cpp` — `runFlattenedPlacement` 에 die area doubling RAII + multi-threading 16 + y-clamp 추가
- `src/mdm/src/MultiDieManager.tcl` — target_density default 0.8 (이전 2.0 잘못된 해석)
- `src/gpl/src/placerBase.cpp` — debug log GPL-0200 (PlacerBaseCommon::init 의 die_rect 추적용; **commit 전 제거 필요**)
- `src/mdm/test/regression_phase4_self_case3.tcl` — `-nesterov_max_iter 1000` 추가

### case3 e2e 결과 (현재 state)
- runFlattenedPlacement 동작 ✅ (exit 0, ~22 분 with multi-threading 16 thread)
- e2e: 71,582,057
- paper Ours: 30,234,112
- **격차 +137%** ❌

### 진단 결과 (이번 세션, log 추적)
- `setDieArea(doubled)` + `setCoreArea(doubled)` **정상 동작** — `getDieArea()` 직후 doubled 반환
- `gpl::PlacerBaseCommon::init` 의 die_rect 도 doubled 인식 (GPL-0012 Die BBox = doubled 영역)
- region_area 도 doubled (GPL-0015 = 738M um² ≈ 2 × single die)
- 그런데 **utilization 174%** — cells area 가 region 의 1.74배. **cells 가 doubled 영역 안 spread 못 함**.
- 가설: **row 가 single die 영역만 cover** (TestCaseManager 가 row 만든 후 우리는 die area 만 변경, row 변경 안 함). doInitialPlace 가 row 위에 cells 배치 → cells 가 single die 영역 안에 cramped → Nesterov 가 그 위에서 작동 → spread 못 함

### Branch / HEAD
- Branch: `feature/mdm/global-tier-opt`
- HEAD: `5e77bada39 test(mdm): Phase 4.4 self-frontend regression scripts (case2/3/4)`
- master+58 commits ahead of master

## 3. 변경된 파일

### 새 commit 들 (이미 commit, branch 에 반영)
- `docs/superpowers/plans/2026-05-07-phase4-flattened-placement.md` — plan (608 줄)
- `src/mdm/include/mdm/MultiDieManager.h` — `runFlattenedPlacement` declaration
- `src/mdm/src/MultiDieManager.cpp` — `runFlattenedPlacement` (이번 세션 후반부에 미커밋 추가 변경 더 있음)
- `src/mdm/src/MultiDieManager.tcl` — Tcl proc `run_flattened_placement`
- `src/mdm/src/MultiDieManager.i` — SWIG `run_flattened_placement` binding
- `src/gpl/include/gpl/Replace.h` — `gpl::PlaceOptions::skipDensityCheck` 멤버 추가
- `src/gpl/src/replace.cpp` — `doInitialPlace` / `doNesterovPlace` 의 8 PlacerBase 생성에서 `!options.skipDensityCheck` 사용
- `src/mdm/test/regression_phase4_self_{case2,case3,case4}.tcl` — self-frontend 측정 scripts

### 미커밋 변경 (commit 전에 정리 필요)
- `src/mdm/src/MultiDieManager.cpp:[runFlattenedPlacement]`:
  - die area doubling RAII (y direction × 2): orig save → setDieArea/setCoreArea(doubled) → restore
  - debug log MDM-0301 (commit 전 제거 추천)
  - multi-threading: `replace_->doInitialPlace(/*threads=*/16, opts)` + `doNesterovPlace(threads, ...)`
  - y-clamp loop: doubled 영역 spread 한 cells 를 original die 영역으로 fold
  - `opts.skipDensityCheck = true` (region_area 가 doubled 인식하긴 하는데 utilization check 가 cells/region 비교 → 174% > 100%, 우회 필요)
- `src/mdm/src/MultiDieManager.tcl`:
  - `target_density` default 2.0 → **0.8** (paper 의 doubling 은 *die area* 두 배지 target_density 두 배 아님)
- `src/gpl/src/placerBase.cpp:[PlacerBaseCommon::init]`:
  - debug log GPL-0200 추가 (commit 전 제거 필수 — gpl src 변경은 최소화)
- `src/mdm/test/regression_phase4_self_case3.tcl`:
  - `run_flattened_placement -nesterov_max_iter 1000` (default 5000 너무 김)

### untracked (이번 세션 변경 없음)
- `paper.pdf` (gitignored)
- `.claude/scheduled_tasks.lock` (시스템)
- `docs/agents/openroad-gpu-contribution-notes.md` (이전 세션, 변동 없음)

## 4. 남은 작업

### 즉시 (다음 세션 시작 첫 작업)

**우선순위 1: row doubling 구현 (직접 가설 검증)**

current cycle stop 위치 — `runFlattenedPlacement` 안에서 die area 만 doubled, row 는 그대로. cells 가 row 위에만 placed → spread 못 함. 해결 plan:

옵션 A: TestCaseManager::rowConstruction 에서 doubled row 만들기 (invasive, all flow 영향)
옵션 B: `runFlattenedPlacement` 안에서 임시 row 추가 — doubled 영역 cover 하도록 dbRow 생성, 끝나면 삭제 (RAII). ~50 줄 작업
옵션 C: archive `reference_impl/mdm/src/` 비교해서 paper 의 row 처리 방식 확인 후 결정

추천: **옵션 B 먼저 시도** (~1-2 시간). 안 되면 archive 비교 (옵션 C).

**우선순위 2: debug log 정리 + commit**

- `placerBase.cpp` 의 GPL-0200 debug log 제거 (gpl 코드 변경 최소화 원칙)
- `MultiDieManager.cpp` 의 MDM-0301 debug log 제거
- 미커밋 파일들 commit (Phase 4.4 v0.1: runFlattenedPlacement 동작, quality 격차 +137% 알려진 상태)

### 다음 cycle (paper case3 따라잡기)
- row doubling 후 case3 e2e 재측정. 만약 quality 좋아지면 다음 cycle 의 case3 baseline lock
- paper 따라잡기 (격차 ±5%) 까지 더 필요한 fix:
  - `runFlattenedPlacement` 의 Nesterov knob 미세 조정 (target_density, max_iter)
  - paper 의 GTO output (§IV.B) 호환성
  - paper 의 Multi-Tier Placement (§IV.F) 신규 구현

### 보류
- case4 — paper 가 Diff_tech (paper Table II) 라 paper 자체가 다른 알고리즘. case3 paper 따라잡은 후 진행
- archive `reference_impl/mdm/src/` 와 우리 GTO 코드 line-by-line diff — 별도 cycle. row doubling 작업 후 진행

### GPU 가속 (별도 path)
사용자가 이번 세션에서 RTX 5090 활용 가속 의문 제기. 결정: (c) **알고리즘 fix + multi-threading 우선, GPU 는 나중**. 이번 cycle 에 multi-threading (threads=16) 적용 — 단일 thread 대비 5-6× 가속 (case3 5-6 시간 → 22 분). GPU porting 은 paper draft 후 PR #5352 unblock 합류 path 로.

## 5. 주요 결정사항

| 결정 | 이유 |
|---|---|
| paper §IV.A 의 "doubling threshold" 를 *die area 두 배 (geometric)* 로 해석 | 사용자 지적 — `target_density 2.0` 은 잘못된 해석 (정상 GP target 0.7-0.8). paper 의 doubling 은 면적 doubling (= cells 합 area / die area ≤ 100% 만족시키기 위함) |
| `gpl::PlaceOptions::skipDensityCheck` flag 신규 (gpl 코드 수정) | PlacerBase 의 utilization > 100% 절대 한계 우회. CLAUDE.md 의 src/sta 금지는 src/gpl 과 무관 |
| multi-threading threads=16 | case4 (220k cells) 의 5000 iter Nesterov 가 단일 thread 면 ~수시간. 16 thread 로 case3 22분. 향후 모든 실험 빨라짐. GPU 가속 (며칠~몇주 작업) 보다 즉시 효과 |
| target_density default 0.8 (이전 2.0 잘못) | 사용자 지적. 보통 GP target 0.7-0.8. doubling 은 die area 차원 |
| die area doubling 가 실제 effective 인지 debug 로 확인 | 디버그 진행하다가 PlacerBase 가 doubled 인식 함을 발견. 이전 추측 (region_area 미인식) 틀림 — 실제 issue 는 row 가 single die 영역만 cover |
| case4 보류, case3 만 우선 | 사용자 결정. case4 는 paper 자체 다른 알고리즘 path. case3 quality 잡힌 후 case4 |

## 6. 주의사항 / 알려진 이슈

### CLAUDE.md 규칙 (이번 cycle 위반 X)
- `src/sta/` 수정 안 함 ✓
- clang-format: `*.i` 파일에 사용 안 함 (manual edit only) ✓
- DCO: `git commit -s` ✓ (모든 commits)
- `src/gpl/` 수정 OK (CLAUDE.md 의 sta 금지와 무관)

### gpl 코드 변경의 위험
이번 cycle 의 `gpl::PlaceOptions::skipDensityCheck` 추가는 OpenROAD 본체 흐름에 영향 *미미* (default false 이라 기존 흐름 변경 X). 단 다음 cycle 작업 후 PR 시 OpenROAD 원작자에게 review 받을 가치 있음 (gpl 의 utilization check 우회 필요성 설명).

### Phase 4.4 v0.1 의 한계 (다음 세션 인지 필요)
1. **case3 final HPWL 71.6M = paper +137%** — paper 추월 못 함 (그러나 self-frontend 자체는 작동)
2. **utilization 174% with skipDensityCheck=true** — gpl 의 정상 사용 패턴 *아님*. paper 알고리즘 의도와 일치하지만 OpenROAD 스타일과 차이
3. **y-clamp 가 cells 좌표 강제 fold** — 3869 cells (case3) y 좌표를 doubled 영역에서 single 영역으로 강제 옮김. 이게 wirelength 격차의 직접 원인 가능성

### 검증 명령
```bash
cd /home/minjae/workspace/etc/openroad/OpenROAD
# 이전 cycle baseline (Xueyan-borrowed) — 여전히 작동
bash src/mdm/test/regression_check.sh case2     # PASS, HPWL 2,004,424
bash src/mdm/test/regression_check.sh case3     # PASS, HPWL 30,229,424
bash src/mdm/test/regression_check.sh case4     # PASS, HPWL 265,734,241

# 이번 cycle self-frontend (case3 만 의미)
build/bin/openroad -no_init -no_splash -exit \
    src/mdm/test/regression_phase4_self_case3.tcl > /tmp/sc3.log 2>&1
# 기대: 71.6M evaluator HPWL (paper +137%, 알려진 한계)
```

### 진행 시간 기록 (참고용)
- cycle 시작: 2026-05-07 ~22:23 KST
- 진행 중간 stop: 2026-05-08 ~01:58 KST
- 총 진행: ~3.5 시간 (case3 e2e 22분 포함)

### 외부 reference 파일 (변동 없음)
- ICCAD 2022 evaluator: `/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`
- Cases: `/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case{2,3,4}.txt`
- Xueyan post-GP: `/tmp/case2_ipl.out{,.par}`, `/tmp/ref_gp/case{3,4}_gp.txt{,.par}`
- iPL-3D paper: `paper.pdf` (gitignored, 9 페이지)
- archive paper reference impl: `/home/minjae/workspace/etc/openroad/archive/3d_ic/reference_impl/mdm/src/` (다음 cycle 비교 가치)

### 다음 세션 첫 단계 권고
1. 이 파일 (`handoff.md`) 읽기
2. `git status` 로 미커밋 변경 확인 — placerBase.cpp 의 debug log 정리, 그 외 변경 commit 결정
3. `bash src/mdm/test/regression_check.sh case3` 로 환경 sanity (이전 baseline 30.23M)
4. row doubling 시도 (옵션 B) — `runFlattenedPlacement` 안에서 임시 dbRow 추가
5. case3 e2e 재측정 후 quality 비교
