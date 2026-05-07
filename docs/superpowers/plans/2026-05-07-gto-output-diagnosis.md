# GTO 출력 Quality 진단 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 우리 GTO (그리고 GTO+Planar) 출력 cells 좌표 분포가 paper post-GP 와 어떻게 다른지 instance-level 정량화하는 pipeline 구축. 6 dumps + Python analyzer + bash orchestrator → markdown report (`docs/agents/phase4-gto-output-diagnosis.md`).

**Architecture:** 3-stage pipeline. (1) Tcl dumps run our flow (flat init → GTO → optional Planar) and `write_iccad2022_output` for cells coords. (2) Python analyzer parses both ours/paper `.out`, matches instances by name, computes 5 metrics (partition disagreement, Euclidean displacement, row-alignment, die cluster, big-disp top-20 breakdown). (3) Bash orchestrator runs 6 dumps in parallel, calls analyzer 6 times, concatenates markdown chunks into final report.

**Tech Stack:** OpenROAD MDM Tcl, Python 3 + numpy, bash. Tests are light-weight `--self-test` mode of the analyzer (no separate test framework, fixtures inline).

**Spec:** `docs/superpowers/specs/2026-05-07-gto-output-diagnosis-design.md`

---

## File Structure

신규 8 파일:
- `src/mdm/test/diagnose/dump_case2_post_gto.tcl` — case2 flat→GTO dump
- `src/mdm/test/diagnose/dump_case2_post_planar.tcl` — case2 flat→GTO→Planar dump
- `src/mdm/test/diagnose/dump_case3_post_gto.tcl`
- `src/mdm/test/diagnose/dump_case3_post_planar.tcl`
- `src/mdm/test/diagnose/dump_case4_post_gto.tcl`
- `src/mdm/test/diagnose/dump_case4_post_planar.tcl`
- `src/mdm/test/diagnose/analyze_displacement.py` — 5 metric 분석 + `--self-test` mode + CLI markdown 출력
- `src/mdm/test/diagnose/run_diagnosis.sh` — 6 dumps + 6 analyzer 호출 + report aggregation

생성될 결과 파일:
- `/tmp/diagnose_caseN_post_X.out` × 6 (gitignore)
- `docs/agents/phase4-gto-output-diagnosis.md` — 6 markdown chunks 합본 + manual conclusion

---

## Task 1: 디렉토리 구조 + case3 post-GTO dump (proof of concept)

**Files:**
- Create: `src/mdm/test/diagnose/dump_case3_post_gto.tcl`

- [ ] **Step 1: 디렉토리 생성 및 case3 post-GTO dump 스크립트 작성**

```bash
mkdir -p src/mdm/test/diagnose
```

`src/mdm/test/diagnose/dump_case3_post_gto.tcl`:
```tcl
# case3 post-GTO dump for diagnosis. flat init + paper-equivalent partition
# is loaded as starting state, our GTO redoes partitioning + does cell
# migration. The .out captures our post-GTO cells coords for comparison
# against paper post-GP.
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

- [ ] **Step 2: 실행 + .out 파일 검증**

Run:
```bash
/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad -no_init -no_splash -exit \
    src/mdm/test/diagnose/dump_case3_post_gto.tcl > /tmp/dump_case3_post_gto_run.log 2>&1
echo "exit=$?"
head -3 /tmp/diagnose_case3_post_gto.out
wc -l /tmp/diagnose_case3_post_gto.out
```
Expected: `exit=0`, 첫 줄 `TopDiePlacement <num>`, 약 44k+ lines (case3 has ~44764 cells).

- [ ] **Step 3: Commit**

```bash
git add src/mdm/test/diagnose/dump_case3_post_gto.tcl
git -c commit.gpgsign=false commit -s -m "diag(mdm): case3 post-GTO dump tcl for diagnosis"
```

---

## Task 2: 나머지 5 Tcl dumps

**Files:**
- Create: `src/mdm/test/diagnose/dump_case3_post_planar.tcl`
- Create: `src/mdm/test/diagnose/dump_case2_post_gto.tcl`
- Create: `src/mdm/test/diagnose/dump_case2_post_planar.tcl`
- Create: `src/mdm/test/diagnose/dump_case4_post_gto.tcl`
- Create: `src/mdm/test/diagnose/dump_case4_post_planar.tcl`

- [ ] **Step 1: case3 post-Planar dump (Task 1 의 + Planar 1 line)**

`src/mdm/test/diagnose/dump_case3_post_planar.tcl`:
```tcl
# case3 post-GTO+Planar dump. Same as post-GTO with one extra Planar
# Correcting iteration appended (default knobs).
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_case3.par
set_mdm_partition_file -file /tmp/flat_case3.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
run_planar_correcting -iterations 1
write_iccad2022_output -out /tmp/diagnose_case3_post_planar.out
exit
```

- [ ] **Step 2: case2 post-GTO dump**

`src/mdm/test/diagnose/dump_case2_post_gto.tcl`:
```tcl
# case2 post-GTO dump. Same pattern as case3 with case2 paths.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
write_iccad2022_output -out /tmp/diagnose_case2_post_gto.out
exit
```

- [ ] **Step 3: case2 post-Planar dump**

`src/mdm/test/diagnose/dump_case2_post_planar.tcl`:
```tcl
# case2 post-GTO+Planar dump.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
run_planar_correcting -iterations 1
write_iccad2022_output -out /tmp/diagnose_case2_post_planar.out
exit
```

- [ ] **Step 4: case4 post-GTO dump**

`src/mdm/test/diagnose/dump_case4_post_gto.tcl`:
```tcl
# case4 post-GTO dump.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case4_gp.txt.par > /tmp/flat_case4.par
set_mdm_partition_file -file /tmp/flat_case4.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
write_iccad2022_output -out /tmp/diagnose_case4_post_gto.out
exit
```

- [ ] **Step 5: case4 post-Planar dump**

`src/mdm/test/diagnose/dump_case4_post_planar.tcl`:
```tcl
# case4 post-GTO+Planar dump.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case4_gp.txt.par > /tmp/flat_case4.par
set_mdm_partition_file -file /tmp/flat_case4.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
run_planar_correcting -iterations 1
write_iccad2022_output -out /tmp/diagnose_case4_post_planar.out
exit
```

- [ ] **Step 6: 5 dumps 모두 병렬 실행 + .out 파일 검증**

```bash
OPENROAD=/home/minjae/workspace/etc/openroad/OpenROAD/build/bin/openroad
ROOT=/home/minjae/workspace/etc/openroad/OpenROAD
for variant in dump_case3_post_planar dump_case2_post_gto dump_case2_post_planar dump_case4_post_gto dump_case4_post_planar; do
    $OPENROAD -no_init -no_splash -exit $ROOT/src/mdm/test/diagnose/${variant}.tcl > /tmp/${variant}_run.log 2>&1 &
