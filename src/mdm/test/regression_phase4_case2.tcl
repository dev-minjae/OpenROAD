# Phase 4 case2 e2e numerical lock — paper-equivalent frontend (Xueyan post-GP)
# + our backend (Planar Correcting + SemiLeg + TermLeg).
# Baseline: HPWL = 2,004,424 (paper Ours = 1,992,499, gap +0.6%).
# Tolerance: +/- 0.5%.
#
# Strategy (divide-and-conquer):
#   - Frontend (partition + GP coord) is loaded from Xueyan reference data,
#     making it paper-equivalent (paper case2 #Term = 461, ours = 463).
#   - Our differentiation = backend. The lock measures backend quality only.
#   - case2 (util 70/75, sparser) keeps Planar Correcting — Planar +Skip
#     comparison: +Planar 2,004,424 vs Skip 2,006,967 (Planar -0.13% better).

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
set_mdm_partition_file -file /tmp/case2_ipl.out.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC (Xueyan post-GP) ==="
get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case2.out
exit
