# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026, The OpenROAD Authors

sta::define_cmd_args "analyze_thermal" {}

proc analyze_thermal { args } {
  sta::parse_key_args "analyze_thermal" args keys {} flags {}
  thm::analyze_thermal_cmd
}
