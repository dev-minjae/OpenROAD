# Handoff — Phase 4 (Global Tier Optimization) + Refactor cycle 완료

## 0. Refactor cycle (이번 세션, 8 commits)

Phase 4 신규 코드 (Global Tier Opt + Planar Correcting + 보조 plumbing) 정리. **case2 e2e HPWL 2,694,774 정확 일치 (0% drift)**.

### 새 commits (이번 cycle)
```
19f21dd617 chore(mdm): regression smoke test + numerical lock
22e8df272d refactor(mdm): remove Phase 4 stubs (YAGNI) — BilevelCoordinator,
           FastTerminalLegalizer, runFlattenedPlacement, run3DPlacement 삭제
ee4540b5b9 refactor(mdm): extract getChildBlocks/dieIndexOf helpers
511e26e616 refactor(mdm): decompose runGlobalTierOptimization
           (findFromToBlocks, mapKnapsackCaps, findLibForBlock, applyMigration)
34a0e0270d refactor(mdm): expose Tcl knobs (Planar + Global Tier Opt)
dfb25f4815 refactor(mdm): simplify GlobalTierOptimizer overflow + huge-net branch
bb0c5c54d4 refactor(mdm): scope makeInterconnections idempotency to topology
c45b8a1781 chore(mdm): document logger ID range convention
```

### 새 Tcl knobs (case3 fix에 즉시 활용 가능)
```tcl
run_planar_correcting -iterations N \
    -density D                    ;# default 1.5
    -intersected_net_weight W     ;# default 1.5
    -nesterov_max_iter N          ;# default 5000
    -no_skip_io_mode              ;# (flag)

run_global_tier_optimization \
    -rho R -alpha A -beta B -gamma G \
    -b_factor B                   ;# default 1.0
    -max_net_fanout F             ;# default 100
    -u_t_percent UT               ;# default 0 = ICCAD case header
    -u_b_percent UB               ;# default 0 = ICCAD case header
    -apply
```

### 새 regression infrastructure
- `src/mdm/test/test_phase4_smoke.tcl` — 30초, dispatch sanity
- `src/mdm/test/regression_phase4_case2.tcl` — case2 e2e baseline
- `src/mdm/test/regression_check.sh` — evaluator + HPWL ±0.5% gate

### Refactor docs
- `docs/superpowers/specs/2026-05-06-mdm-phase4-refactor-design.md` — design
- `docs/superpowers/plans/2026-05-06-mdm-phase4-refactor.md` — implementation plan

# (이하 이전 세션 handoff — Phase 4 진행 중간 상태 자료)


## 1. 작업 목표

OpenROAD multi-die 3D placement에서 **paper iPL-3D §IV.B (Global Tier Optimization, Algorithm 2)** 와 후속 §IV.D (Planar Solution Correcting)를 from-scratch로 구현 + e2e legal output 도달. 이전 Stage 3.3 cells legalization은 외부 partition file에 의존했지만, Phase 4는 partition decision 자체를 우리 코드가 결정.

## 2. 현재 진행 상황 (브랜치 `feature/mdm/global-tier-opt`, HEAD `df11a310a7`)

### 완료
- **Phase 4.0** (research, no code) — paper hand-trace, src/par audit, SwitchInstHelper 검증, RePlAce freeze 결정, Xueyan oracle 식별. notes: `.workspace/phase4-research-notes.md`.
- **Phase 4.1** (skeleton) — `GlobalTierOptimizer`/`FastTerminalLegalizer`/`BilevelCoordinator` empty classes + 3개 Tcl 명령 plumbing.
- **Phase 4.2** (Algorithm 2 core) — surrogate Eq 10a, single-bin overflow, knapsack, fanout cap, target-tech area, paper-faithful unit (dbu→μm 변환). `runGlobalTierOptimization -apply` 가 partition_id flip + SwitchInstanceHelper migration + makeInterconnections 재호출.
- **Phase 4.5** (Planar Correcting, paper §IV.D) — RAII-scoped `ScopedFirmFreeze` + `replace_->doNesterovPlace`로 die-alternating GPL.
- 부수 fix: row-choice typo (orig_x → orig_y), Tetris row-pack (default OFF), snap_cells_to_rows helper, `B_factor=1.0`, `cellAreaInToTech` (different-tech), `makeInterconnections` idempotent.

