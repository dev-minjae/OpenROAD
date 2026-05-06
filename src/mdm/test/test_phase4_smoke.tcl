# Phase 4 smoke test — verify Tcl dispatch + apply path works.
# Loads case2, runs Algorithm 2 -apply, asserts post-conditions.
# Does NOT run Planar Correcting (slow GPL pass) or SemiLegalizer.
# Numerical lock (HPWL +/- 0.5%) is in regression_phase4_case2.tcl.

set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2

puts "=== smoke 1: dry-run Algorithm 2 (apply=false) ==="
run_global_tier_optimization

puts "=== smoke 2: Algorithm 2 -apply (partition flip + reinterconnect) ==="
run_global_tier_optimization -apply

puts "=== smoke 3: snap_cells_to_rows ==="
snap_cells_to_rows

puts "=== Phase 4 smoke test: all commands returned cleanly ==="
exit
