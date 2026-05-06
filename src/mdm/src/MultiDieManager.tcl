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
  [-no_cells_dynamic_row] \
  [-skip_pair_swap] \
  [-tetris]}

proc run_semi_legalizer { args } {
  sta::parse_key_args "run_semi_legalizer" args \
    keys {-target_die} \
    flags {-no_abacus -no_cells_dynamic_row -skip_pair_swap -tetris}

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
  set skip_pair_swap false
  if { [info exists flags(-skip_pair_swap)] } {
    set skip_pair_swap true
  }
  set use_tetris false
  if { [info exists flags(-tetris)] } {
    set use_tetris true
  }
  mdm::run_semi_legalizer $target $use_abacus $use_cells_dynamic_row \
                          $skip_pair_swap $use_tetris
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

# Phase 4 — iPL-3D paper §IV.B Algorithm 2 single-shot.
sta::define_cmd_args "run_global_tier_optimization" {\
    [-rho rho] [-alpha alpha] [-beta beta] [-gamma gamma] [-apply]}

proc run_global_tier_optimization { args } {
  sta::parse_key_args "run_global_tier_optimization" args \
    keys {-rho -alpha -beta -gamma} flags {-apply}

  # Paper Table III defaults — surrogate now runs in normalized μm
  # (auto-converted from dbu via getICCADScale), so paper's constants
  # apply directly.
  set rho 500.0
  set alpha 100.0
  set beta 0.5
  set gamma 0.0
  set apply 0
  if { [info exists keys(-rho)] } {
    set rho $keys(-rho)
  }
  if { [info exists keys(-alpha)] } {
    set alpha $keys(-alpha)
  }
  if { [info exists keys(-beta)] } {
    set beta $keys(-beta)
  }
  if { [info exists keys(-gamma)] } {
    set gamma $keys(-gamma)
  }
  if { [info exists flags(-apply)] } {
    set apply 1
  }
  mdm::run_global_tier_optimization $rho $alpha $beta $gamma $apply
}

# Phase 4 — iPL-3D paper §IV.D Planar Solution Correcting (SP-2).
sta::define_cmd_args "run_planar_correcting" {[-iterations iter]}

proc run_planar_correcting { args } {
  sta::parse_key_args "run_planar_correcting" args \
    keys {-iterations} flags {}

  set iterations 1
  if { [info exists keys(-iterations)] } {
    set iterations $keys(-iterations)
  }
  mdm::run_planar_correcting $iterations
}

# Helper: snap each child-die cell's y to nearest row. Closes the
# free-form-output → row-aligned-input gap between Planar Correcting
# and CellsLegalizer.
sta::define_cmd_args "snap_cells_to_rows" {}

proc snap_cells_to_rows { args } {
  mdm::snap_cells_to_rows
}