### 측정 결과 (case2 e2e, evaluator-checked)

| Path | HPWL | 비교 |
|---|---:|---|
| flat init | 1,986,626 | (baseline) |
| Algorithm 2 + apply | 2,099,526 | (raw) |
| Planar Correcting | **1,952,358** | **paper-grade** (paper Table I = 1,992,499) |
| SemiLeg | 2,672,769 | post-Planar input의 +720K bump |
| **Evaluator (final)** | **2,694,774 (legal)** | paper +35.3%, Contest #1 +30.1% |

### case3 진단 (e2e 부분 측정)

| Stage | HPWL |
|---|---:|
| flat init | 27,272,257 |
| Algorithm 2 + apply (ρ=500 paper unit) | 28,197,323 |
| Planar Correcting | (40분 미수렴 — Nesterov overflow 0.13~0.21에 stuck) |
| Algorithm 2 step의 #Term | 16,196 (paper 9,612 → 1.7×, 이전 ρ=500 dbu에서는 29,529 → 3.07×) |

case3 e2e는 **Phase 4.5 Planar Correcting hyperparameter 미해결**로 완전 측정 못 함 (ρ=500 dbu 시점에서 측정한 case3 e2e 49,036,412 evaluator 통과 결과 있음 — 단 그건 over-migration 상태).

### case2 vs paper case2 (#Term)
- 우리: 711 (paper 1.5×)
- 이전 ρ unit 미스매치 시점: 1540 (paper 3.34×)

## 3. 변경된 파일 (이번 Phase 4 시리즈에서)

### 새 파일 (commit included)
- `src/mdm/src/GlobalTierOptimizer.{h,cpp}` — Algorithm 2 core
- `src/mdm/src/FastTerminalLegalizer.{h,cpp}` — Phase 4.1 stub (Phase 4.3+ 본격 채울 예정)
- `src/mdm/src/BilevelCoordinator.{h,cpp}` — Phase 4.1 stub (Phase 4.6 본격)
- `src/mdm/test/test_phase41_skeleton.tcl` — Phase 4.1 smoke test

### 수정된 파일
- `src/mdm/include/mdm/MultiDieManager.h` — 새 method (`runFlattenedPlacement`, `runGlobalTierOptimization`, `run3DPlacement`, `runPlanarCorrecting`, `snapCellsToRows`, `getMaxUtils`, `getICCADScale`)
- `src/mdm/src/MultiDieManager.cpp` — 위 method 구현 + apply path + makeInterconnections idempotent + ScopedFirmFreeze RAII
- `src/mdm/src/MultiDieManager.tcl` — 새 Tcl proc (`run_flattened_placement`, `run_global_tier_optimization`, `run_3d_placement`, `run_planar_correcting`, `snap_cells_to_rows`) + `run_semi_legalizer`에 `-skip_pair_swap`/`-tetris` 추가
- `src/mdm/src/MultiDieManager.i` — SWIG bindings (clang-format 사용 절대 금지)
- `src/mdm/src/CellsLegalizer.{h,cpp}` — Mode enum (ABACUS/TETRIS), tryMoveCellAcrossDies stub, legalizeBlockTetris, skip_pair_swap_, typo fix
- `src/mdm/src/SemiLegalizer.cpp` — typo fix (orig_x → orig_y)
- `src/mdm/src/TestCaseManager.h` — `getMaxUtils()` accessor (이미 getScale 있음)
- `src/mdm/CMakeLists.txt`, `src/mdm/BUILD` — 새 cpp/h 등록
- `src/mdm/messages.txt` — auto-regenerated (gitignored)

### Workspace 문서 (gitignored)
- `.workspace/phase4-global-tier-opt-plan.md` — master plan
- `.workspace/phase4-research-notes.md` — Phase 4.0 research
- `.workspace/phase42-algorithm2-plan.md` — Phase 4.2 plan
- `.workspace/phase42-qor-results.md` — QoR 측정 결과
- `.workspace/phase4-progress-log.md` — **모든 단계별 진행 + advisor 리뷰 + 결정 사항** (이거 먼저 읽기)
- `.workspace/phase41-skeleton-plan.md` — Phase 4.1 plan
- `.workspace/bench/tcl/phase42_diagnostic_case{2,3,4}.tcl` — raw HPWL diagnostic
- `.workspace/bench/tcl/phase42_e2e_case{2,3}*.tcl`, `phase45_e2e_case2*.tcl` — e2e scripts

