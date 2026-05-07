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


def self_test():
    self_test_parser()
    self_test_partition_disagreement()
    self_test_displacement()
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