done
wait
ls -la /tmp/diagnose_*.out
```
Expected: 6 `.out` files. case2 dumps 빠름 (~30s 각), case3 ~1분, case4 ~3분 (case2 +Planar 추가 5분, case3/4 +Planar 추가 5-10분).

- [ ] **Step 7: Commit**

```bash
git add src/mdm/test/diagnose/dump_case3_post_planar.tcl
git add src/mdm/test/diagnose/dump_case2_post_gto.tcl
git add src/mdm/test/diagnose/dump_case2_post_planar.tcl
git add src/mdm/test/diagnose/dump_case4_post_gto.tcl
git add src/mdm/test/diagnose/dump_case4_post_planar.tcl
git -c commit.gpgsign=false commit -s -m "diag(mdm): remaining 5 dump tcls (case2/3/4 × {post-GTO, post-Planar})"
```

---

## Task 3: Python analyzer skeleton + ICCAD .out parser (TDD)

**Files:**
- Create: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: skeleton + parser test 작성 (`--self-test` mode)**

`src/mdm/test/diagnose/analyze_displacement.py`:
```python
#!/usr/bin/env python3
"""Diagnose our GTO output coord distribution against paper post-GP."""

import argparse
import sys
from collections import defaultdict


SAMPLE_OUT = """\
TopDiePlacement 2
Inst C1 100 200
Inst C2 150 230
BottomDiePlacement 1
Inst C3 50 75
Terminal T1 1234 5678
"""


def parse_iccad_out(text):
    """Parse ICCAD .out format. Returns {'top': {name: (x, y)}, 'bottom': {name: (x, y)}}.
    Terminal lines are ignored — only cell instances matter for displacement."""
    result = {'top': {}, 'bottom': {}}
    section = None
    for line in text.splitlines():
        parts = line.strip().split()
        if not parts:
            continue
        if parts[0] == 'TopDiePlacement':
            section = 'top'
        elif parts[0] == 'BottomDiePlacement':
            section = 'bottom'
        elif parts[0] == 'Terminal':
            section = None  # ignore terminals
        elif parts[0] == 'Inst' and section is not None:
            name = parts[1]
            x = int(parts[2])
            y = int(parts[3])
            result[section][name] = (x, y)
    return result


def self_test_parser():
    parsed = parse_iccad_out(SAMPLE_OUT)
    assert parsed['top'] == {'C1': (100, 200), 'C2': (150, 230)}, f"top wrong: {parsed['top']}"
    assert parsed['bottom'] == {'C3': (50, 75)}, f"bottom wrong: {parsed['bottom']}"
    print("PASS parser")


def self_test():
    self_test_parser()
    print("All self-tests PASSED")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    print("(main not implemented yet)")


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: --self-test 실행, 통과 확인 (parser 만 테스트)**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected:
```
PASS parser
All self-tests PASSED
```

(이 task 의 TDD red 단계는 다음 metric task 들이 담당. 이 task 는 parser 의 GREEN 단계.)

- [ ] **Step 3: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): analyzer skeleton + ICCAD .out parser + --self-test mode"
```

---

## Task 4: Partition disagreement metric (TDD)

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: self-test 추가 (RED)**

`analyze_displacement.py` 의 `SAMPLE_OUT` 아래에 추가:
```python
PAPER_SAMPLE_OUT = """\
TopDiePlacement 2
Inst C1 100 200
Inst C3 75 100
BottomDiePlacement 1
Inst C2 200 300
"""


