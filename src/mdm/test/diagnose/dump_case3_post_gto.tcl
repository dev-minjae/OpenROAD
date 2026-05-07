# case3 post-GTO dump for diagnosis. flat init + paper-equivalent partition
# is loaded as starting state, our GTO redoes partitioning + does cell
# migration. The .out captures our post-GTO cells coords for comparison
# against paper post-GP.
set_iccad_scale -scale 2000
read_iccad2022 -case /home/minjae/workspace/etc/openroad/archive/3d_ic/benchmarks/iccad2022/case3.txt
parse_iccad2022_output -file /tmp/ref_gp/case3_gp.txt
exec awk "{print \$1, 0}" /tmp/ref_gp/case3_gp.txt.par > /tmp/flat_case3.par
set_mdm_partition_file -file /tmp/flat_case3.par
set_3D_IC -die_number 2
run_global_tier_optimization -apply
write_iccad2022_output -out /tmp/diagnose_case3_post_gto.out
exit
