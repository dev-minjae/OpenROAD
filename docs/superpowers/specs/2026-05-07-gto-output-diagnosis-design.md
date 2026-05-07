# GTO 출력 Quality 진단 — Design Spec

작성일: 2026-05-07.

## Goal

우리 GTO (Global Tier Optimization, paper §IV.B Algorithm 2) 출력 의 cells 좌표 분포가 paper post-GP 와 어떻게 다른지 instance-level 정량화. 차이의 *모양* (uniform shift / random scatter / cluster / row misalign) 식별. **다음 cycle 의 leverage point** (GTO 자체 fix vs Planar fix vs Multi-Tier 추가) 결정에 데이터 제공.

진단 결과는 D2 depth (handoff brainstorming 결정): 4 metric (displacement / row-alignment / cluster / big-disp breakdown) + partition disagreement = 5 metric. case2/3/4 모두. 각 case 의 GTO 출력 + GTO+Planar 출력 둘 다 비교 (variant trajectory: GTO → Planar 가 격차 좁히는지).

## Architecture

```
[Tcl dumps × 6]              [Python analyzer × 6]            [Markdown report]
 case{2,3,4} ×                compare ours vs paper      →     phase4-gto-output-
   {post-GTO, post-Planar}    5 metric per call          →     diagnosis.md
                                                                (24 entries +
                                                                 conclusion)
```

3 stage flow:
1. Tcl dumps run our flow with flat init + GTO (and optionally Planar) → save `.out` files
2. Python analyzer parses both `.out` (ours + Xueyan paper post-GP), matches instances by name, computes 5 metrics, prints markdown chunk
3. Bash wrapper executes 6 Tcl scripts + 6 analyzer calls + concatenates chunks into final report

## File Structure (신규)

```
src/mdm/test/diagnose/
├── dump_case2_post_gto.tcl        # 6 dump scripts. Pattern identical
├── dump_case2_post_planar.tcl     #   except case file path + variant (1 line)
├── dump_case3_post_gto.tcl
├── dump_case3_post_planar.tcl
├── dump_case4_post_gto.tcl
├── dump_case4_post_planar.tcl
├── analyze_displacement.py         # Single Python file, argparse + 5 metric
└── run_diagnosis.sh                # Bash orchestrator
docs/agents/phase4-gto-output-diagnosis.md   # Final aggregated report
```

## 5 Metric 정의

### 1. Partition disagreement
- 우리 가 die A 에 둔 cell 을 paper 는 die B 에 둔 비율.
- 계산: `|symmetric_diff(our_top, paper_top)| / |union(our_top, paper_top)|`
- 출력: `case3 GTO: N% (M cells out of total)`

### 2. Displacement (Euclidean)
- same-die cells (`our_top ∩ paper_top`, `our_bottom ∩ paper_bottom`) 만 비교.
- per-cell: `Δ = sqrt((x_ours - x_paper)² + (y_ours - y_paper)²)` (ICCAD units, will note unit)
- 통계: P50, P90, P99, max, mean, std
- ASCII histogram, 10 log-scaled bins

### 3. Row-alignment
- 각 cell 의 `y_misalign = y mod row_height`. `row_height` = ICCAD case file 의 `TopDieRows`/`BottomDieRows` 5번째 column.
- aligned = `y_misalign < 1` (rounding tolerance)
- 출력: aligned %, avg `y_misalign`

### 4. Die cluster
- bin grid 50×50 per die. die size = ICCAD `TopDieRows` 의 (width, num_rows × height).
- per-bin: cell count
- max bin count / avg bin count = peak/avg ratio (cluster 정도)
- entropy = `-Σ p_i log(p_i)` (uniformity, 높을수록 균일)

