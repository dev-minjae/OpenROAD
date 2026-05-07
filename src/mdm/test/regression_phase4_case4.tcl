# Phase 4 case4 e2e numerical lock — paper-equivalent frontend (Xueyan post-GP)
# + our backend (SemiLeg + TermLeg, Planar Correcting skipped).
# Baseline: HPWL = 265,734,241 (paper Ours = 267,381,744, gap -0.6% — better).
# Tolerance: +/- 0.5%.
#
# Strategy (divide-and-conquer):
#   - Frontend (partition + GP coord) is loaded from Xueyan reference data,
#     making it paper-equivalent (paper case4 #Term = 43140, ours = 43189).
#   - Our differentiation = backend. The lock measures backend quality only.
#   - case4 (util 66/70, moderate) skips Planar — Skip 265,734,241 vs +Planar
#     266,860,344 (Skip -0.4% better). Same Skip-wins pattern as case3 here,
#     suggesting Planar destructiveness extends below the case3 dense regime.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
set_mdm_partition_file -file /tmp/ref_gp/case4_gp.txt.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
puts "=== Skipping Planar Correcting ==="
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case4.out
exit
