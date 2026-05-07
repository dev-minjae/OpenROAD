# Phase 4.4 self-frontend — case3 (dense; Skip Planar per prior cycle finding).
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_self_case3.par
set_mdm_partition_file -file /tmp/flat_self_case3.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

# Skip Planar for case3 (dense regime — see prior regression_phase4_case3.tcl)
puts "=== Skipping Planar Correcting (case3 dense) ==="

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case3.out
exit