### 5. Big-displacement breakdown
- top-20 displacement cells (Euclidean 기준)
- 각 cell 의: instance name, fanout (#nets connected — `read_iccad2022` 의 net data 활용), lib_cell name, lib_cell area, displacement
- 패턴 검색: macros 위주? lib_cell 별 분포? fanout high vs low?

## Tcl dump 패턴

```tcl
# dump_case3_post_gto.tcl  (post-Planar variant: 같은 + run_planar_correcting 1 line)
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_case3.par
set_mdm_partition_file -file /tmp/flat_case3.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
write_iccad2022_output -out /tmp/diagnose_case3_post_gto.out
exit
```

Variant `post_planar` 는 `write_iccad2022_output` 직전 `run_planar_correcting -iterations 1` 추가. case2/case4 는 case 파일 + Xueyan reference path 만 다름.

## Python analyzer signature

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py \
    --ours /tmp/diagnose_case3_post_gto.out \
    --paper /tmp/ref_gp/case3_gp.txt \
    --case-input /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt \
    --case-name case3 \
    --variant post-GTO
```

`--case-input` 에서 `awk '/Rows/ {print $5; exit}'` 으로 row_height 추출. `--case-input` 의 nets 정보로 fanout 계산.

stdout: markdown chunk (전체 report 의 한 section).

## Bash orchestrator

```bash
#!/usr/bin/env bash
# src/mdm/test/diagnose/run_diagnosis.sh
set -e
ROOT=/home/minjae/workspace/etc/openroad/OpenROAD
ARCHIVE=/home/minjae/workspace/etc/openroad/archive/3d_ic
OPENROAD=$ROOT/build/bin/openroad
ANALYZER=$ROOT/src/mdm/test/diagnose/analyze_displacement.py
REPORT=$ROOT/docs/agents/phase4-gto-output-diagnosis.md

# Run all 6 Tcl dumps in parallel (different cases independent)
for case in case2 case3 case4; do
  for variant in post_gto post_planar; do
    $OPENROAD -no_init -no_splash -exit \
        $ROOT/src/mdm/test/diagnose/dump_${case}_${variant}.tcl \
        > /tmp/dump_${case}_${variant}_run.log 2>&1 &
  done
done
wait

# Generate report header
cat > $REPORT <<EOF
# Phase 4 — GTO 출력 Quality 진단

작성일: $(date +%Y-%m-%d).

비교 baseline: paper post-GP (Xueyan reference data — paper-equivalent
partition + GP coord). 측정 대상: 우리 flat init → GTO → [Planar?]
의 cells 좌표 분포.

EOF

# Per-case analyzer calls
for case in case2 case3 case4; do
  case_input=$ARCHIVE/benchmarks/iccad2022/$case.txt
  paper_out=$([ "$case" = "case2" ] && echo "/tmp/case2_ipl.out" \
                                   || echo "/tmp/ref_gp/${case}_gp.txt")
  for variant in post_gto post_planar; do
    python3 $ANALYZER \
        --ours /tmp/diagnose_${case}_${variant}.out \
        --paper $paper_out \
        --case-input $case_input \
        --case-name $case \
        --variant $variant >> $REPORT
  done
done

# Append conclusion section (manual edit after run)
cat >> $REPORT <<'EOF'

## Conclusion (manual)

(to be filled after reviewing the per-case sections — pattern identification
across cases, leverage point recommendation for next cycle)
EOF

echo "Report written to $REPORT"
```

## Output schema

각 analyzer call 의 stdout markdown chunk:

```markdown
## case3 — post-GTO

**Partition disagreement**: 4.2% (1881 cells out of 44764 total)

**Displacement** (same-die cells, 39200 instances):
| metric | value (ICCAD units) |
|---|---|
| P50  | 150  |
| P90  | 8200 |
| P99  | 24500 |
| max  | 67800 |
| mean | 1340 |
| std  | 4200 |

Histogram (Euclidean, log-scale bins):
```
0-1k     ████████████████████████ 24500
1-2k     ████████ 8100
...
```

**Row-alignment**: 87.3% aligned, avg misalign 240 (units)

**Die cluster (TopDie)**: peak/avg = 3.2 (paper: 1.4), entropy 6.8 nats (paper: 7.5 nats)

**Top-5 displacement cells**:
| inst | fanout | lib_cell | area | disp |
|---|---|---|---|---|
| C12345 | 3  | MC2  | 3220 | 67800 |
| ... | ... | ... | ... | ... |

---
```

각 case 마다 `## case{2,3,4} — post-GTO` 와 `## case{2,3,4} — post-Planar` 두 chunk. 총 6 chunks. 마지막 conclusion section 은 manual edit (사용자가 6 chunks 검토 후 패턴 + leverage point 직접 작성, 또는 후속 brainstorming 짧게).

## Out of scope

- 자동화된 conclusion (패턴 인식) — 6 chunks 의 raw 데이터만 제공. conclusion 은 manual.
- plot 생성 (`.png`) — text histogram 으로 충분. matplotlib 있어도 사용 안 함 (의존성 단순화).
- TerminalLegalizer 전후 비교 — TerminalLegalizer 는 cells 가 아니라 cross-die net terminals 만 변경. 본 진단의 범위 (cells 좌표 분포) 외.
- C++ 측 분석 함수 — 진단 단계엔 과한 투자, 후속 cycle 에서 정식 leverage point 결정 후 별도 plan.

## Success criteria

1. 6 Tcl dumps 가 모두 exit 0 으로 완료 (build 손상 없이 dump 생성)
2. Python analyzer 가 6번 호출 모두 정상 markdown chunk 출력 (parsing 오류 없음)
3. `docs/agents/phase4-gto-output-diagnosis.md` 에 6 chunks 합본 + conclusion section 자리 마련
4. 합본 의 데이터로 다음 cycle leverage point 명확히 식별 가능 (예: "case3 의 row-alignment 87% 인데 paper 는 99% — Multi-Tier Placement 가 paper 에서 row-snap 역할" 같은 패턴)
5. 진단 자체는 1 commit (signed-off, DCO)

## Self-review

**Placeholder scan**: spec 안에 TBD/TODO 없음. row_height 추출 mechanism (`awk '/Rows/ {print $5; exit}'`) 명시. case3 row_height = 115 (확인됨, case2/case4 도 같은 method 로 추출).

**Internal consistency**: architecture (3-stage) ↔ file structure (8 files) ↔ orchestrator script 의 6 dumps + 6 analyzer 호출 일관. metric 5개 명시한 것이 analyzer signature 와 output schema 에 모두 반영.

**Scope check**: 단일 cycle (~1일) 작업. 진단만, 알고리즘 변경 없음. 다음 cycle 의 input 으로만 사용. scope 적정.

**Ambiguity check**:
- "row_height" 단위 명시 (ICCAD case file 의 5번째 column, post `write_iccad2022_output` 의 .out 좌표와 동일 unit). dbu 변환 필요 없음.
- "fanout" 정의 명시 (cell 이 연결된 net 수, ICCAD case file 의 Net 정의 parsing).
- "lib_cell area" = lib_cell 의 width × height (LibCell 정의 line 의 2번째/3번째 column).

수정 필요 부분 없음.