def self_test_partition_disagreement():
    ours = parse_iccad_out(SAMPLE_OUT)
    paper = parse_iccad_out(PAPER_SAMPLE_OUT)
    pct, count, total = partition_disagreement(ours, paper)
    # ours: top={C1,C2}, bottom={C3}
    # paper: top={C1,C3}, bottom={C2}
    # disagree: C2 (we say top, paper says bottom), C3 (we say bottom, paper says top) = 2
    # total cells in union = 3 (C1, C2, C3)
    # pct = 2/3 = 66.67
    assert count == 2, f"count wrong: {count}"
    assert total == 3, f"total wrong: {total}"
    assert abs(pct - 66.666666) < 0.01, f"pct wrong: {pct}"
    print("PASS partition_disagreement")
```

`self_test()` 안에 호출 추가:
```python
def self_test():
    self_test_parser()
    self_test_partition_disagreement()
    print("All self-tests PASSED")
```

- [ ] **Step 2: --self-test 실행, fail 확인**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected: `NameError: name 'partition_disagreement' is not defined`

- [ ] **Step 3: `partition_disagreement` 함수 구현**

`SAMPLE_OUT` 위 또는 `parse_iccad_out` 아래에 추가:
```python
def partition_disagreement(ours, paper):
    """Returns (percentage, disagreeing_count, total_cells_in_union)."""
    ours_top = set(ours['top'].keys())
    ours_bottom = set(ours['bottom'].keys())
    paper_top = set(paper['top'].keys())
    paper_bottom = set(paper['bottom'].keys())
    # disagree: cell on different dies in ours vs paper
    disagree = (ours_top & paper_bottom) | (ours_bottom & paper_top)
    total = (ours_top | ours_bottom) | (paper_top | paper_bottom)
    if not total:
        return 0.0, 0, 0
    return 100.0 * len(disagree) / len(total), len(disagree), len(total)
```

- [ ] **Step 4: --self-test 실행, PASS 확인**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected: `PASS partition_disagreement` `All self-tests PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): metric — partition disagreement"
```

---

## Task 5: Displacement metric (TDD)

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: self-test 추가 (RED)**

```python
def self_test_displacement():
    ours = parse_iccad_out(SAMPLE_OUT)
    paper = parse_iccad_out(PAPER_SAMPLE_OUT)
    # ours top: C1=(100,200), C2=(150,230) ; paper top: C1=(100,200), C3=(75,100)
    # ours bottom: C3=(50,75) ; paper bottom: C2=(200,300)
    # same-die intersection: top {C1}, bottom {} (C3 in ours bottom, paper top, etc)
    # C1 displacement = 0 (same coord)
    # so all displacements = [0]
    stats, hist = displacement(ours, paper)
    assert stats['count'] == 1, f"count wrong: {stats}"
    assert stats['p50'] == 0, f"p50 wrong: {stats}"
    assert stats['max'] == 0, f"max wrong: {stats}"
    print("PASS displacement (trivial)")

    # construct a richer example
    ours2 = {'top': {'A': (0, 0), 'B': (10, 10), 'C': (100, 100)},
             'bottom': {}}
    paper2 = {'top': {'A': (3, 4), 'B': (10, 10), 'C': (200, 200)},
              'bottom': {}}
    # A: sqrt(9+16)=5, B: 0, C: sqrt(10000+10000)=141.42
    stats2, _ = displacement(ours2, paper2)
    assert stats2['count'] == 3, f"rich count: {stats2}"
    assert abs(stats2['max'] - 141.42135) < 0.01, f"max wrong: {stats2}"
    print("PASS displacement (rich)")
```

`self_test()` 에 추가:
```python
self_test_displacement()
```

- [ ] **Step 2: --self-test 실행, fail 확인**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected: `NameError: name 'displacement' is not defined`

- [ ] **Step 3: `displacement` 함수 구현 (numpy 사용)**

`analyze_displacement.py` 상단 import 에 numpy 추가:
```python
import numpy as np
```

함수 추가:
```python
def displacement(ours, paper):
    """Per-cell Euclidean displacement of same-die cells.
    Returns (stats_dict, histogram_pairs).
    stats_dict: {count, p50, p90, p99, max, mean, std} (units: ICCAD units)
    histogram_pairs: list of (label, bin_count) for ASCII rendering."""
    disps = []
    for die in ('top', 'bottom'):
        common = set(ours[die].keys()) & set(paper[die].keys())
        for name in common:
            ox, oy = ours[die][name]
            px, py = paper[die][name]
            disps.append(((ox - px) ** 2 + (oy - py) ** 2) ** 0.5)
    arr = np.array(disps) if disps else np.array([0.0])
    stats = {
        'count': len(disps),
        'p50': float(np.percentile(arr, 50)),
        'p90': float(np.percentile(arr, 90)),
        'p99': float(np.percentile(arr, 99)),
        'max': float(arr.max()),
        'mean': float(arr.mean()),
        'std': float(arr.std()),
    }
    # Log-scale bins: 0-1k, 1-2k, 2-5k, 5-10k, 10-20k, 20-50k, 50-100k, 100k+
    edges = [0, 1000, 2000, 5000, 10000, 20000, 50000, 100000, float('inf')]
    labels = ['0-1k', '1-2k', '2-5k', '5-10k', '10-20k', '20-50k', '50-100k', '100k+']
    hist = []
    for i, label in enumerate(labels):
        lo, hi = edges[i], edges[i + 1]
        n = int(((arr >= lo) & (arr < hi)).sum())
        hist.append((label, n))
    return stats, hist
