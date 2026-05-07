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

# Check if all 6 .out files already exist; if so, skip the slow dump phase.
# Pass --force to re-run dumps regardless.
all_present=1
for case in case2 case3 case4; do
    for variant in post_gto post_planar; do
        if [ ! -f /tmp/diagnose_${case}_${variant}.out ]; then
            all_present=0
            break 2
        fi
    done
done

if [ "$all_present" = "1" ] && [ "${1:-}" != "--force" ]; then
    echo "[1/3] All 6 dump .out files exist — skipping dump phase (use --force to re-run)."
else
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
fi

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
