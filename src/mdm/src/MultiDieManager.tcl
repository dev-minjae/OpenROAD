# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2021-2026, The OpenROAD Authors

sta::define_cmd_args "set_3D_IC" {[-die_number die_number]}

proc set_3D_IC { args } {
  sta::parse_key_args "set_3D_IC" args \
    keys {-die_number} flags {}

  if { ![info exists keys(-die_number)] } {
    utl::error MDM 100 "-die_number is required."
  }

  set die_number $keys(-die_number)
  mdm::set_3D_IC $die_number
}

sta::define_cmd_args "read_iccad2022" {[-case case_file]}

proc read_iccad2022 { args } {
  sta::parse_key_args "read_iccad2022" args \
    keys {-case} flags {}

  if { ![info exists keys(-case)] } {
    utl::error MDM 101 "-case is required."
  }
  mdm::read_iccad2022 $keys(-case)
}

sta::define_cmd_args "write_iccad2022_output" {[-out out_file]}

proc write_iccad2022_output { args } {
  sta::parse_key_args "write_iccad2022_output" args \
    keys {-out} flags {}

  if { ![info exists keys(-out)] } {
    utl::error MDM 102 "-out is required."
  }
  mdm::write_iccad2022_output $keys(-out)
}

sta::define_cmd_args "parse_iccad2022_output" {[-file file] [-die die]}

proc parse_iccad2022_output { args } {
  sta::parse_key_args "parse_iccad2022_output" args \
    keys {-file -die} flags {}

  if { ![info exists keys(-file)] } {
    utl::error MDM 103 "-file is required."
  }
  set die ""
  if { [info exists keys(-die)] } {
    set die $keys(-die)
  }
  mdm::parse_iccad2022_output $keys(-file) $die
}

sta::define_cmd_args "set_iccad_scale" {[-scale scale]}

proc set_iccad_scale { args } {
  sta::parse_key_args "set_iccad_scale" args \
    keys {-scale} flags {}

  if { ![info exists keys(-scale)] } {
    utl::error MDM 104 "-scale is required."
  }
  mdm::set_iccad_scale $keys(-scale)
}

sta::define_cmd_args "set_mdm_partition_file" {[-file partition_file]}

proc set_mdm_partition_file { args } {
  sta::parse_key_args "set_mdm_partition_file" args \
    keys {-file} flags {}

  if { ![info exists keys(-file)] } {
    utl::error MDM 105 "-file is required."
  }
  mdm::set_mdm_partition_file $keys(-file)
}
