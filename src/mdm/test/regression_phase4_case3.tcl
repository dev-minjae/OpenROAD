# Phase 4 case3 e2e numerical lock.
# Baseline: HPWL = 52,217,337 (skip Planar Correcting variant, evaluator_0525).
# Tolerance: +/- 0.5%.
#
# Why skip Planar Correcting for case3?
# case3 is dense (TopDie/BottomDie MaxUtil = 78/78). Post-GTO overflow is
# already low (~0.13). Running Planar Correcting Nesterov in this regime:
#   - overflow shoots from 0.13 to 0.39 within 100 iters then stuck
#   - HPWL grows monotonically (26.86M -> 180M+ at default max_iter=5000)
#   - SemiLeg recovers only partially (final 96.0M vs skip's 52.2M)
# Knob tuning (density 0.8/1.0/1.5, weight 0.1/0.5/1.5, max_iter 200/500/2000/5000)
# all gave worse results than skipping. Planar Correcting helps for sparser
# designs (case2 at 70/75 util: 2,694,774 vs skip's 2,735,844) but hurts on
# dense designs.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_case3.par
set_mdm_partition_file -file /tmp/flat_case3.par
set_3D_IC -die_number 2
puts "=== HPWL after flat init ==="
get_3d_hpwl
run_global_tier_optimization -apply
puts "=== HPWL after Algorithm 2 + apply ==="
get_3d_hpwl
puts "=== Skipping Planar Correcting (case3 dense regime) ==="
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case3.out
exit
