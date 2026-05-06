# Handoff — case3 Planar Correcting fix 완료, 다음 leverage point는 case4 검증 또는 paper 수치 확정

## 1. 작업 목표

이전 cycle의 `handoff.md §4 #1` 즉시 leverage point. case3 Planar Correcting Nesterov 미수렴 fix.
실제 발견: 미수렴이 아니라 **dense 디자인 (util 78/78) 에서 Planar Correcting 자체가 destructive**. SemiLeg 후 HPWL을 96M로 부풀림.
해결: case3 e2e 에서 Planar Correcting **skip**. HPWL 96.1M → **52.2M (45.7% 감소)**.

## 2. 현재 진행 상황

**완료 (이번 세션, 1 commit 예정)**.

### 검증 상태
- Smoke test PASS (`src/mdm/test/test_phase4_smoke.tcl`, ~10초)
- case2 numerical lock: HPWL = **2,694,774 (0% drift)** (`regression_check.sh case2`, ~5분)
- case3 numerical lock: HPWL = **52,217,337 (0% drift)** (`regression_check.sh case3`, ~3분, 신규)

### Branch / HEAD
- Branch: `feature/mdm/global-tier-opt`
- HEAD: `6aa1205e69` (이전 cycle 완료) — 이번 cycle commit 예정
- master + 이전 46 commits + 이번 1 commit = 47 commits ahead of master

### Knob 실험 결과 (case3, e2e SemiLeg HPWL)
| 실험 | density | intersected_net_weight | max_iter | HPWL | vs Skip |
|---|---|---|---|---|---|
| **Skip Planar** | — | — | — | **52.2M** | — (winner) |
| F | 1.5 (default) | 1.5 (default) | 200 | 59.1M | +13% |
| E | 1.5 | 1.5 | 500 | 59.3M | +14% |
| Default (case2 baseline) | 1.5 | 1.5 | 5000 | 96.1M | +84% |
| D | 1.5 | 0.1 | 1000 | 111M | +113% |
| B | 0.8 | 1.5 | 2000 | 116M | +122% |
| A | 1.0 | 0.5 | 2000 | 164M | +214% |
| C | 1.5 | 0.5 | 2000 | crash (Inf/NaN) | — |

**case2 control**: Skip = 2.74M (vs default 2.69M, **1.5% 더 나쁨**) — case2엔 Planar이 정말로 도움.

## 3. 변경된 파일

### 수정 파일
- `src/mdm/test/regression_phase4_case3.tcl` (case3 e2e script):
  - 이전: 기본 knobs로 `run_planar_correcting -iterations 1` 호출
  - 이후: Planar Correcting 호출 제거. 헤더에 *왜 skip* 인지 설명 (utility 78/78 dense, post-GTO overflow 0.13으로 이미 낮음, Nesterov 돌리면 폭발)
- `src/mdm/test/regression_check.sh` (numerical lock 평가기):
  - 이전: case2 hardcoded
  - 이후: `regression_check.sh [case2|case3]` (default `case2`). case별 baseline 매핑.

### 미커밋 파일
- `paper.pdf` (gitignored)
- `.claude/scheduled_tasks.lock` (시스템)
- `docs/agents/openroad-gpu-contribution-notes.md` (이전 세션 untracked, 이번 cycle 변경 없음)

## 4. 남은 작업

### 즉시 leverage point 후보
1. **case4 e2e 측정** — case4 (util 66/70, sparser) 는 case2처럼 Planar이 도움일 가능성 높음. 측정해서 baseline lock 추가
2. **paper +X% 수치 확정** — case2 + case3 둘 다 baseline 잡혔으니 Xueyan reference (`/tmp/case2_ipl.out`, `/tmp/ref_gp/case3_gp.txt`) 와 비교해서 paper claim 수치 도출
3. **자동 detection** — `runPlanarCorrecting` 호출 시 post-GTO overflow를 측정해서 `< 0.15` 면 skip 권고하거나 자동 skip. 알고리즘적 변경이라 별도 cycle 필요할 수 있음. 지금은 사용자 명시 선택만 가능

### Phase 4 plan 잔여 (이전 handoff 그대로)
1. **Phase 4.3 VNS** — 신규 N1/N2/N3 neighborhoods. case2/3 #Term 이미 paper 1.5~1.7×까지 좋아져서 가치 측정 후 결정
2. **Phase 4.4 Flattened Init wrapper** — 우리 GPL로 직접 flat init. `runFlattenedPlacement` 이번 refactor에서 삭제됨 — 새로 작성
3. **Phase 4.6 Bilevel Coordinator** — paper Algorithm 1 M=4 SP-1↔SP-2. `BilevelCoordinator` 이전 refactor에서 삭제됨 — Phase 4.6에서 새로 작성
4. **Phase 4.7 Cross-die move (paper §IV.C.2)** — 이전 refactor의 `applyMigration()` helper가 그대로 재사용 가능. `CellsLegalizer::tryMoveCellAcrossDies` stub 채우기
5. **Phase 4.8 from-scratch bench** — 외부 `.par` 없이 `read_iccad2022 → set_3D_IC → run_3d_placement → write` only

