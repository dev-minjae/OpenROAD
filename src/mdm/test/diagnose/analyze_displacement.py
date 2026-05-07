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