### Commits (master 이후 새로 추가, 시간순)
```
f9895ed1f6 feat(mdm): Phase 4.1 skeleton
5037faf873 feat(mdm): Phase 4.2 — Algorithm 2 core + first QoR signal
cef0b7c2b9 fix(mdm): Phase 4.2 e2e — target-tech area + idempotent makeInterconnections
7cb5d3fdaa fix(mdm): fanout cap is now unbiased — count ΔTerm on huge nets
4374b46e42 feat(mdm): Phase 4.5 Planar Solution Correcting
9757c800dd fix(mdm): row-choice typo + skip_pair_swap option
4251037b0a feat(mdm): add snap_cells_to_rows helper
546081f931 feat(mdm): Tetris row-pack (default OFF)
c021aaff73 fix(mdm): restore MultiDieManager.i SWIG syntax
3037a2bbbc fix(mdm): ρ default 1M (dbu, obsoleted by next)
df11a310a7 fix(mdm): surrogate now runs in paper's μm unit (paper-faithful)  ← HEAD
```

## 4. 남은 작업

### 즉시 leverage point
1. **case3 Planar Correcting nesterov 미수렴 fix** — `runPlanarCorrecting` 내 `gpl::PlaceOptions`의 `density`, `nesterovPlaceMaxIter`, `routabilityCheckOverflow` 등 case3에 맞게 tuning. 현재 Nesterov가 overflow 0.13~0.21 fixed point에 걸림.
2. **case3 e2e 끝까지 측정** — Planar 수렴 후 SemiLeg + TerminalLeg + evaluator 까지. 진짜 "paper +X%" 수치 결정.

### Phase 4 plan 의 잔여 phase
3. **Phase 4.3 VNS (Variable Neighborhood Search)** — paper §IV.B-4. N1/N2/N3 neighborhoods 구현. 단 case2/3에서 #Term은 이미 paper의 1.5~1.7× 까지 좋아짐 — VNS의 가치 측정 후 결정.
4. **Phase 4.4 Flattened Init wrapper** — 현재 `parse_iccad2022_output` 으로 임시 (Xueyan 좌표 import). 우리 GPL로 직접 flat init 만드는 wrapper.
5. **Phase 4.6 Bilevel Coordinator** — paper Algorithm 1, M=4 SP-1↔SP-2 alternation. 현재 `BilevelCoordinator` 는 stub.
6. **Phase 4.7 Cross-die move (paper §IV.C.2 MOVE mode)** — `CellsLegalizer::tryMoveCellAcrossDies` stub 채우기. SwitchInstanceHelper + reStitchAffectedNets + 로컬 re-legalize.
7. **Phase 4.8 from-scratch bench** — 외부 `.par` 없이 `read_iccad2022 → set_3D_IC → run_3d_placement → write` 만으로의 case{2,3,4} bench scripts.

### Phase 4.2 simplifications 잔여 (paper와 차이)
8. **β·Δo (overlap term, Eq 10b)** — 현재 dropped. flat init에서는 cells row-aligned이라 영향 작지만 dense 영역에서는 의미. paper Table III β=0.5.
9. **multi-bin overflow** — 현재 single-bin per die. paper §IV.B-3은 region-based bin grid. high-density designs에서 차이.

## 5. 주요 결정사항