```

- [ ] **Step 4: --self-test 실행, PASS 확인**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected: `PASS displacement (trivial)` `PASS displacement (rich)` `All self-tests PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): metric — Euclidean displacement + histogram"
```

---

## Task 6: Row-alignment metric (TDD)

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: self-test 추가 (RED)**

```python
def self_test_row_alignment():
    # row_height=10, cells y={0, 5, 10, 17, 20}
    # aligned (within 1 of 0 mod 10): y=0 (0), y=10 (0), y=20 (0). 3/5 = 60%
    # avg misalign = (0+5+0+7+0)/5 = 2.4
    placement = {'top': {'A': (0, 0), 'B': (0, 5), 'C': (0, 10), 'D': (0, 17), 'E': (0, 20)},
                 'bottom': {}}
    pct, avg = row_alignment(placement, row_height=10, tolerance=1)
    assert pct == 60.0, f"pct wrong: {pct}"
    assert abs(avg - 2.4) < 0.01, f"avg wrong: {avg}"
    print("PASS row_alignment")
```

`self_test()` 에 추가.

- [ ] **Step 2: --self-test 실행, fail 확인**

Expected: `NameError: name 'row_alignment' is not defined`

- [ ] **Step 3: `row_alignment` 함수 구현**

```python
def row_alignment(placement, row_height, tolerance=1):
    """Cells whose y is within `tolerance` of a multiple of `row_height` are
    counted as aligned. Returns (aligned_pct, avg_misalign)."""
    total = 0
    aligned = 0
    misalign_sum = 0
    for die in ('top', 'bottom'):
        for _, (_, y) in placement[die].items():
            total += 1
            misalign = y % row_height
            # misalign close to 0 OR close to row_height (other side of mod)
            d = min(misalign, row_height - misalign)
            if d <= tolerance:
                aligned += 1
            misalign_sum += d
    if total == 0:
        return 0.0, 0.0
    return 100.0 * aligned / total, misalign_sum / total
```

- [ ] **Step 4: --self-test 실행, PASS 확인**

Expected: `PASS row_alignment` `All self-tests PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): metric — row-alignment (mod row_height)"
```

---

## Task 7: Die cluster metric (TDD)

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: self-test 추가 (RED)**

```python
def self_test_die_cluster():
    # 50x50 bins on a 100x100 die. cells: 2 in bin (0,0), 1 in bin (49,49)
    # bin grid: 2x2 = 4 cells per bin (effective bin size 2x2 for die_w=100, bins=50)
    # bin (0,0): cells with x in [0,2) y in [0,2) — A=(0,0), B=(1,1) → count 2
    # bin (49,49): cells with x in [98,100) y in [98,100) — C=(99,99) → count 1
    # nonzero bins: 2. total cells = 3.
    # if all 50x50=2500 bins, mean = 3/2500 = 0.0012, max = 2. ratio = 1666...
    # For our purpose, only consider non-empty distribution OR include all bins.
    # We will include all bins (more uniform = lower peak/avg ratio).
    cells = {'A': (0, 0), 'B': (1, 1), 'C': (99, 99)}
    peak_avg, entropy = die_cluster(cells, die_width=100, die_height=100, num_bins=50)
    # peak=2, avg=3/(50*50)=0.0012, ratio=2/0.0012=1666.66
    assert abs(peak_avg - 1666.6666) < 1.0, f"peak/avg wrong: {peak_avg}"
    # entropy: 2 nonzero bins. p(bin1)=2/3, p(bin2)=1/3. H=-(2/3 log 2/3 + 1/3 log 1/3) = 0.6365 nats
    assert abs(entropy - 0.6365) < 0.01, f"entropy wrong: {entropy}"
    print("PASS die_cluster")
```

`self_test()` 에 추가.

- [ ] **Step 2: --self-test 실행, fail 확인**

Expected: `NameError: name 'die_cluster' is not defined`

- [ ] **Step 3: `die_cluster` 함수 구현**

```python
def die_cluster(cells, die_width, die_height, num_bins=50):
    """Returns (peak/avg ratio, entropy in nats).
    cells: dict {name: (x, y)}.
    Bin grid is num_bins x num_bins covering [0, die_width] x [0, die_height]."""
    if not cells:
        return 0.0, 0.0
    bin_w = die_width / num_bins
    bin_h = die_height / num_bins
    counts = np.zeros((num_bins, num_bins), dtype=np.int64)
    for (x, y) in cells.values():
        i = min(int(x / bin_w), num_bins - 1)
        j = min(int(y / bin_h), num_bins - 1)
        counts[i, j] += 1
    peak = float(counts.max())
    avg = float(counts.mean())
    ratio = peak / avg if avg > 0 else 0.0
    # Entropy from p_i = count_i / total over non-empty bins
    total = counts.sum()
    if total == 0:
        return ratio, 0.0
    p = counts.flatten() / total
    p_nonzero = p[p > 0]
    entropy = float(-(p_nonzero * np.log(p_nonzero)).sum())
    return ratio, entropy
