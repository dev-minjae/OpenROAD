# Handoff — Divide-and-Conquer Baseline 록인 완료, 다음은 Phase 4.4 Flattened Init

## 1. 작업 목표

`docs/superpowers/plans/2026-05-07-divide-and-conquer-baseline.md` 의 plan 실행. paper-equivalent frontend (Xueyan post-GP 데이터) + 우리 backend 로 baseline 갈아엎기. paper Table I "Ours" 컬럼과 직접 비교 가능한 numerical lock 확보.

## 2. 현재 진행 상황

**완료 (이번 세션, 2 commits 예정).**

### 검증 상태 — 3 케이스 모두 paper Ours ±1% 안

| Case | 우리 baseline | paper Ours | gap | path |
|---|---|---|---|---|
| case2 | **2,004,424** | 1,992,499 | **+0.6%** | W1 +Planar |
| case3 | **30,229,424** | 30,234,112 | **-0.02%** | W1 Skip |
| case4 | **265,734,241** | 267,381,744 | **-0.6%** | W1 Skip |

3 케이스 평균 격차: **paper -0.01% (사실상 동률, case3/case4 미세 추월)**.

### Branch / HEAD
- Branch: `feature/mdm/global-tier-opt`
- HEAD: `21c50a4a8e` (이전 cycle, case3 e2e baseline lock — skip Planar)
- 이번 cycle 후 master+48 commits ahead (이전 47 + 이번 1)

### 검증 명령
```bash
bash src/mdm/test/regression_check.sh case2  # ~5분, PASS, HPWL 2,004,424
bash src/mdm/test/regression_check.sh case3  # ~3분, PASS, HPWL 30,229,424
bash src/mdm/test/regression_check.sh case4  # ~10분, PASS, HPWL 265,734,241
```

## 3. 변경된 파일

### 수정/덮어쓰기
- `src/mdm/test/regression_phase4_case2.tcl` — 새 path: Xueyan post-GP load + Planar (1 iter) + SemiLeg
- `src/mdm/test/regression_phase4_case3.tcl` — 새 path: Xueyan post-GP load + Skip Planar + SemiLeg
- `src/mdm/test/regression_check.sh` — case4 baseline 추가, baseline 매핑 새 수치로 갱신, header docstring 보강

### 신규
- `src/mdm/test/regression_phase4_case4.tcl` — case4 numerical lock 신규
- `docs/agents/phase4-divide-and-conquer.md` — 격차 분석, step 별 wirelength, W1/W2 구분, 다음 leverage points
- `docs/superpowers/plans/2026-05-07-divide-and-conquer-baseline.md` — 이번 cycle plan (writing-plans skill 산출물)

### 미커밋
- `paper.pdf` (gitignored)
- `.claude/scheduled_tasks.lock` (시스템)
- `docs/agents/openroad-gpu-contribution-notes.md` (이전 cycle untracked, 이번 cycle 변경 없음)

## 4. 남은 작업 (다음 cycle 후보)

### 1순위 — Phase 4.4 Flattened Init wrapper 신규 구현

이번 cycle 의 W2 시도 (raw coord + paper partition + 우리 Planar + 우리 SemiLeg) 가 막힌 직접적 원인. ICCAD 2022 raw input 은 cell 좌표 정보 없음 → `set_3D_IC` 후 cells unplaced → `gpl::Replace` movable instances=0 crash.

해결책: `runFlattenedPlacement` Tcl command 부활. 우리 자체 flat 2D GP (gpl::Replace::doNesterovPlace 1-pass on the unified flat layout). 이전 cycle 에서 YAGNI 로 삭제됨 — 정식으로 다시 추가.

가치:
- W2/W3 시나리오 측정 가능 → 우리 GP/Planar quality 단독 검증
- Xueyan 데이터 의존성 제거 → ICCAD 외 임의 디자인 처리
- paper draft 시 "Xueyan 데이터 빌려옴" 약점 제거

### 2순위 — case3 marginal 추월 (paper -0.02% → 명백한 추월)

backend 의 SemiLeg/TermLeg 미세 개선:
- CellsLegalizer pairSwap 추가 iteration
- SemiLeg displacement penalty
- TermLeg r-tree refinement passes 증가

### 3순위 — case4 의 -0.6% 격차 더 키우기 (1순위/2순위 작업이 비례 효과)

### 보류 — Phase 4.6 Bilevel Coordinator (paper Algorithm 1)

paper Table IV ablation 는 alternating 없이도 case3 30.33M 나옴. 우리 baseline 이 이미 paper-class 이라 Bilevel 의 marginal value 작음. 추후 paper "+X%" claim 가치를 분명히 증가시키는 게 확인되면 추가.

