# case4 post-GTO+Planar dump.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case4.txt
parse_iccad2022_output -file /tmp/ref_gp/case4_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case4_gp.txt.par > /tmp/flat_case4.par
set_mdm_partition_file -file /tmp/flat_case4.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
run_planar_correcting -iterations 1
write_iccad2022_output -out /tmp/diagnose_case4_post_planar.out
exit
