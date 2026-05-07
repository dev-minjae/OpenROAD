#!/usr/bin/env python3
"""Diagnose our GTO output coord distribution against paper post-GP."""

import argparse
import sys
from collections import defaultdict

import numpy as np


SAMPLE_OUT = """\
TopDiePlacement 2
Inst C1 100 200
Inst C2 150 230
BottomDiePlacement 1
Inst C3 50 75
Terminal T1 1234 5678
"""


PAPER_SAMPLE_OUT = """\
TopDiePlacement 2
Inst C1 100 200
Inst C3 75 100
BottomDiePlacement 1
Inst C2 200 300
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


def self_test_parser():
    parsed = parse_iccad_out(SAMPLE_OUT)
    assert parsed['top'] == {'C1': (100, 200), 'C2': (150, 230)}, f"top wrong: {parsed['top']}"
    assert parsed['bottom'] == {'C3': (50, 75)}, f"bottom wrong: {parsed['bottom']}"
    print("PASS parser")


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


def self_test_displacement():
    ours = parse_iccad_out(SAMPLE_OUT)
    paper = parse_iccad_out(PAPER_SAMPLE_OUT)
    # ours top: C1=(100,200), C2=(150,230) ; paper top: C1=(100,200), C3=(75,100)
    # ours bottom: C3=(50,75) ; paper bottom: C2=(200,300)
    # same-die intersection: top {C1}, bottom {} (C3 in ours bottom, paper top, etc)
    # C1 displacement = 0 (same coord)
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


def self_test_row_alignment():
    # row_height=10, cells y={0, 5, 10, 17, 20}
    # aligned (within 1 of 0 mod 10): y=0 (0), y=10 (0), y=20 (0). 3/5 = 60%
    # avg misalign = (0+5+0+3+0)/5 = 1.6 (note: misalign for 17 is min(7, 10-7)=3)
    placement = {'top': {'A': (0, 0), 'B': (0, 5), 'C': (0, 10), 'D': (0, 17), 'E': (0, 20)},
                 'bottom': {}}
    pct, avg = row_alignment(placement, row_height=10, tolerance=1)
    assert pct == 60.0, f"pct wrong: {pct}"
    assert abs(avg - 1.6) < 0.01, f"avg wrong: {avg}"
    print("PASS row_alignment")


def self_test_die_cluster():
    # 50x50 bins on a 100x100 die. cells: 2 in bin (0,0), 1 in bin (49,49)
    cells = {'A': (0, 0), 'B': (1, 1), 'C': (99, 99)}
    peak_avg, entropy = die_cluster(cells, die_width=100, die_height=100, num_bins=50)
    # peak=2, avg=3/(50*50)=0.0012, ratio=2/0.0012=1666.66
    assert abs(peak_avg - 1666.6666) < 1.0, f"peak/avg wrong: {peak_avg}"
    # entropy: 2 nonzero bins. p(bin1)=2/3, p(bin2)=1/3. H=-(2/3 log 2/3 + 1/3 log 1/3) = 0.6365 nats
    assert abs(entropy - 0.6365) < 0.01, f"entropy wrong: {entropy}"
    print("PASS die_cluster")


def self_test():
    self_test_parser()
    self_test_partition_disagreement()
    self_test_displacement()
    self_test_row_alignment()
    self_test_die_cluster()
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
