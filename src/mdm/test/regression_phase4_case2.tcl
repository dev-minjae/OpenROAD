# Phase 4 numerical lock — case2 e2e for HPWL regression check.
# Baseline: HPWL = 2,694,774 (recorded at HEAD df11a310a7).
# Tolerance: +/- 0.5%.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2
puts "=== HPWL after flat init ==="
get_3d_hpwl
run_global_tier_optimization -apply
puts "=== HPWL after Algorithm 2 + apply ==="
get_3d_hpwl
run_planar_correcting -iterations 1
puts "=== HPWL after Planar Correcting (1 iter) ==="
get_3d_hpwl
run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl
write_iccad2022_output -out /tmp/regression_phase4_case2.out
exit