```

- [ ] **Step 4: --self-test 실행, PASS 확인**

Expected: `PASS die_cluster` `All self-tests PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): metric — die cluster (peak/avg + entropy)"
```

---

## Task 8: Big-displacement breakdown metric (TDD)

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

이 metric 은 cells 의 fanout (#nets connected) + lib_cell name + area 까지 필요. 두 input 더 받음: ICCAD case file (Net 정의 + LibCell area).

- [ ] **Step 1: self-test 추가 (RED) — fanout/lib_cell parser + breakdown**

```python
SAMPLE_CASE_INPUT = """\
NumTechnologies 1
Tech TA 3
LibCell MC1 10 20 2
Pin P1 1 2
Pin P2 3 4
LibCell MC2 5 5 1
Pin P1 0 0
LibCell MC3 100 100 1
Pin P1 50 50
NumInsts 3
Inst C1 MC1
Inst C2 MC2
Inst C3 MC3
NumNets 2
Net N1 2
Pin C1 P1
Pin C2 P1
Net N2 3
Pin C1 P2
Pin C2 P1
Pin C3 P1
"""


def self_test_case_input_parser():
    info = parse_case_input(SAMPLE_CASE_INPUT)
    # libs: MC1 (10*20=200), MC2 (5*5=25), MC3 (100*100=10000)
    assert info['lib_area']['MC1'] == 200
    assert info['lib_area']['MC2'] == 25
    # inst→lib: C1→MC1, C2→MC2, C3→MC3
    assert info['inst_lib']['C1'] == 'MC1'
    # fanout: C1 in N1+N2=2, C2 in N1+N2=2, C3 in N2=1
    assert info['fanout']['C1'] == 2
    assert info['fanout']['C3'] == 1
    print("PASS case_input_parser")


def self_test_big_displacement():
    case_info = parse_case_input(SAMPLE_CASE_INPUT)
    ours = {'top': {'C1': (0, 0), 'C2': (0, 0), 'C3': (0, 0)}, 'bottom': {}}
    paper = {'top': {'C1': (10, 0), 'C2': (5, 0), 'C3': (1000, 0)}, 'bottom': {}}
    # C1 disp=10, C2 disp=5, C3 disp=1000
    # top-2 by disp: C3 (1000), C1 (10)
    top = big_displacement(ours, paper, case_info, k=2)
    assert len(top) == 2
    assert top[0]['inst'] == 'C3'
    assert top[0]['disp'] == 1000.0
    assert top[0]['lib_cell'] == 'MC3'
    assert top[0]['area'] == 10000
    assert top[0]['fanout'] == 1
    assert top[1]['inst'] == 'C1'
    print("PASS big_displacement")
```

`self_test()` 에 둘 다 추가.

- [ ] **Step 2: --self-test 실행, fail 확인**

Expected: `NameError: name 'parse_case_input' is not defined`

- [ ] **Step 3: `parse_case_input` + `big_displacement` 구현**

```python
def parse_case_input(text):
    """Parse ICCAD case file. Returns {lib_area: {lib_name: area},
    inst_lib: {inst_name: lib_name}, fanout: {inst_name: int}}."""
    lib_area = {}
    inst_lib = {}
    fanout = defaultdict(int)
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        parts = lines[i].strip().split()
        if not parts:
            i += 1
            continue
        if parts[0] == 'LibCell':
            # LibCell <name> <width> <height> <num_pins>
            name, w, h = parts[1], int(parts[2]), int(parts[3])
            lib_area[name] = w * h
            i += 1  # skip pin lines (we don't need them here)
            continue
        if parts[0] == 'Inst' and len(parts) >= 3 and parts[2].startswith('MC'):
            # "Inst C1 MC1" — instance definition
            inst_lib[parts[1]] = parts[2]
            i += 1
            continue
        if parts[0] == 'Net':
            # Net N1 <pin_count>
            pin_count = int(parts[2])
            i += 1
            for _ in range(pin_count):
                if i >= len(lines):
                    break
                pp = lines[i].strip().split()
                if len(pp) >= 3 and pp[0] == 'Pin':
                    fanout[pp[1]] += 1
                i += 1
            continue
        i += 1
    return {'lib_area': lib_area, 'inst_lib': inst_lib, 'fanout': dict(fanout)}


def big_displacement(ours, paper, case_info, k=20):
    """Top-K by Euclidean displacement (same-die cells only).
    Returns list of dicts: [{inst, disp, lib_cell, area, fanout}, ...]."""
    items = []
    for die in ('top', 'bottom'):
        common = set(ours[die].keys()) & set(paper[die].keys())
        for name in common:
            ox, oy = ours[die][name]
            px, py = paper[die][name]
            d = ((ox - px) ** 2 + (oy - py) ** 2) ** 0.5
            lib = case_info['inst_lib'].get(name, '?')
            items.append({
                'inst': name,
                'disp': d,
                'lib_cell': lib,
                'area': case_info['lib_area'].get(lib, 0),
                'fanout': case_info['fanout'].get(name, 0),
            })
    items.sort(key=lambda x: x['disp'], reverse=True)
    return items[:k]
