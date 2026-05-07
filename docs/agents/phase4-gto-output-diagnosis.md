# Phase 4 — GTO 출력 Quality 진단

작성일: 2026-05-07.

비교 baseline: paper post-GP (Xueyan reference data — paper-equivalent
partition + GP coord). 측정 대상: 우리 flat init → GTO → [Planar?]
의 cells 좌표 분포 (5 metrics × 3 cases × 2 variants = 30 datapoints).


## case2 — post-gto

**Partition disagreement**: 25.96% (710 cells out of 2735 total)

**Displacement** (same-die cells, 2025 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 0 |
| P90  | 0 |
| P99  | 0 |
| max  | 0 |
| mean | 0 |
| std  | 0 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 2025
      1-2k  0
      2-5k  0
     5-10k  0
    10-20k  0
    20-50k  0
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 79.9% aligned, avg misalign 8.9
**Row-alignment** (paper): 79.9% aligned, avg misalign 8.9

**Die cluster** (TopDie, ours): peak/avg = 7.07, entropy 6.96 nats
**Die cluster** (TopDie, paper): peak/avg = 4.60, entropy 7.10 nats
**Die cluster** (BottomDie, ours): peak/avg = 20.42, entropy 6.04 nats
**Die cluster** (BottomDie, paper): peak/avg = 13.39, entropy 6.10 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C950 | 2 | MC149 | 66528 | 0 |
| C554 | 3 | MC77 | 42336 | 0 |
| C2573 | 2 | MC6 | 18396 | 0 |
| C542 | 2 | MC56 | 18144 | 0 |
| C1272 | 3 | MC153 | 25956 | 0 |
| C2731 | 1 | MC250 | 12600 | 0 |
| C1580 | 3 | MC110 | 40824 | 0 |
| C1314 | 3 | MC64 | 50400 | 0 |
| C1867 | 3 | MC110 | 40824 | 0 |
| C2277 | 5 | MC118 | 42588 | 0 |
| C181 | 3 | MC50 | 89460 | 0 |
| C232 | 3 | MC47 | 25452 | 0 |
| C1708 | 3 | MC110 | 40824 | 0 |
| C1136 | 3 | MC44 | 34524 | 0 |
| C396 | 2 | MC55 | 17136 | 0 |
| C744 | 2 | MC107 | 50652 | 0 |
| C1387 | 3 | MC123 | 82404 | 0 |
| C890 | 3 | MC140 | 24696 | 0 |
| C666 | 3 | MC64 | 50400 | 0 |
| C2180 | 3 | MC47 | 25452 | 0 |

---


## case2 — post-planar

**Partition disagreement**: 25.96% (710 cells out of 2735 total)

**Displacement** (same-die cells, 2025 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 46 |
| P90  | 99 |
| P99  | 143 |
| max  | 200 |
| mean | 50 |
| std  | 36 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 2025
      1-2k  0
      2-5k  0
     5-10k  0
    10-20k  0
    20-50k  0
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 16.7% aligned, avg misalign 33.1
**Row-alignment** (paper): 79.9% aligned, avg misalign 8.9

**Die cluster** (TopDie, ours): peak/avg = 8.24, entropy 6.90 nats
**Die cluster** (TopDie, paper): peak/avg = 4.60, entropy 7.10 nats
**Die cluster** (BottomDie, ours): peak/avg = 20.42, entropy 6.04 nats
**Die cluster** (BottomDie, paper): peak/avg = 13.39, entropy 6.10 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C1634 | 2 | MC67 | 25704 | 200 |
| C2666 | 1 | MC250 | 12600 | 188 |
| C2669 | 1 | MC250 | 12600 | 178 |
| C2678 | 1 | MC250 | 12600 | 163 |
| C1333 | 3 | MC115 | 81648 | 163 |
| C2552 | 2 | MC55 | 17136 | 163 |
| C2644 | 1 | MC250 | 12600 | 161 |
| C2649 | 1 | MC250 | 12600 | 161 |
| C1833 | 3 | MC110 | 40824 | 159 |
| C86 | 2 | MC36 | 153468 | 158 |
| C584 | 2 | MC106 | 32760 | 156 |
| C2720 | 1 | MC250 | 12600 | 155 |
| C2701 | 1 | MC250 | 12600 | 153 |
| C2652 | 1 | MC250 | 12600 | 152 |
| C2659 | 1 | MC250 | 12600 | 152 |
| C2685 | 1 | MC250 | 12600 | 150 |
| C1563 | 2 | MC67 | 25704 | 148 |
| C446 | 2 | MC85 | 33768 | 145 |
| C2350 | 2 | MC6 | 18396 | 144 |
| C2728 | 1 | MC250 | 12600 | 143 |

---


## case3 — post-gto

**Partition disagreement**: 33.57% (15027 cells out of 44764 total)

**Displacement** (same-die cells, 29737 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 0 |
| P90  | 0 |
| P99  | 0 |
| max  | 0 |
| mean | 0 |
| std  | 0 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 29737
      1-2k  0
      2-5k  0
     5-10k  0
    10-20k  0
    20-50k  0
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 2.7% aligned, avg misalign 28.8
**Row-alignment** (paper): 2.7% aligned, avg misalign 28.8

**Die cluster** (TopDie, ours): peak/avg = 3.37, entropy 7.27 nats
**Die cluster** (TopDie, paper): peak/avg = 2.23, entropy 7.56 nats
**Die cluster** (BottomDie, ours): peak/avg = 6.59, entropy 7.14 nats
**Die cluster** (BottomDie, paper): peak/avg = 3.85, entropy 7.37 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C24263 | 3 | MC9 | 7590 | 0 |
| C22001 | 3 | MC9 | 7590 | 0 |
| C14579 | 3 | MC3 | 6325 | 0 |
| C18706 | 3 | MC3 | 6325 | 0 |
| C15834 | 3 | MC3 | 6325 | 0 |
| C22012 | 3 | MC3 | 6325 | 0 |
| C31403 | 3 | MC6 | 10465 | 0 |
| C31662 | 5 | MC5 | 13225 | 0 |
| C9000 | 7 | MC31 | 25415 | 0 |
| C14463 | 3 | MC6 | 10465 | 0 |
| C11833 | 2 | MC2 | 5635 | 0 |
| C34028 | 2 | MC2 | 5635 | 0 |
| C7158 | 2 | MC2 | 5635 | 0 |
| C13411 | 2 | MC2 | 5635 | 0 |
| C22827 | 3 | MC9 | 7590 | 0 |
| C21300 | 3 | MC3 | 6325 | 0 |
| C25370 | 3 | MC9 | 7590 | 0 |
| C8886 | 7 | MC31 | 25415 | 0 |
| C27824 | 3 | MC9 | 7590 | 0 |
| C13995 | 3 | MC9 | 7590 | 0 |

---


## case3 — post-planar

**Partition disagreement**: 33.57% (15027 cells out of 44764 total)

**Displacement** (same-die cells, 29737 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 1457 |
| P90  | 4180 |
| P99  | 7293 |
| max  | 15996 |
| mean | 1805 |
| std  | 1743 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 11707
      1-2k ███████████████████████ 6752
      2-5k ████████████████████████████████ 9624
     5-10k █████ 1618
    10-20k  36
    20-50k  0
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 2.7% aligned, avg misalign 28.8
**Row-alignment** (paper): 2.7% aligned, avg misalign 28.8

**Die cluster** (TopDie, ours): peak/avg = 4.24, entropy 7.45 nats
**Die cluster** (TopDie, paper): peak/avg = 2.23, entropy 7.56 nats
**Die cluster** (BottomDie, ours): peak/avg = 6.59, entropy 7.14 nats
**Die cluster** (BottomDie, paper): peak/avg = 3.85, entropy 7.37 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C11276 | 2 | MC2 | 5635 | 15996 |
| C11271 | 2 | MC2 | 5635 | 15604 |
| C11441 | 3 | MC3 | 6325 | 15426 |
| C11273 | 3 | MC3 | 6325 | 14505 |
| C11279 | 2 | MC2 | 5635 | 13103 |
| C11265 | 2 | MC2 | 5635 | 12732 |
| C16735 | 3 | MC9 | 7590 | 12724 |
| C16890 | 3 | MC9 | 7590 | 12664 |
| C16883 | 3 | MC3 | 6325 | 12412 |
| C16881 | 3 | MC9 | 7590 | 12411 |
| C16148 | 2 | MC2 | 5635 | 11793 |
| C16884 | 3 | MC3 | 6325 | 11745 |
| C16878 | 3 | MC9 | 7590 | 11686 |
| C16891 | 3 | MC9 | 7590 | 11451 |
| C16771 | 3 | MC9 | 7590 | 11190 |
| C11263 | 3 | MC3 | 6325 | 11149 |
| C11272 | 2 | MC2 | 5635 | 11105 |
| C11281 | 3 | MC3 | 6325 | 10983 |
| C11266 | 3 | MC3 | 6325 | 10810 |
| C6595 | 2 | MC2 | 5635 | 10684 |

---


## case4 — post-gto

**Partition disagreement**: 22.27% (49184 cells out of 220845 total)

**Displacement** (same-die cells, 171661 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 0 |
| P90  | 0 |
| P99  | 0 |
| max  | 0 |
| mean | 0 |
| std  | 0 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 171661
      1-2k  0
      2-5k  0
     5-10k  0
    10-20k  0
    20-50k  0
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 3.3% aligned, avg misalign 22.9
**Row-alignment** (paper): 3.3% aligned, avg misalign 22.9

**Die cluster** (TopDie, ours): peak/avg = 4.22, entropy 7.26 nats
**Die cluster** (TopDie, paper): peak/avg = 3.06, entropy 7.38 nats
**Die cluster** (BottomDie, ours): peak/avg = 7.95, entropy 7.08 nats
**Die cluster** (BottomDie, paper): peak/avg = 5.20, entropy 7.38 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C43201 | 3 | MC101 | 11385 | 0 |
| C163708 | 5 | MC333 | 35075 | 0 |
| C82688 | 3 | MC34 | 7475 | 0 |
| C158067 | 3 | MC124 | 8740 | 0 |
| C157200 | 3 | MC34 | 7475 | 0 |
| C62693 | 2 | MC12 | 5060 | 0 |
| C137684 | 5 | MC30 | 11270 | 0 |
| C197319 | 4 | MC38 | 9315 | 0 |
| C205565 | 2 | MC25 | 6210 | 0 |
| C204381 | 3 | MC34 | 7475 | 0 |
| C171804 | 3 | MC34 | 7475 | 0 |
| C218361 | 3 | MC34 | 7475 | 0 |
| C147620 | 5 | MC30 | 11270 | 0 |
| C118486 | 3 | MC34 | 7475 | 0 |
| C124563 | 5 | MC30 | 11270 | 0 |
| C179129 | 5 | MC43 | 10580 | 0 |
| C173820 | 3 | MC124 | 8740 | 0 |
| C193317 | 5 | MC30 | 11270 | 0 |
| C9116 | 5 | MC276 | 47955 | 0 |
| C23428 | 4 | MC38 | 9315 | 0 |

---


## case4 — post-planar

**Partition disagreement**: 22.27% (49184 cells out of 220845 total)

**Displacement** (same-die cells, 171661 instances):

| metric | value (ICCAD units) |
|---|---|
| P50  | 2104 |
| P90  | 11388 |
| P99  | 22951 |
| max  | 32597 |
| mean | 4166 |
| std  | 5378 |

Histogram (Euclidean):
```
      0-1k ████████████████████████████████████████ 63381
      1-2k █████████████ 20633
      2-5k ████████████████████████ 38849
     5-10k █████████████████ 27325
    10-20k ██████████ 16758
    20-50k ██ 4715
   50-100k  0
     100k+  0
```

**Row-alignment** (ours): 3.2% aligned, avg misalign 23.0
**Row-alignment** (paper): 3.3% aligned, avg misalign 22.9

**Die cluster** (TopDie, ours): peak/avg = 2.39, entropy 7.77 nats
**Die cluster** (TopDie, paper): peak/avg = 3.06, entropy 7.38 nats
**Die cluster** (BottomDie, ours): peak/avg = 7.95, entropy 7.08 nats
**Die cluster** (BottomDie, paper): peak/avg = 5.20, entropy 7.38 nats

**Top-20 displacement cells**:

| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C77727 | 3 | MC99 | 14835 | 32597 |
| C77731 | 3 | MC34 | 7475 | 32597 |
| C78114 | 3 | MC34 | 7475 | 32586 |
| C78038 | 5 | MC30 | 11270 | 32584 |
| C77967 | 4 | MC304 | 25530 | 32584 |
| C78113 | 3 | MC34 | 7475 | 32570 |
| C85044 | 2 | MC11 | 7820 | 31411 |
| C84998 | 2 | MC67 | 9775 | 31405 |
| C85833 | 2 | MC130 | 72220 | 31382 |
| C87605 | 2 | MC21 | 27715 | 31247 |
| C86665 | 4 | MC93 | 127880 | 31209 |
| C78103 | 3 | MC34 | 7475 | 31190 |
| C78170 | 3 | MC32 | 19090 | 31189 |
| C78040 | 5 | MC30 | 11270 | 31184 |
| C78104 | 3 | MC34 | 7475 | 31179 |
| C78102 | 3 | MC34 | 7475 | 31174 |
| C78162 | 4 | MC29 | 10465 | 31173 |
| C69966 | 3 | MC34 | 7475 | 30716 |
| C69894 | 5 | MC30 | 11270 | 30699 |
| C69965 | 3 | MC34 | 7475 | 30695 |

---


## Conclusion

### 1. 측정 design 의 degenerate 측면 (post-GTO 변형)

post-GTO 변형의 **displacement = 0** 이 모든 case 에서. 이는 우리 GTO 가 cells 좌표를 *옮기지 않고* partition flip 만 하기 때문. Tcl flow 의 `parse_iccad2022_output` 가 cells 에 paper post-GP coord 을 부여 → flat partition 강제 → GTO 는 partition migrate 만, 좌표 그대로. 결과: same-die cells 의 우리 좌표 = paper 좌표 (정확히 동일) → 0 displacement, row-alignment 도 paper 와 동일. **post-GTO 변형의 informative metric 은 partition disagreement + die cluster 두 개.**

post-Planar 변형은 Planar Correcting (Nesterov) 이 좌표를 옮기므로 displacement 비제로.

### 2. 패턴 요약

**(1) Partition disagreement** — 모든 case 에서 22-34%:
- case2: 25.96% (710 cells)
- case3: 33.57% (15,027 cells, 가장 큼)
- case4: 22.27% (49,184 cells)
→ 우리 GTO 가 paper 와 약 1/4~1/3 cells 의 die 배정 다르게 결정. 이전 cycle 의 SemiLeg overhead +84.6% (case3) 의 직접 원인 candidate.

**(2) Displacement (post-Planar)** — Planar 가 cells 옮기는 magnitude:
- case2: P50=46, max=200 (sparse → Planar movement 작음)
- case3: P50=1457, max=15996 (dense → 5-30× 더 많이 이동)
- case4: P50=2104, max=32597 (가장 큼, large + sparser)
→ Planar 가 dense/large case 에서 더 적극적으로 cells 재배치. case3 의 cells 가 paper coord 시작점에서 평균 1457 units 이동 = paper 좌표가 우리 GTO partition + 우리 die geometry 와 호환 안 됨 (Planar 가 큰 폭으로 정정 시도).

**(3) Row-alignment** — post-Planar 가 row 에서 cells 떼어냄:
- case2: ours 79.9% (post-GTO, paper 와 동일) → 16.7% (post-Planar). **Planar 가 case2 의 row 정렬을 깨뜨림**.
- case3/4: post-GTO 와 paper 모두 ~3% (raw post-GP 는 row-aligned 아님), post-Planar 도 ~3% 비슷. case3/4 는 row 정렬 손상 무관.
→ Planar Correcting 이 row 보존 안 함 (Nesterov 는 row 모름). dense case 는 처음부터 row-aligned 아니라 영향 없음, sparse case2 는 손해.

**(4) Die cluster (peak/avg + entropy)** — 모든 case 에서 우리 ours > paper:
- case2 TopDie: ours peak/avg 7.07 vs paper 4.60 (우리 53% 더 cluster)
- case3 TopDie: 3.37 vs 2.23 (51% 더)
- case3 BottomDie: 6.59 vs 3.85 (71% 더)
- case4 TopDie: 4.22 vs 3.06 (38% 더)
- case4 BottomDie: 7.95 vs 5.20 (53% 더)
→ **모든 case 에서 일관: 우리 GTO 출력은 paper 보다 cells 가 약 40-70% 더 cluster**. paper 는 더 균일한 분포. entropy 도 우리 항상 lower (less uniform).

post-Planar 변형은 die cluster 에 mixed effect:
- case2 TopDie: 7.07 → 8.24 (Planar 가 cluster 더 심하게 만듦)
- case3 TopDie: 3.37 → 4.24 (Planar 가 약간 더 cluster)
- case4 TopDie: 4.22 → 2.39 (Planar 가 cluster 줄임 — 유일한 case)
→ Planar Correcting 이 cluster 를 줄여주는 건 case4 만. 작은/dense case 에서는 오히려 cluster 키움.

**(5) Big-displacement cells (post-Planar)** — pattern:
- 모든 case 에서 fanout = 1-3 (가벼운 connectivity)
- lib_cell area 도 평균보다 약간 큰 정도 (huge macros 아님)
- → 큰 변위 cells 의 공통점 *없음*. 특정 lib_cell type 만 큰 변위 받는 게 아님 (bulk 적인 movement).
- case3 의 큰 변위 cells 는 small standard cells (MC2/MC3, area 5-7k) 가 dominant — small cells 가 dense 환경에서 squeeze out 되는 패턴.

### 3. 다음 cycle leverage point 추천

#### 1순위 — **GTO output 의 die cluster 분포 paper 화**

근거: 모든 case 에서 우리 ours > paper 약 40-70% 더 cluster (peak/avg). entropy 도 우리 항상 lower. 이 cluster 가 SemiLeg 가 cells 를 spread 시키는데 큰 비용 야기 (이전 cycle case3 +84.6% overhead 의 핵심 원인 후보).

해법 후보:
- **GTO migration 시 displacement penalty** — paper 의 §IV.B Algorithm 2 가 단순 partition flip 인지, displacement 도 고려하는 더 정교한 algorithm 인지 archive reference_impl 비교
- **multi-bin overflow** (Phase 4.2 잔여) — 현재 single-bin per die. paper §IV.B-3 의 region grid 도입하면 cluster 분포가 자연히 균일화

#### 2순위 — **Partition disagreement 22-34% 줄이기**

근거: 우리 와 paper 의 die 배정 disagreement 가 case3 33.57%, 1.5만 cells. 이 disagree cells 가 잘못된 die 에 가서 SemiLeg 가 push around 함.

해법 후보:
- archive `reference_impl/mdm/src/` 와 우리 `GlobalTierOptimizer.cpp` line-by-line diff. b_factor (1.0 vs paper 1.1?), alpha/beta/gamma defaults, knapsack scoring criteria 차이 발견 가능
- paper Algorithm 2 의 cell migration 결정 방식 의 미세 차이 분석

#### 3순위 — **Planar Correcting 의 row 보존 문제 (case2 만 영향)**

case2 sparse 환경에서 Planar 가 79.9% → 16.7% 로 row 정렬 깨뜨림. SemiLeg 가 cells 를 다시 row 에 snap 해야 하므로 추가 displacement 비용.

해법: Planar Correcting 후 `snapCellsToRows` 자동 호출 (이미 우리 코드에 구현됨, 사용자 explicit 호출 필요). 또는 Planar Nesterov 의 cost function 에 row-snap term 추가.

#### 보류 — Phase 4.4 Flattened Init

이번 진단으로 보면 *frontend (flat 2D GP) 가 본질적 leverage point 아님*. paper coord 받아서도 우리 GTO + Planar 가 cluster + row 깨뜨림 → 진짜 leverage 는 GTO/Planar 알고리즘 자체. Phase 4.4 는 외부 데이터 의존성 제거의 가치 (별도 motivation) 는 있지만, paper 추월의 직접 leverage 아님.

### 4. handoff.md 의 §4 갱신 권고

기존 §4 의 1순위 ("Phase 4.4 Flattened Init wrapper") → 2순위로 강등. 새 1순위:
"GTO output cluster 분포 paper 화 — multi-bin overflow + displacement penalty 도입 (archive reference_impl 비교 우선)".
