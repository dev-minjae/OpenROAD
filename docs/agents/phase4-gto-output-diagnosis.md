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
| C2279 | 0 | MC56 | 18144 | 0 |
| C1869 | 0 | MC110 | 40824 | 0 |
| C137 | 0 | MC45 | 24696 | 0 |
| C1132 | 0 | MC157 | 48636 | 0 |
| C1143 | 0 | MC183 | 82404 | 0 |
| C1660 | 0 | MC120 | 96768 | 0 |
| C289 | 0 | MC49 | 42084 | 0 |
| C1108 | 0 | MC178 | 74340 | 0 |
| C1897 | 0 | MC110 | 40824 | 0 |
| C1216 | 0 | MC170 | 66780 | 0 |
| C468 | 0 | MC47 | 25452 | 0 |
| C1963 | 0 | MC110 | 40824 | 0 |
| C1238 | 0 | MC58 | 49896 | 0 |
| C1536 | 0 | MC67 | 25704 | 0 |
| C1506 | 0 | MC60 | 49140 | 0 |
| C385 | 0 | MC47 | 25452 | 0 |
| C897 | 0 | MC55 | 17136 | 0 |
| C2178 | 0 | MC192 | 42084 | 0 |
| C543 | 0 | MC45 | 24696 | 0 |
| C2708 | 0 | MC250 | 12600 | 0 |

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
| C1634 | 0 | MC67 | 25704 | 200 |
| C2666 | 0 | MC250 | 12600 | 188 |
| C2669 | 0 | MC250 | 12600 | 178 |
| C2678 | 0 | MC250 | 12600 | 163 |
| C1333 | 0 | MC115 | 81648 | 163 |
| C2552 | 0 | MC55 | 17136 | 163 |
| C2644 | 0 | MC250 | 12600 | 161 |
| C2649 | 0 | MC250 | 12600 | 161 |
| C1833 | 0 | MC110 | 40824 | 159 |
| C86 | 0 | MC36 | 153468 | 158 |
| C584 | 0 | MC106 | 32760 | 156 |
| C2720 | 0 | MC250 | 12600 | 155 |
| C2701 | 0 | MC250 | 12600 | 153 |
| C2652 | 0 | MC250 | 12600 | 152 |
| C2659 | 0 | MC250 | 12600 | 152 |
| C2685 | 0 | MC250 | 12600 | 150 |
| C1563 | 0 | MC67 | 25704 | 148 |
| C446 | 0 | MC85 | 33768 | 145 |
| C2350 | 0 | MC6 | 18396 | 144 |
| C2728 | 0 | MC250 | 12600 | 143 |

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
| C6573 | 0 | MC3 | 6325 | 0 |
| C19380 | 0 | MC9 | 7590 | 0 |
| C10370 | 0 | MC3 | 6325 | 0 |
| C25540 | 0 | MC2 | 5635 | 0 |
| C27992 | 0 | MC2 | 5635 | 0 |
| C18191 | 0 | MC2 | 5635 | 0 |
| C8459 | 0 | MC5 | 13225 | 0 |
| C39102 | 0 | MC2 | 5635 | 0 |
| C12928 | 0 | MC8 | 11385 | 0 |
| C6569 | 0 | MC3 | 6325 | 0 |
| C18867 | 0 | MC3 | 6325 | 0 |
| C14896 | 0 | MC9 | 7590 | 0 |
| C2585 | 0 | MC13 | 13915 | 0 |
| C28054 | 0 | MC8 | 11385 | 0 |
| C13242 | 0 | MC3 | 6325 | 0 |
| C32026 | 0 | MC5 | 13225 | 0 |
| C34589 | 0 | MC3 | 6325 | 0 |
| C8089 | 0 | MC5 | 13225 | 0 |
| C31902 | 0 | MC5 | 13225 | 0 |
| C14145 | 0 | MC4 | 14145 | 0 |

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
| C11276 | 0 | MC2 | 5635 | 15996 |
| C11271 | 0 | MC2 | 5635 | 15604 |
| C11441 | 0 | MC3 | 6325 | 15426 |
| C11273 | 0 | MC3 | 6325 | 14505 |
| C11279 | 0 | MC2 | 5635 | 13103 |
| C11265 | 0 | MC2 | 5635 | 12732 |
| C16735 | 0 | MC9 | 7590 | 12724 |
| C16890 | 0 | MC9 | 7590 | 12664 |
| C16883 | 0 | MC3 | 6325 | 12412 |
| C16881 | 0 | MC9 | 7590 | 12411 |
| C16148 | 0 | MC2 | 5635 | 11793 |
| C16884 | 0 | MC3 | 6325 | 11745 |
| C16878 | 0 | MC9 | 7590 | 11686 |
| C16891 | 0 | MC9 | 7590 | 11451 |
| C16771 | 0 | MC9 | 7590 | 11190 |
| C11263 | 0 | MC3 | 6325 | 11149 |
| C11272 | 0 | MC2 | 5635 | 11105 |
| C11281 | 0 | MC3 | 6325 | 10983 |
| C11266 | 0 | MC3 | 6325 | 10810 |
| C6595 | 0 | MC2 | 5635 | 10684 |

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
| C123166 | 0 | MC117 | 26450 | 0 |
| C176993 | 0 | MC45 | 8510 | 0 |
| C19828 | 0 | MC38 | 9315 | 0 |
| C214327 | 0 | MC179 | 21850 | 0 |
| C196452 | 0 | MC31 | 6785 | 0 |
| C82063 | 0 | MC35 | 9775 | 0 |
| C174155 | 0 | MC25 | 6210 | 0 |
| C14858 | 0 | MC38 | 9315 | 0 |
| C92511 | 0 | MC228 | 15180 | 0 |
| C49899 | 0 | MC120 | 21965 | 0 |
| C81972 | 0 | MC124 | 8740 | 0 |
| C53204 | 0 | MC102 | 14605 | 0 |
| C100260 | 0 | MC24 | 9430 | 0 |
| C95134 | 0 | MC23 | 8165 | 0 |
| C90595 | 0 | MC231 | 10925 | 0 |
| C129811 | 0 | MC109 | 12075 | 0 |
| C97106 | 0 | MC38 | 9315 | 0 |
| C114263 | 0 | MC197 | 12075 | 0 |
| C138854 | 0 | MC285 | 48875 | 0 |
| C168882 | 0 | MC48 | 10005 | 0 |

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
| C77727 | 0 | MC99 | 14835 | 32597 |
| C77731 | 0 | MC34 | 7475 | 32597 |
| C78114 | 0 | MC34 | 7475 | 32586 |
| C78038 | 0 | MC30 | 11270 | 32584 |
| C77967 | 0 | MC304 | 25530 | 32584 |
| C78113 | 0 | MC34 | 7475 | 32570 |
| C85044 | 0 | MC11 | 7820 | 31411 |
| C84998 | 0 | MC67 | 9775 | 31405 |
| C85833 | 0 | MC130 | 72220 | 31382 |
| C87605 | 0 | MC21 | 27715 | 31247 |
| C86665 | 0 | MC93 | 127880 | 31209 |
| C78103 | 0 | MC34 | 7475 | 31190 |
| C78170 | 0 | MC32 | 19090 | 31189 |
| C78040 | 0 | MC30 | 11270 | 31184 |
| C78104 | 0 | MC34 | 7475 | 31179 |
| C78102 | 0 | MC34 | 7475 | 31174 |
| C78162 | 0 | MC29 | 10465 | 31173 |
| C69966 | 0 | MC34 | 7475 | 30716 |
| C69894 | 0 | MC30 | 11270 | 30699 |
| C69965 | 0 | MC34 | 7475 | 30695 |

---


## Conclusion (manual)

(다음 cycle 의 leverage point 결정에 사용. 6 metric chunks 검토 후 패턴 식별:
어떤 metric 이 가장 큰 격차? GTO → +Planar 가 격차를 줄이는가? case 별 패턴
일관성? 결론 내려서 이 section 채우기.)