```

- [ ] **Step 4: --self-test 실행, PASS 확인**

Expected: `PASS case_input_parser` `PASS big_displacement` `All self-tests PASSED`

- [ ] **Step 5: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): metric — top-K big displacement breakdown + ICCAD case parser"
```

---

## Task 9: argparse + main + markdown output

**Files:**
- Modify: `src/mdm/test/diagnose/analyze_displacement.py`

- [ ] **Step 1: argparse 확장 + markdown 출력 함수 구현**

`main()` 을 다음으로 교체 (이미 있는 `--self-test` 분기는 유지):
```python
def render_markdown(case_name, variant, ours_path, paper_path, case_input_path):
    with open(ours_path) as f:
        ours = parse_iccad_out(f.read())
    with open(paper_path) as f:
        paper = parse_iccad_out(f.read())
    with open(case_input_path) as f:
        case_info = parse_case_input(f.read())

    # row_height: 5번째 column of TopDieRows
    row_height = None
    die_width = die_height = None
    with open(case_input_path) as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 6 and parts[0] == 'TopDieRows':
                # TopDieRows <x> <y> <width> <height> <num_rows>
                row_height = int(parts[4])
                die_width = int(parts[3])
                die_height = row_height * int(parts[5])
                break
    if row_height is None:
        raise RuntimeError(f"row_height not found in {case_input_path}")

    out = []
    out.append(f"\n## {case_name} — {variant}\n")

    pct, count, total = partition_disagreement(ours, paper)
    out.append(f"**Partition disagreement**: {pct:.2f}% ({count} cells out of {total} total)\n")

    stats, hist = displacement(ours, paper)
    out.append("**Displacement** (same-die cells, " + str(stats['count']) + " instances):\n")
    out.append("| metric | value (ICCAD units) |")
    out.append("|---|---|")
    out.append(f"| P50  | {stats['p50']:.0f} |")
    out.append(f"| P90  | {stats['p90']:.0f} |")
    out.append(f"| P99  | {stats['p99']:.0f} |")
    out.append(f"| max  | {stats['max']:.0f} |")
    out.append(f"| mean | {stats['mean']:.0f} |")
    out.append(f"| std  | {stats['std']:.0f} |")
    out.append("")
    out.append("Histogram (Euclidean):\n```")
    if hist:
        max_n = max(n for _, n in hist) or 1
        for label, n in hist:
            bar = '█' * int(40 * n / max_n)
            out.append(f"{label:>10s} {bar} {n}")
    out.append("```\n")

    pct_align, avg_mis = row_alignment(ours, row_height)
    out.append(f"**Row-alignment** (ours): {pct_align:.1f}% aligned, avg misalign {avg_mis:.1f}")
    pct_align_paper, avg_mis_paper = row_alignment(paper, row_height)
    out.append(f"**Row-alignment** (paper): {pct_align_paper:.1f}% aligned, avg misalign {avg_mis_paper:.1f}\n")

    for die_label, die_key in (('TopDie', 'top'), ('BottomDie', 'bottom')):
        ratio_ours, ent_ours = die_cluster(ours[die_key], die_width, die_height)
        ratio_paper, ent_paper = die_cluster(paper[die_key], die_width, die_height)
        out.append(f"**Die cluster** ({die_label}, ours): peak/avg = {ratio_ours:.2f}, entropy {ent_ours:.2f} nats")
        out.append(f"**Die cluster** ({die_label}, paper): peak/avg = {ratio_paper:.2f}, entropy {ent_paper:.2f} nats")
    out.append("")

    top_k = big_displacement(ours, paper, case_info, k=20)
    out.append("**Top-20 displacement cells**:\n")
    out.append("| inst | fanout | lib_cell | area | disp |")
    out.append("|---|---|---|---|---|")
    for item in top_k:
        out.append(f"| {item['inst']} | {item['fanout']} | {item['lib_cell']} | {item['area']} | {item['disp']:.0f} |")
    out.append("\n---\n")
    return '\n'.join(out)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--ours')
    parser.add_argument('--paper')
    parser.add_argument('--case-input')
    parser.add_argument('--case-name')
    parser.add_argument('--variant')
    args = parser.parse_args()
    if args.self_test:
        self_test()
        return
    if not all([args.ours, args.paper, args.case_input, args.case_name, args.variant]):
        sys.stderr.write("Need --ours, --paper, --case-input, --case-name, --variant (or --self-test)\n")
        sys.exit(2)
    print(render_markdown(args.case_name, args.variant, args.ours, args.paper, args.case_input))


if __name__ == '__main__':
    main()
