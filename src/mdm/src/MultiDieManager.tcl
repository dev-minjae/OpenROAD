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

sta::define_cmd_args "multi_die_detail_placement" {\
  [-max_displacement_x max_displacement_x] \
  [-max_displacement_y max_displacement_y]}

proc multi_die_detail_placement { args } {
  sta::parse_key_args "multi_die_detail_placement" args \
    keys {-max_displacement_x -max_displacement_y} flags {}

  set dx 0
  set dy 0
  if { [info exists keys(-max_displacement_x)] } {
    set dx $keys(-max_displacement_x)
  }
  if { [info exists keys(-max_displacement_y)] } {
    set dy $keys(-max_displacement_y)
  }
  mdm::multi_die_detail_placement $dx $dy
}

sta::define_cmd_args "run_semi_legalizer" {\
  [-target_die target_die] \
  [-no_abacus] \
  [-no_cells_dynamic_row]}

proc run_semi_legalizer { args } {
  sta::parse_key_args "run_semi_legalizer" args \
    keys {-target_die} flags {-no_abacus -no_cells_dynamic_row}

  set target ""
  if { [info exists keys(-target_die)] } {
    set target $keys(-target_die)
    if { $target ne "" && $target ne "top" && $target ne "bottom" } {
      utl::error MDM 108 "-target_die must be \"top\", \"bottom\", or empty."
    }
  }
  set use_abacus true
  if { [info exists flags(-no_abacus)] } {
    set use_abacus false
  }
  set use_cells_dynamic_row true
  if { [info exists flags(-no_cells_dynamic_row)] } {
    set use_cells_dynamic_row false
  }
  mdm::run_semi_legalizer $target $use_abacus $use_cells_dynamic_row
}

sta::define_cmd_args "get_3d_hpwl" {[-exact]}

proc get_3d_hpwl { args } {
  sta::parse_key_args "get_3d_hpwl" args \
    keys {} flags {-exact}

  set approximate true
  if { [info exists flags(-exact)] } {
    set approximate false
  }
  mdm::get_3d_hpwl $approximate
}

sta::define_cmd_args "get_hpwl" {[-die die]}

proc get_hpwl { args } {
  sta::parse_key_args "get_hpwl" args \
    keys {-die} flags {}

  set die ""
  if { [info exists keys(-die)] } {
    set die $keys(-die)
  }
  mdm::get_hpwl $die
}

sta::define_cmd_args "export_inst_coordinates" {[-file file]}

proc export_inst_coordinates { args } {
  sta::parse_key_args "export_inst_coordinates" args \
    keys {-file} flags {}

  if { ![info exists keys(-file)] } {
    utl::error MDM 106 "-file is required."
  }
  mdm::export_coordinates $keys(-file)
}

sta::define_cmd_args "import_inst_coordinates" {[-file file]}

proc import_inst_coordinates { args } {
  sta::parse_key_args "import_inst_coordinates" args \
    keys {-file} flags {}

  if { ![info exists keys(-file)] } {
    utl::error MDM 107 "-file is required."
  }
  mdm::import_coordinates $keys(-file)
}
