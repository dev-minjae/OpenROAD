#!/usr/bin/env bash
# Phase 4 numerical lock: run case2 e2e, evaluate, compare HPWL to
# baseline. Exits non-zero on regression.
set -e

BASELINE_HPWL=2694774
TOLERANCE_PCT=0.5

ROOT=/home/minjae/workspace/etc/openroad/OpenROAD
ARCHIVE=/home/minjae/workspace/etc/openroad/archive/3d_ic
OPENROAD=$ROOT/build/bin/openroad
EVALUATOR=$ARCHIVE/tools/evaluator_0525
CASE_INPUT=$ARCHIVE/benchmarks/iccad2022/case2.txt
TCL=$ROOT/src/mdm/test/regression_phase4_case2.tcl
OUTPUT=/tmp/regression_phase4_case2.out
EVAL_LOG=/tmp/regression_phase4_eval.log
RUN_LOG=/tmp/regression_phase4_run.log

echo "[1/3] Running OpenROAD ($TCL)..."
$OPENROAD -no_init -no_splash -exit $TCL > $RUN_LOG 2>&1 || {
    echo "FAIL: OpenROAD run failed. Last 30 lines of $RUN_LOG:"
    tail -30 $RUN_LOG
    exit 1
}

echo "[2/3] Running evaluator..."
$EVALUATOR $CASE_INPUT $OUTPUT > $EVAL_LOG 2>&1 || {
    echo "FAIL: evaluator failed. Output:"
    cat $EVAL_LOG
    exit 1
}

echo "[3/3] Parsing HPWL..."
# evaluator_0525 prints "Total HPWL for this design is <N>" at the end.
HPWL=$(grep -oP 'Total HPWL[^0-9]*\K[0-9]+' $EVAL_LOG | tail -1)
if [ -z "$HPWL" ]; then
    echo "FAIL: could not parse HPWL from evaluator output. Full log:"
    cat $EVAL_LOG
    exit 1
fi

DIFF_PCT=$(echo "scale=4; ($HPWL - $BASELINE_HPWL) * 100 / $BASELINE_HPWL" | bc)
ABS_DIFF_PCT=${DIFF_PCT#-}

echo "Baseline HPWL: $BASELINE_HPWL"
echo "Current HPWL:  $HPWL"
echo "Diff:          $DIFF_PCT %  (tolerance +/- $TOLERANCE_PCT %)"

if [ "$(echo "$ABS_DIFF_PCT > $TOLERANCE_PCT" | bc -l)" = "1" ]; then
    echo "FAIL: HPWL drift exceeds $TOLERANCE_PCT% tolerance."
    exit 1
fi
echo "PASS: HPWL within tolerance."