```

- [ ] **Step 2: --self-test 실행, 모든 metrics PASS 확인**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py --self-test
```
Expected: `PASS parser` `PASS partition_disagreement` `PASS displacement (trivial)` `PASS displacement (rich)` `PASS row_alignment` `PASS die_cluster` `PASS case_input_parser` `PASS big_displacement` `All self-tests PASSED`

- [ ] **Step 3: case3 post-GTO 실제 데이터로 실행 (sanity)**

```bash
python3 src/mdm/test/diagnose/analyze_displacement.py \
    --ours /tmp/diagnose_case3_post_gto.out \
    --paper /tmp/ref_gp/case3_gp.txt \
    --case-input /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt \
    --case-name case3 \
    --variant post-GTO | head -40
```
Expected: markdown chunk with `## case3 — post-GTO` header, partition disagreement %, displacement table + histogram, row alignment line, die cluster lines, top-20 table. 숫자 sanity 확인 (예: P50 > 0, top-1 disp > 0, etc).

- [ ] **Step 4: Commit**

```bash
git add src/mdm/test/diagnose/analyze_displacement.py
git -c commit.gpgsign=false commit -s -m "diag(mdm): analyzer CLI — argparse + markdown output rendering"
```

---

## Task 10: Bash orchestrator

**Files:**
- Create: `src/mdm/test/diagnose/run_diagnosis.sh`

- [ ] **Step 1: orchestrator 작성**

`src/mdm/test/diagnose/run_diagnosis.sh`:
```bash
#!/usr/bin/env bash
# GTO output diagnosis orchestrator. Runs 6 dumps (case2/3/4 × {post-GTO,
# post-Planar}) in parallel, calls analyzer for each, aggregates markdown
# into docs/agents/phase4-gto-output-diagnosis.md.
set -e

ROOT=/home/minjae/workspace/etc/openroad/OpenROAD
ARCHIVE=/home/minjae/workspace/etc/openroad/archive/3d_ic
OPENROAD=$ROOT/build/bin/openroad
ANALYZER=$ROOT/src/mdm/test/diagnose/analyze_displacement.py
REPORT=$ROOT/docs/agents/phase4-gto-output-diagnosis.md

paper_for() {
    case "$1" in
        case2) echo "/tmp/case2_ipl.out" ;;
        case3) echo "/tmp/ref_gp/case3_gp.txt" ;;
        case4) echo "/tmp/ref_gp/case4_gp.txt" ;;
    esac
}

echo "[1/3] Running 6 dumps in parallel..."
for case in case2 case3 case4; do
    for variant in post_gto post_planar; do
        $OPENROAD -no_init -no_splash -exit \
            $ROOT/src/mdm/test/diagnose/dump_${case}_${variant}.tcl \
            > /tmp/dump_${case}_${variant}_run.log 2>&1 &
    done
done
wait

# Verify all 6 .out files exist
for case in case2 case3 case4; do
    for variant in post_gto post_planar; do
        if [ ! -f /tmp/diagnose_${case}_${variant}.out ]; then
            echo "FAIL: /tmp/diagnose_${case}_${variant}.out not generated"
            tail -30 /tmp/dump_${case}_${variant}_run.log
            exit 1
        fi
    done
done

echo "[2/3] Generating report header..."
cat > $REPORT <<EOF
# Phase 4 — GTO 출력 Quality 진단

작성일: $(date +%Y-%m-%d).

비교 baseline: paper post-GP (Xueyan reference data — paper-equivalent
partition + GP coord). 측정 대상: 우리 flat init → GTO → [Planar?]
의 cells 좌표 분포 (5 metrics × 3 cases × 2 variants = 30 datapoints).

EOF

echo "[3/3] Calling analyzer for each (case, variant)..."
for case in case2 case3 case4; do
    case_input=$ARCHIVE/benchmarks/iccad2022/${case}.txt
    paper_path=$(paper_for $case)
    for variant_path in post_gto post_planar; do
        variant_label=$(echo $variant_path | tr '_' '-')
        python3 $ANALYZER \
            --ours /tmp/diagnose_${case}_${variant_path}.out \
            --paper $paper_path \
            --case-input $case_input \
            --case-name $case \
            --variant $variant_label >> $REPORT
    done
done

cat >> $REPORT <<'EOF'

## Conclusion (manual)

(다음 cycle 의 leverage point 결정에 사용. 6 metric chunks 검토 후 패턴 식별:
어떤 metric 이 가장 큰 격차? GTO → +Planar 가 격차를 줄이는가? case 별 패턴
일관성? 결론 내려서 이 section 채우기.)
EOF

echo "Done. Report: $REPORT"
```

`chmod +x src/mdm/test/diagnose/run_diagnosis.sh`

- [ ] **Step 2: orchestrator 실행 (전체 pipeline 검증)**

```bash
bash src/mdm/test/diagnose/run_diagnosis.sh
```
Expected: `[1/3]`, `[2/3]`, `[3/3]` 단계 출력. 마지막 `Done. Report: .../phase4-gto-output-diagnosis.md`. 약 ~10-15분 (case4 +Planar 가 longest, 5-10분).

- [ ] **Step 3: 결과 markdown 검증**