### Phase 4.2 simplifications 잔여
- **β·Δo (overlap term, Eq 10b)** 추가 — 현재 dropped
- **multi-bin overflow** — 현재 single-bin per die

### GPU 기여 (별도 path, paper draft 후)
`docs/agents/openroad-gpu-contribution-notes.md` Path A 참고. PR #5352 unblock 합류. 본 작업과 분리.

## 5. 주요 결정사항

| 결정 | 이유 |
|---|---|
| case3 e2e에서 Planar Correcting skip | 8개 knob 조합 실험 (density 0.8~1.5, weight 0.1~1.5, max_iter 200~5000) 모두 Skip보다 나쁨. dense 디자인의 post-GTO 평형점을 Nesterov가 깨뜨림 |
| **case2는 그대로 Planar 유지** | case2 (util 70/75 sparser) 는 Planar 1 iter 으로 HPWL 2.69M (Skip 2.74M 대비 1.5% 좋음). 기존 baseline 그대로 |
| `regression_check.sh` 를 케이스 인자 받도록 일반화 | 두 케이스 baseline이 다른 path (Planar 유무) 라 분리. 추후 case4 추가 시 같은 패턴으로 확장 가능 |
| case4 측정 안 함 (이번 scope 외) | 이번 cycle 목표는 case3 fix. case4는 다음 leverage point 후보 |
| C++ 코드 변경 없음 | 모든 fix가 Tcl/script 수준. 기존 `runPlanarCorrecting` API 그대로. 자동 detection logic은 별도 cycle |
| 8개 knob 실험 모두 Skip보다 나쁨에도 불구하고 case3 default knob을 그대로 둠 | case2에서 default가 winner. case별 권장 path는 regression script가 명시 |

## 6. 주의사항 / 알려진 이슈

### CLAUDE.md 규칙 (이번 cycle 준수 확인)
- `clang-format` 변경 없음 (C++ 수정 안 함)
- `git commit -s` 필수 (DCO)
- `src/sta/` 수정 안 함

### Regression infrastructure 사용법 (확장됨)
```bash
# Smoke (~10초, 매 변경 후)
$ROOT/build/bin/openroad -no_init -no_splash -exit $ROOT/src/mdm/test/test_phase4_smoke.tcl

# Numerical lock case2 (~5분)
bash $ROOT/src/mdm/test/regression_check.sh case2
# 기대: PASS, HPWL 2,694,774

# Numerical lock case3 (~3분, 신규)
bash $ROOT/src/mdm/test/regression_check.sh case3
# 기대: PASS, HPWL 52,217,337

# 둘 다 (~8분)
bash $ROOT/src/mdm/test/regression_check.sh case2 && bash $ROOT/src/mdm/test/regression_check.sh case3
```

### case3 Planar 실험 raw 데이터
모든 실험 로그는 `/tmp/case3_exp_*_run.log`, `/tmp/case3_baseline_run.log`. 추후 paper 작성 시 trajectory 그래프 추출 가능.

### case별 utility (paper 작성 시 참고)
- case2: TopDie 70 / BottomDie 75 (sparser → Planar 도움)
- case3: TopDie 78 / BottomDie 78 (dense → Planar destructive)
- case4: TopDie 66 / BottomDie 70 (sparser → 미검증, case2 패턴 예상)

### 외부 reference 파일 위치 (변동 없음)
- ICCAD 2022 evaluator: `/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`
- ICCAD case files: `/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case{2,3,4}.txt`
- Xueyan reference outputs: `/tmp/case2_ipl.out`, `/tmp/ref_gp/case3_gp.txt`, `/tmp/ref_gp/case4_gp.txt`
- Xueyan partition files: `/tmp/case2_ipl.out.par`, `/tmp/ref_gp/case3_gp.txt.par`, `/tmp/ref_gp/case4_gp.txt.par`
- iPL-3D paper: `paper.pdf` (gitignored)
- archive 참조 구현: `/home/minjae/workspace/etc/openroad/archive/3d_ic/reference_impl/mdm/src/`

### 다음 세션 첫 단계 권고
1. 이 파일 (`handoff.md`) 읽기
2. `bash src/mdm/test/regression_check.sh case2 && bash src/mdm/test/regression_check.sh case3` 실행해서 환경 sanity 확인 (둘 다 0% drift)
3. case4 e2e 측정 (default Planar 1 iter vs Skip) → baseline lock 추가
4. 또는 paper +X% 수치 확정 (Xueyan reference 비교)
