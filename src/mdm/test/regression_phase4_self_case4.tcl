# Phase 4.4 self-frontend — case4 (Skip Planar; Skip is winner per prior cycle).
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

exec awk "{print \$1, 0}" /tmp/ref_gp/case4_gp.txt.par > /tmp/flat_self_case4.par
set_mdm_partition_file -file /tmp/flat_self_case4.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

puts "=== Skipping Planar Correcting (case4 — Skip winner per prior cycle) ==="

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case4.out
exit
