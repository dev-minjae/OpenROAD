# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors

sta::define_cmd_args "analyze_thermal" {}

proc analyze_thermal { args } {
  sta::parse_key_args "analyze_thermal" args keys {} flags {}
  thm::analyze_thermal_cmd
}

sta::define_cmd_args "sweep_thermal" {[-dies list]\
                                      [-tsv_diameter_um value]\
                                      [-tsv_pitch_um value]\
                                      [-bump_diameter_um value]\
                                      [-bump_pitch_um value]\
                                      [-hybrid_cu_coverage value]}

proc sweep_thermal { args } {
  sta::parse_key_args "sweep_thermal" args \
    keys {-dies -tsv_diameter_um -tsv_pitch_um -bump_diameter_um \
          -bump_pitch_um -hybrid_cu_coverage} \
    flags {}

  set dies "12 16 20"
  if { [info exists keys(-dies)] } { set dies $keys(-dies) }
  set tsv_d 5.0
  if { [info exists keys(-tsv_diameter_um)] } { set tsv_d $keys(-tsv_diameter_um) }
  set tsv_p 40.0
  if { [info exists keys(-tsv_pitch_um)] } { set tsv_p $keys(-tsv_pitch_um) }
  set bump_d 20.0
  if { [info exists keys(-bump_diameter_um)] } { set bump_d $keys(-bump_diameter_um) }
  set bump_p 45.0
  if { [info exists keys(-bump_pitch_um)] } { set bump_p $keys(-bump_pitch_um) }
  set hyb 0.20
  if { [info exists keys(-hybrid_cu_coverage)] } { set hyb $keys(-hybrid_cu_coverage) }

  thm::sweep_thermal_cmd $dies $tsv_d $tsv_p $bump_d $bump_p $hyb
}

sta::define_cmd_args "dump_thermal_stack" {[-num_dies n]\
                                           [-bond_type microbump|hybrid]\
                                           [-tsv_diameter_um value]\
                                           [-tsv_pitch_um value]\
                                           [-bump_diameter_um value]\
                                           [-bump_pitch_um value]\
                                           [-hybrid_cu_coverage value]}

proc dump_thermal_stack { args } {
  sta::parse_key_args "dump_thermal_stack" args \
    keys {-num_dies -bond_type -tsv_diameter_um -tsv_pitch_um \
          -bump_diameter_um -bump_pitch_um -hybrid_cu_coverage} \
    flags {}

  set n 4
  if { [info exists keys(-num_dies)] } { set n $keys(-num_dies) }
  set bt microbump
  if { [info exists keys(-bond_type)] } { set bt $keys(-bond_type) }
  set tsv_d 5.0
  if { [info exists keys(-tsv_diameter_um)] } { set tsv_d $keys(-tsv_diameter_um) }
  set tsv_p 40.0
  if { [info exists keys(-tsv_pitch_um)] } { set tsv_p $keys(-tsv_pitch_um) }
  set bump_d 20.0
  if { [info exists keys(-bump_diameter_um)] } { set bump_d $keys(-bump_diameter_um) }
  set bump_p 45.0
  if { [info exists keys(-bump_pitch_um)] } { set bump_p $keys(-bump_pitch_um) }
  set hyb 0.20
  if { [info exists keys(-hybrid_cu_coverage)] } { set hyb $keys(-hybrid_cu_coverage) }

  thm::dump_thermal_stack_cmd $n $bt $tsv_d $tsv_p $bump_d $bump_p $hyb
}
