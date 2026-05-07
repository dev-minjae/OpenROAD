# Phase 4.4 self-frontend — no parse_iccad2022_output (paper coord borrow).
# Self-contained pipeline: read → flat 2D GP → flat partition → GTO → SemiLeg.
# No baseline locked yet (this script is for measurement, not a numerical
# lock). Compare against paper Table I "Flattened GP HPWL" and the existing
# Xueyan-borrowed regression baseline.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt

run_flattened_placement
puts "=== HPWL after flattened placement ==="
get_3d_hpwl

# All cells start on die 0; GTO migrates some to die 1.
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_self_case2.par
set_mdm_partition_file -file /tmp/flat_self_case2.par
set_3D_IC -die_number 2
puts "=== HPWL after set_3D_IC ==="
get_3d_hpwl

run_global_tier_optimization -apply
puts "=== HPWL after GTO ==="
get_3d_hpwl

run_planar_correcting -iterations 1
puts "=== HPWL after Planar ==="
get_3d_hpwl

run_semi_legalizer
puts "=== HPWL after SemiLeg ==="
get_3d_hpwl

write_iccad2022_output -out /tmp/regression_phase4_self_case2.out
exit
