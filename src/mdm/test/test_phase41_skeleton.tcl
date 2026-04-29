# Phase 4.1 skeleton smoke test: invoke each new Tcl command and observe
# the logger emits the expected MDM-0300/0301/0302/0303/0304/0305/0306
# messages. All commands return without crashing; no algorithm runs.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
set_mdm_partition_file -file /tmp/case2_ipl.out.par
set_3D_IC -die_number 2

puts "=== Phase 4.1 skeleton: run_flattened_placement ==="
run_flattened_placement -density 1.0

puts "=== Phase 4.1 skeleton: run_global_tier_optimization ==="
run_global_tier_optimization -rho 500.0 -alpha 100.0 -beta 0.5 -gamma 0.0

puts "=== Phase 4.1 skeleton: run_3d_placement ==="
run_3d_placement -iterations 2

puts "=== Phase 4.1 skeleton: all stub commands returned ==="
exit
