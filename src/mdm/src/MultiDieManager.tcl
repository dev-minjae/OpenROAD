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