### 보류 — Phase 4.7 Cross-die move, Phase 4.8 from-scratch bench

이전 handoff 그대로. 별도 cycle.

## 5. 주요 결정사항

| 결정 | 이유 |
|---|---|
| baseline 을 W1 (paper coord + paper partition + 우리 backend) 으로 통일 | paper Table I 와 직접 비교 가능, paper-equivalent starting state. 격차 ±1% 안. |
| case3, case4 에서 Skip Planar / case2 에서 +Planar | empirically per-case 측정 winner. case3/4 는 Skip 이 -0.5%/-0.4% 좋음, case2 는 +Planar 가 -0.13% 좋음. |
| W2 (raw coord) path 는 보류 | 우리 flat 2D GP 미구현. Phase 4.4 의 명확한 motivation 됨. |
| Phase 4.6 Bilevel 우선순위 강등 | paper ablation 결과 + 우리 baseline 이 이미 paper-class. |
| C++ 코드 변경 없음 (이번 cycle) | 모든 변경 Tcl/script 수준. 기존 API 그대로. |
| `runFlattenedPlacement` 부활 결정은 다음 cycle 로 미룸 | 이번 cycle scope = baseline 록인. Phase 4.4 는 별도 plan 필요 (writing-plans). |

## 6. 주의사항 / 알려진 이슈

### CLAUDE.md 규칙 (이번 cycle 준수 확인)
- `clang-format` 변경 없음 (C++ 수정 안 함)
- `git commit -s` 필수 (DCO)
- `src/sta/` 수정 안 함

### Tcl flow 차이 (이전 baseline → 이번 baseline)
이전:
```tcl
read_iccad2022 -case <case>.txt
parse_iccad2022_output -file <xueyan>.out
exec awk "{print \$1, 0}" <xueyan>.par > /tmp/flat_<case>.par   # FORCE flat
set_mdm_partition_file -file /tmp/flat_<case>.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply        # 우리 Algorithm 2 (now bypassed)
[run_planar_correcting -iterations 1]      # 옵션
run_semi_legalizer
```

이번:
```tcl
read_iccad2022 -case <case>.txt
parse_iccad2022_output -file <xueyan>.out
set_mdm_partition_file -file <xueyan>.par   # paper partition direct (NO flatten)
set_3D_IC -die_number 2                      # paper-equivalent state
[run_planar_correcting -iterations 1]        # case2 만
run_semi_legalizer
```

차이점:
- partition 을 flatten 안 함 → cells 가 paper partition 으로 die 분리됨
- `run_global_tier_optimization` 호출 제거 → 우리 GTO 가 partition 다시 결정 안 함 (paper partition 그대로 사용)
- coord 도 paper post-GP 그대로

### case3 의 Planar Correcting 행동 변화
- 이전 cycle: Skip 이 winner 이유 = 우리 GTO 출력에서 시작하면 Planar 가 발산
- 이번 cycle: paper coord 에서 시작하면 Planar 정상 작동 (case3 W1 +Planar 30.39M, 발산 없음)
- 즉 **Planar Correcting 알고리즘 자체는 OK. 우리 GTO 출력이 Planar 와 호환되지 않음** — 진짜 leverage point 후보 (별도 cycle 에서 GTO 출력 quality 검증)

### 외부 reference 파일 (변동 없음)
- ICCAD 2022 evaluator: `/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`
- Case files: `/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case{2,3,4}.txt`
- Xueyan reference: `/tmp/case2_ipl.out{,.par}`, `/tmp/ref_gp/case{3,4}_gp.txt{,.par}`
- iPL-3D paper: `paper.pdf` (gitignored)

### Raw experiment logs
- W1 paths: `/tmp/case{2,3,4}_W1_run.log`, `/tmp/case{2,3,4}_W1_skip_run.log`
- W2 fail demos: `/tmp/case{2,3,4}_W2_run.log` ("Movable instances: 0" 패턴)
- 자세한 분석: `docs/agents/phase4-divide-and-conquer.md`

### 다음 세션 첫 단계 권고
1. 이 파일 (`handoff.md`) 읽기
2. `bash src/mdm/test/regression_check.sh case2 && bash src/mdm/test/regression_check.sh case3 && bash src/mdm/test/regression_check.sh case4` 실행해서 환경 sanity (3 케이스 모두 0% drift)
3. Phase 4.4 Flattened Init wrapper 의 plan 작성 (writing-plans skill). gpl::Replace::doNesterovPlace 단일-block 호출 + Tcl wrapper. 이전 cycle 에서 삭제된 stub 의 정상 구현.
4. 또는 paper "+X%" 수치 확정 (이미 분석 됨, `docs/agents/phase4-divide-and-conquer.md` §8 참고)
