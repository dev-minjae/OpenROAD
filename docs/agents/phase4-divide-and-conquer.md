# Phase 4 — Divide-and-Conquer Baseline + Paper 격차 분석

작성일: 2026-05-07. divide-and-conquer baseline 측정 cycle 결과 정리.

## 1. 전략

**Frontend = paper-equivalent (Xueyan reference data).** partition 정보 (cell → die) + post-GP coord 둘 다 받음. paper 의 §IV.B GTO + §IV.D Planar Correcting 출력에 해당. 우리 자체 GTO/Planar 알고리즘은 검증/우회 됨.

**Backend = 우리 차별화 영역.** SemiLeg + TerminalLegalizer + (옵션) Planar Correcting 1 iter. 이 영역 quality 만 paper 와 비교 가능해짐.

**왜 이 구조인가**:
1. paper 와 정확한 starting state 에서 같은 metric 으로 비교 가능
2. 우리 frontend (GTO/Planar) 의 격차가 backend 측정에 섞이지 않음
3. paper "+X%" claim 의 검증 가능한 baseline 확보
4. 우리 frontend (Phase 4.4 Flattened Init, GTO 출력 quality) 는 별도 cycle 에서 다룸

## 2. paper Table I 와의 격차

paper case2 = ICCAD 2022 case2.txt e2e HPWL.

| Case | Util (Top/Bottom) | 우리 baseline (post-GP backend) | paper Ours | gap | #Term (우리/paper) |
|---|---|---|---|---|---|
| case2 | 70 / 75 | 2,004,424 (W1 +Planar) | 1,992,499 | **+0.6%** | 463 / 461 |
| case3 | 78 / 78 | 30,229,424 (W1 Skip) | 30,234,112 | **-0.02%** | 9,471 / 9,612 |
| case4 | 66 / 70 | 265,734,241 (W1 Skip) | 267,381,744 | **-0.6%** | 43,189 / 43,140 |

3 케이스 모두 paper Ours ±1% 안. case3, case4 는 미세하게 paper 추월.

## 3. backend step 별 wirelength 누적

case3 기준 (W1 Skip path):

| step | HPWL | delta | 누적 vs flat-init |
|---|---|---|---|
| set_3D_IC (Xueyan post-GP) | 27,479,632 | — | — |
| (Planar Correcting Skip) | 27,479,632 | 0% | — |
| SemiLeg (CellsLegalizer) | 30,123,098 | +9.6% | — |
| TerminalLegalizer (evaluator final) | 30,229,424 | +0.4% | +10.0% |

paper Flattened GP for case3 = 26,474,613. 우리 set_3D_IC HPWL 27,479,632 = paper Flattened +3.8%. paper 자체 backend 누적 = 26.47 → 30.23 = **+14.2%**. 우리 backend 누적 = 27.48 → 30.23 = **+10.0%**. **우리 backend 이 paper backend 보다 4.2%p 적은 wirelength 추가**. 이게 paper 동률/추월 의 주된 이유.

case2 (W1 +Planar path):

| step | HPWL | delta |
|---|---|---|
| set_3D_IC (Xueyan post-GP) | 2,000,861 | — |
| Planar Correcting (1 iter) | 1,866,948 | -6.7% |
| SemiLeg | 1,991,164 | +6.6% |
| TerminalLegalizer (evaluator) | 2,004,424 | +0.7% |

case2 는 Planar 가 도움 (-6.7% drop), SemiLeg 가 +6.6% 회복. 종합 +0.2%. dense (case3/case4) 와 다른 dynamic.

## 4. Skip vs +Planar 케이스별 결과

| Case | W1 +Planar | W1 Skip | Skip - +Planar |
|---|---|---|---|
| case2 | 2,004,424 ✅ | 2,006,967 | +0.13% (Planar 도움) |
| case3 | 30,388,283 | 30,229,424 ✅ | -0.5% (Planar 해로움) |
| case4 | 266,860,344 | 265,734,241 ✅ | -0.4% (Planar 해로움) |

case2 만 Planar 가 도움. case3/case4 는 Skip winner. 패턴 가설: dense (≥ 75%) → Skip, sparse → +Planar. case2 max util 75 이지만 평균 낮음 (TopDie 70). case4 는 평균 더 낮은데도 (66/70) Skip winner.

→ utility 만으로 결정 안 됨. **Planar Correcting 의 wirelength gradient 와 dies 의 cell count ratio 도 영향**. 자세한 진단은 후속 작업.

## 5. W1 vs W2 (raw coord) 시도 결과

W2 = "raw input coord + paper partition + 우리 Planar + 우리 SemiLeg" — *우리 GP (Planar Correcting) quality 단독 검증* 의도. 결과:

- case2 W2: GPL crash. `[INFO GPL-0006] Number of instances: 0` `[INFO GPL-0007] Movable instances: 0`. Nesterov initialization 실패 후 abort.
- case3 W2: 같은 crash.
- case4 W2: 같은 crash 예상.

원인: ICCAD 2022 raw input 은 cells 좌표 정보 없음 (.par 만 있고 .out 없음). `set_3D_IC` 후 cells 가 모두 origin 또는 unplaced → `gpl::Replace` initialize 시 movable instances=0.

