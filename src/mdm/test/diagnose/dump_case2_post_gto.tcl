# case2 post-GTO dump. Same pattern as case3 with case2 paths.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case2.txt
parse_iccad2022_output -file /tmp/case2_ipl.out
exec awk "{print \$1, 0}" /tmp/case2_ipl.out.par > /tmp/flat_case2.par
set_mdm_partition_file -file /tmp/flat_case2.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
write_iccad2022_output -out /tmp/diagnose_case2_post_gto.out
exit
