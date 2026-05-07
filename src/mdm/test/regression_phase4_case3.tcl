# Phase 4 case3 e2e numerical lock — paper-equivalent frontend (Xueyan post-GP)
# + our backend (SemiLeg + TermLeg, Planar Correcting skipped).
# Baseline: HPWL = 30,229,424 (paper Ours = 30,234,112, gap -0.02% — better).
# Tolerance: +/- 0.5%.
#
# Strategy (divide-and-conquer):
#   - Frontend (partition + GP coord) is loaded from Xueyan reference data,
#     making it paper-equivalent (paper case3 #Term = 9612, ours = 9471).
#   - Our differentiation = backend. The lock measures backend quality only.
#   - case3 (util 78/78, dense) skips Planar Correcting — +Planar/Skip
#     comparison: Skip 30,229,424 vs +Planar 30,388,283 (Skip -0.5% better).
#     Note this is the OPPOSITE of case2; the dense regime makes Planar
#     marginally destructive even when starting from paper-equivalent coord.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case3_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
puts "=== Skipping Planar Correcting (case3 dense regime) ==="
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case3.out
exit