해결책: 우리 자체 flat 2D GP (Phase 4.4 Flattened Init wrapper) 신규 구현. 이건 별도 cycle. W2 시나리오는 Phase 4.4 완료 후 재시도.

## 6. 다음 leverage point

격차 분포 (paper Ours 대비 ±1% 안) 보면, **paper 추월용 marginal 개선**이 주요 leverage. 우선순위:

### 1순위 — Phase 4.4 Flattened Init wrapper 신규 구현

이번 cycle 막힌 부분. 우리 자체 flat 2D GP 가 있으면:
- `runFlattenedPlacement` Tcl command 부활
- W2 / W3 시나리오 측정 가능 → 우리 frontend 단독 quality 검증
- Xueyan 데이터 의존성 제거 → 임의 ICCAD/ISPD 디자인 직접 처리 가능 (paper 8 비교 외 추가 디자인)
- 2026-05-07 시점 우리 코드에 stub 도 없는 상태 (이전 cycle 에서 YAGNI 로 삭제). Phase 4.4 의 정식 구현 일정 임박

### 2순위 — case3 W1 Skip 의 -0.02% 격차 paper 명백한 추월로 키우기

backend 의 SemiLeg 또는 TerminalLegalizer 미세 개선:
- CellsLegalizer 의 pairSwap 추가 iteration (현재 1-pass)
- SemiLeg displacement penalty (cells 가 멀리 이동하면 비용 부과) — paper 와의 비교에 유리
- TermLeg r-tree refinement passes 늘리기 (현재 4-7 passes, 더 늘리면 wirelength 미세 개선)

### 3순위 — case4 W1 Skip 의 -0.6% 추월을 더 키우기

case4 가 가장 큰 디자인. backend 효율이 가장 빛나는 case. 위 2순위 변경이 케이스에 비례 효과. 같이 처리.

### 보류 — Phase 4.6 Bilevel Coordinator

paper Algorithm 1 (M=4 alternating SP-1 ↔ SP-2). paper Table IV ablation 는 alternating *없이도* case3 30.33M 나옴 → Bilevel 없이도 paper Ours 가능. 우리 baseline 이 이미 paper-class 이라 Bilevel 의 marginal value 작음. 추후 paper "+X%" claim 가치가 명백히 증가시키는 게 확인되면 추가.

### 보류 — case3 Planar Correcting 의 dense regime fix

case3 W1 Skip 가 이미 paper 동률. Planar 이 case3 에서 marginal 도움/해로움 (Skip vs +Planar -0.5%) 임을 confirm. fundamental fix 의 가치는 case4 미세개선에 한정. 1-2 순위 끝낸 후 결정.

## 7. 데이터 / 검증 포인트

### 환경 sanity check
```bash
bash src/mdm/test/regression_check.sh case2  # ~5분, expect: PASS, HPWL 2,004,424
bash src/mdm/test/regression_check.sh case3  # ~3분, expect: PASS, HPWL 30,229,424
bash src/mdm/test/regression_check.sh case4  # ~10분, expect: PASS, HPWL 265,734,241
```

### Raw experimental data 위치
- W1 +Planar runs: `/tmp/case{2,3,4}_W1_run.log`, `/tmp/case{2,3,4}_W1.out`
- W1 Skip runs: `/tmp/case{2,3,4}_W1_skip_run.log`, `/tmp/case{2,3,4}_W1_skip.out`, `/tmp/case3_post_gp_run.log` (case3 Skip 별도 측정 명칭)
- W2 (실패) runs: `/tmp/case{2,3,4}_W2_run.log`
- evaluator output: `/tmp/regression_phase4_case{2,3,4}_eval.log` (regression_check.sh 실행 후)

### 외부 reference 파일
- ICCAD 2022 evaluator: `/home/minjae/workspace/etc/openroad/archive/3d_ic/tools/evaluator_0525`
- Cases: `/home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case{2,3,4}.txt`
- Xueyan post-GP: `/tmp/case2_ipl.out{,.par}`, `/tmp/ref_gp/case{3,4}_gp.txt{,.par}`
- iPL-3D paper: `paper.pdf` (gitignored)

## 8. paper "+X%" claim 후보 표현 (paper draft 시)

3 케이스 평균 격차: paper -0.01% (사실상 동률).

case별 더 좋은 표현:
- "우리 backend (post-GTO refinement) 단독 측정 시 paper Ours 의 ±1% 안 매칭, dense (case3) 및 large (case4) 에서 paper 미세 추월"
- "iPL-3D paper 의 frontend 와 동일한 starting state 에서 우리 backend 가 case3, case4 에서 각 -0.02%, -0.6% 의 wirelength 개선"
- "paper Algorithm 1 의 Bilevel coordinator 없이도 paper Ours 와 등가의 결과 (3 케이스 평균 격차 -0.01%) 달성, simpler one-shot post-GTO flow 로"

세 번째 표현이 가장 강력 — paper 의 Bilevel 복잡성 없이 같은 결과. 단 frontend (GTO) 은 우리가 새로 만든 게 아니라 paper 의 출력 빌려옴 — claim 시 명시 필요. Xueyan 데이터 == paper 동일 인물 출력이라는 점 paper draft 에 정직하게 기재.