| 결정 | 이유 |
|---|---|
| Standard OpenROAD `dpl::Opendp` 사용 안 함 | ICCAD 2022 cells width가 site space 배수 아님 → DPL displacement search 무한루프 (사용자 확인: 정상 1-2초가 우리는 10분+ hung). `strategy-and-gap-tracking.md:290`에 이미 기록됨. SemiLegalizer/CellsLegalizer가 ICCAD-aware DP 역할. |
| ρ default = paper Table III의 500 (μm 단위) | dbu→μm 변환 layer 추가 (`dbu_per_um`). paper-faithful + case2 e2e 5% 개선 (#Term 1540→711). |
| `B_factor = 1.0` (paper의 1.1 대신) | paper의 1.1은 후속 detailed tier opt가 cap 정정한다고 가정. 우리는 단독이라 strict. Phase 4.7 land 후 1.1로 복원 가능. |
| Tetris row-pack 기본 OFF | simple Tetris implementation이 cells dense 영역에서 row 끝 초과 → illegal output. Abacus가 cluster q/e/w로 displacement-min 잘 처리. Tetris 코드는 ICCAD-aware DPL의 seed로 보존. |
| row-snap helper 도입 후 보존 | row-snap만으로는 SemiLeg +725K bump 해결 못 함 (Abacus row-pack의 cluster-internal pack이 displacement-min 안 함). 단 future Phase 4.6/4.7 인프라로 보존. |
| pair-swap 활성화 유지 | advisor 가설 #2 ("swap이 noise 증폭")이 falsified. skip하면 case2 e2e +250K worse. swap이 row-pack displacement를 회복. |
| 우리 `CellsLegalizer` (Stage 3.3) 가 paper §IV.C.1과 align됨 | paper Fig 5 dynamic-row Abacus + mid-row insert + cascade merge 충실 구현. 단 §IV.C.2 First Improvement의 priority sort/top-δ/FIX-MOVE는 미구현. cross-row pair-swap이 부분적 대체. |

## 6. 주의사항 / 알려진 이슈

### 빌드/Format 규칙 (CLAUDE.md 외 추가)
- **`clang-format`은 `*.i` 파일에 절대 금지** (SWIG 문법 깨짐). 한 번 실행해서 망가졌다가 `c021aaff73`에서 revert함. 추후 어떤 format 작업도 `*.i` 제외 명시.
- `messages.txt`는 빌드 시 자동 생성 (gitignored). 새 logger 호출 ID는 추가만 하면 됨.
- `git commit -s` 필수 (DCO).

### Algorithm 2 / Surrogate
- **paper-unit-faithful 상태**: `TierOptParams.dbu_per_um`이 caller(MultiDieManager)에 의해 채워짐. 새 caller가 호출 시 이걸 set 안 하면 raw dbu (legacy) 동작.
- **fanout cap=100**: huge net (clock/broadcast)은 ΔWL 없이 ΔTerm만 카운트 (advisor R-13 fix). 너무 작은 cap이면 ΔWL signal 손실.
- **Algorithm 2 -apply path가 makeInterconnections 재호출**: cross-die nets 의 BTerm pair 가 처음 set_3D_IC에서 0개 만들어진 상태에서, apply 후 cell migration으로 새로 cross-die 된 net 들의 BTerm 만듦. `makeInterconnections`는 이제 idempotent.

### Phase 4.5 Planar Correcting
- **case3에서 nesterov 미수렴**: ρ=paper-unit으로 cells migration 줄어 cells 더 sparse → electrostatic restoring force 약함 → 40분+ overflow 0.13~0.21 stuck. `gpl::PlaceOptions`의 `density` (현재 1.5), `nesterovPlaceMaxIter` (default 5000), `intersectedNetWeight` (1.5) tuning 필요.
- **`doIncrementalPlace` 사용 금지**: post-migration mixed-block layout에서 InitialPlace BiCGSTAB crash. 대신 `doNesterovPlace` 사용 (initial place skip, cells 이미 valid 위치).
- **`ScopedFirmFreeze` RAII**: stack-bound. 동일 stack frame에서 grt/dpl 같은 concurrent placer 호출 금지 (process-wide visible 함).

### Different-tech (case2/3_hidden)
- case2 main도 `Tech TA` (top, MC1=69×176) vs `Tech TB` (bottom, MC1=99×252). knapsack은 to-tech master area 사용 (`cellAreaInToTech` lazy cache). 이게 빠지면 168% utilization (이전 bug).

### case3/4 e2e 측정의 시간 비용
- case3: Planar Correcting + SemiLeg + TerminalLeg = 30분 정도 (현재 ρ=500 paper unit에서 Planar 미수렴 issue).
- case4: 더 오래. 측정 시 background로 monitor 사용.

### 행정 파일 (gitignored, 매 세션 갱신)
- `.workspace/phase4-progress-log.md`이 가장 자세한 timeline + advisor 리뷰 인용. 다음 세션 첫 30분에 이거 정독 권고.
- `handoff.md` (이 파일) — Phase 4 series 한 페이지 요약.
- `paper.pdf` — iPL-3D 원문 (gitignored).

### 외부 reference 파일 위치
- ICCAD 2022 evaluator: `/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`
  - 호출: `evaluator_0525 <input_case.txt> <output.txt>`
- ICCAD case files: `/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case{2,3,4}.txt`
- Xueyan reference outputs: `/tmp/case2_ipl.out`, `/tmp/ref_gp/case3_gp.txt`, `/tmp/ref_gp/case4_gp.txt`
- Xueyan partition files: `/tmp/case2_ipl.out.par`, `/tmp/ref_gp/case3_gp.txt.par`, `/tmp/ref_gp/case4_gp.txt.par`
- iPL-3D paper summary (자급자족): `.workspace/ipl-3d-paper-summary.md`
- archive Xueyan reference impl: `/home/minjae/workspace/etc/openroad/archive/3d_ic/reference_impl/mdm/src/`

## 7. 다음 세션 첫 30분 권고 순서

1. **이 파일 (`handoff.md`) 읽기** (5분)
2. **`.workspace/phase4-progress-log.md` 읽기** — 모든 advisor 리뷰 + 결정 timeline (15분)
3. **`git log --oneline master..HEAD` 로 commit sequence 확인** (1분)
4. **case2 e2e quick smoke test로 환경 검증** (3분):
   ```bash
   /home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit \
     /home/minjae/workspace/etc/openroad/OpenROAD/.workspace/bench/tcl/phase45_e2e_case2.tcl
   /home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525 \
     /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt \
     /tmp/phase45_e2e_case2.out
   # 기대: HPWL 2,694,774 (legal)
   ```
5. **사용자에게 다음 leverage point 결정 받기** — Planar Correcting case3 fix / β·Δo 추가 / Phase 4.6 / Phase 4.7

## 8. Git 상태

- Branch: `feature/mdm/global-tier-opt`
- HEAD: `df11a310a7` (Phase 4.2 paper-faithful unit)
- Uncommitted (untracked): `handoff.md` (이 파일), `paper.pdf` (gitignored), `.claude/scheduled_tasks.lock` (시스템)
- master 이후 새 commits: 11개 (Phase 4 시리즈 전체)
- master+35 commits ahead (Stage 3.3 시리즈 + Phase 4 시리즈)

## 9. 핵심 reflection (이번 세션)

1. **Algorithm 2 + Planar Correcting까지는 paper-grade**. case2 post-Planar = 1.95M < paper 1.99M. Algorithm 2의 surrogate가 paper Table III default에서 cells migration 결정 잘 함. 단 paper-faithful unit 변환 (dbu→μm) 미구현 시 ρ가 underweight 되어 #Term 3× 부풀림. **`df11a310a7` commit이 그걸 fix**.

2. **남은 +35% e2e gap (case2)은 SemiLeg/CellsLegalizer가 free-form input 처리 약함이 한 부분 + TerminalLeg over-crowding이 다른 부분**. advisor 진단대로 "downstream 컴포넌트가 input distribution mismatch" pattern.

3. **Standard OpenROAD DPL은 ICCAD에서 작동 안 함** — 사용자 직감 + `strategy-and-gap-tracking.md:290` 기록 + 우리 trial confirms. 우리 SemiLeg/CellsLegalizer 가 ICCAD-aware functional DP. Tetris row-pack은 그 위 layer (default OFF).

4. **사용자 발언 "paper 대로 abacus" + "paper 대로 surrogate" 가 모두 정확한 방향성**. Stage 3.3 abacus는 paper §IV.C.1 충실 구현. surrogate도 이제 paper Table III 그대로 (μm unit).

5. **Tetris simple impl는 illegal output** — Abacus의 cluster q/e/w가 정답 (advisor 정확한 진단). Tetris 코드는 ICCAD-aware new DPL의 seed로 보존.