```bash
wc -l docs/agents/phase4-gto-output-diagnosis.md
grep -c "^## case" docs/agents/phase4-gto-output-diagnosis.md
head -5 docs/agents/phase4-gto-output-diagnosis.md
```
Expected: 약 200-400 줄. `^## case` 매칭 6개 (case2/3/4 × 2 variants). header 가 "Phase 4 — GTO 출력 Quality 진단".

- [ ] **Step 4: Commit (orchestrator + 결과 report 함께)**

```bash
git add src/mdm/test/diagnose/run_diagnosis.sh
git add docs/agents/phase4-gto-output-diagnosis.md
git -c commit.gpgsign=false commit -s -m "diag(mdm): orchestrator + initial diagnosis report (3 cases × 2 variants)"
```

---

## Task 11: Conclusion section (manual analysis)

**Files:**
- Modify: `docs/agents/phase4-gto-output-diagnosis.md` (Conclusion section 만)

- [ ] **Step 1: 6 chunks 검토 + 패턴 식별**

다음 질문들에 답을 데이터에서 찾기:
1. partition disagreement 가 작은가 (< 5%)? 우리 GTO 의 partition decision 이 paper 와 얼마나 다른가?
2. displacement P90 가 dense (case3) vs sparse (case2/4) 에 따라 어떻게 달라지는가?
3. row-alignment % 가 우리 vs paper 사이에 큰 차이? paper 가 거의 100% 인데 우리는 N% 면 → row-snap 이 빠진 단계
4. die cluster peak/avg ratio 가 우리에서 더 큰가 (cells cluster 됐다는 신호)? entropy 비교 도?
5. top-20 displacement cells 패턴 — 특정 lib_cell 만 큰가? macros 만? fanout high 만?
6. GTO → +Planar variant 가 격차를 좁히는가, 키우는가?

- [ ] **Step 2: Conclusion section 작성 (`docs/agents/phase4-gto-output-diagnosis.md` 의 끝 section 직접 편집)**

`## Conclusion (manual)` section 을 다음 형식으로 채움:
```markdown
## Conclusion

### 패턴 요약
- (1) Partition 차이: ...
- (2) Displacement: ...
- (3) Row-alignment: ...
- (4) Die cluster: ...
- (5) Big-displacement cells 의 공통점: ...
- (6) GTO vs +Planar 의 trajectory: ...

### 다음 cycle leverage point 추천
- 1순위: ... (이유)
- 2순위: ... (이유)

### handoff.md 의 §4 업데이트 권고
이 결과로 handoff.md `§4 남은 작업` 의 1순위가 다음과 같이 변경되어야:
- 기존: "Phase 4.4 Flattened Init wrapper 신규 구현" (1순위)
- 변경: "<위 conclusion 의 1순위>" 또는 "유지 (Phase 4.4)" + 사유
```

- [ ] **Step 3: 진단 결과로 handoff.md 갱신**

handoff.md 의 `§4 남은 작업` 1순위를 conclusion 의 1순위로 갱신. 다른 priority 도 재배열 필요시 함께.

- [ ] **Step 4: Commit (cycle 종료)**

```bash
git add docs/agents/phase4-gto-output-diagnosis.md handoff.md
git -c commit.gpgsign=false commit -s -m "diag(mdm): conclusion + leverage point recommendation, handoff updated"
```

---

## Self-Review

**Spec coverage:**
- ✅ 6 Tcl dumps — Tasks 1, 2
- ✅ Python analyzer 5 metric — Tasks 3-8
- ✅ argparse + markdown — Task 9
- ✅ Bash orchestrator — Task 10
- ✅ 결과 위치 (`docs/agents/phase4-gto-output-diagnosis.md`) — Tasks 10, 11
- ✅ Out of scope 준수 (no plot, no automated conclusion, no C++ analysis)
- ✅ Success criteria 5개 모두 — Task 10 (1, 2, 3 항목), Task 11 (4, 5 항목)

**Placeholder scan:** 모든 step 에 actual code/command 있음. "TBD" 없음. row_height 추출은 `parts[4]` of `TopDieRows` 명시. fanout 은 NumNets section parse 로 명시. 함수 signature 일관 (`partition_disagreement`, `displacement`, `row_alignment`, `die_cluster`, `parse_case_input`, `big_displacement`, `render_markdown`).

**Type consistency:**
- Task 3 의 `parse_iccad_out` 반환 형식 = `{'top': {name: (x,y)}, 'bottom': {name: (x,y)}}`
- Task 5 의 `displacement` 가 `parse_iccad_out` 의 출력을 입력으로 받음 — 일치
- Task 6 의 `row_alignment` 가 같은 형식 입력, `row_height` int — 일치
- Task 7 의 `die_cluster` 가 die-level dict (`ours[die_key]`) 입력, die_width/die_height int — Task 9 의 main 에서 `ours[die_key]` 추출 후 호출하는 것 일치
- Task 8 의 `parse_case_input` 반환 = `{'lib_area': ..., 'inst_lib': ..., 'fanout': ...}` (Task 4-7 metrics 와 별개), `big_displacement` 가 이 dict 를 사용
- Task 9 `render_markdown` 이 모든 metric 함수 호출 — signatures 일치 확인됨

수정 필요 없음.
