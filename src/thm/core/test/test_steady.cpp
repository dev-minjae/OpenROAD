// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors
//
// Standalone validation of the thm_core steady-state FDM solver. Links only
// thm_core (no OpenROAD, no test framework), so it runs regardless of
// OpenROAD's ENABLE_TESTS. Returns non-zero on any failure.

#include <cmath>
#include <cstdio>
#include <vector>

#include "thm_core/SteadySolver.h"

namespace {

using thm::core::Layer;
using thm::core::SteadyProblem;
using thm::core::SteadyResult;
using thm::core::solveSteady;

int g_failures = 0;

void check(bool ok, const char* name, double value, double tol)
{
  std::printf("  [%s] %-42s = %.3e (tol %.0e)\n",
              ok ? "PASS" : "FAIL",
              name,
              value,
              tol);
  if (!ok) {
    ++g_failures;
  }
}

// Test A: a 1-D heterogeneous column has no transverse conduction, so the
// discrete system is exactly a series resistor ladder. Compare every node
// temperature to that closed form (expect ~machine precision).
void testColumnVsLadder()
{
  std::printf("Test A: 1-D heterogeneous column vs analytic resistor ladder\n");
  SteadyProblem p;
  p.layers = {{300e-6, 130.0}, {20e-6, 0.8}, {50e-6, 130.0}};  // Si/underfill/Si
  p.nx = 1;
  p.ny = 1;
  p.dx_m = 1e-3;
  p.dy_m = 1e-3;
  p.t_bc_celsius = 25.0;
  const int n = static_cast<int>(p.layers.size());
  p.power_w.assign(n, 0.0);
  const double power = 1.5;
  p.power_w[n - 1] = power;  // inject at the top cell
  const SteadyResult r = solveSteady(p);

  const double area = p.dx_m * p.dy_m;
  // Resistance from each node down to the boundary. All injected power flows
  // through the whole chain, so T(node l) = t_bc + power * R(node l -> bc).
  std::vector<double> r_to_bc(n);
  double acc = (p.layers[0].thickness_m / 2.0)
               / (p.layers[0].conductivity_w_per_mk * area);  // cell0 -> bc
  r_to_bc[0] = acc;
  for (int l = 1; l < n; ++l) {
    acc += (p.layers[l - 1].thickness_m / 2.0)
               / (p.layers[l - 1].conductivity_w_per_mk * area)
           + (p.layers[l].thickness_m / 2.0)
                 / (p.layers[l].conductivity_w_per_mk * area);
    r_to_bc[l] = acc;
  }
  double max_rel = 0.0;
  for (int l = 0; l < n; ++l) {
    const double t_analytic = p.t_bc_celsius + power * r_to_bc[l];
    const double rise = t_analytic - p.t_bc_celsius;
    const double rel
        = std::fabs(r.temperature_celsius[l] - t_analytic) / std::fabs(rise);
    max_rel = std::max(max_rel, rel);
  }
  check(max_rel < 1e-9, "node field vs ladder (rel err)", max_rel, 1e-9);

  // Reconcile with the engineering full-stack R_th = sum(t_i/(k_i*A)) measured
  // to the TOP SURFACE: surface temp = top-node temp + the top half-cell drop.
  double r_full = 0.0;
  for (const Layer& layer : p.layers) {
    r_full += layer.thickness_m / (layer.conductivity_w_per_mk * area);
  }
  const double t_top_surface
      = r.temperature_celsius[n - 1]
        + power * (p.layers[n - 1].thickness_m / 2.0)
              / (p.layers[n - 1].conductivity_w_per_mk * area);
  const double t_surface_analytic = p.t_bc_celsius + power * r_full;
  const double rel_surface = std::fabs(t_top_surface - t_surface_analytic)
                             / std::fabs(t_surface_analytic - p.t_bc_celsius);
  check(rel_surface < 1e-9,
        "top-surface T = power*sum(t/kA)+t_bc",
        rel_surface,
        1e-9);
}

// Test B: in steady state every watt injected must leave through the only
// non-adiabatic boundary, so sum of boundary flux == sum of injected power.
void testEnergyConservation()
{
  std::printf("Test B: energy conservation (boundary flux = injected power)\n");
  SteadyProblem p;
  p.layers = {{200e-6, 150.0}, {30e-6, 1.0}, {80e-6, 120.0}};
  p.nx = 5;
  p.ny = 4;
  p.dx_m = 120e-6;
  p.dy_m = 90e-6;
  p.t_bc_celsius = 30.0;
  const int nxy = p.nx * p.ny;
  const int nz = static_cast<int>(p.layers.size());
  p.power_w.assign(nxy * nz, 0.0);
  p.power_w[(nz - 1) * nxy + 2 * p.nx + 2] = 0.7;  // hot cell, top layer
  p.power_w[(nz - 1) * nxy + 1 * p.nx + 0] = 0.3;  // another, top layer
  p.power_w[0 * nxy + 3 * p.nx + 4] = 0.2;         // some heat in bottom layer
  const SteadyResult r = solveSteady(p);

  double injected = 0.0;
  for (const double w : p.power_w) {
    injected += w;
  }
  const double area = p.dx_m * p.dy_m;
  const double k0 = p.layers[0].conductivity_w_per_mk;
  const double t0 = p.layers[0].thickness_m;
  const double g_bc = area / (t0 / (2.0 * k0));
  double flux = 0.0;
  for (int j = 0; j < p.ny; ++j) {
    for (int i = 0; i < p.nx; ++i) {
      const int c = j * p.nx + i;  // bottom layer (l = 0)
      flux += g_bc * (r.temperature_celsius[c] - p.t_bc_celsius);
    }
  }
  const double rel = std::fabs(flux - injected) / std::fabs(injected);
  check(rel < 1e-9, "|boundary flux - injected|/injected", rel, 1e-9);
  std::printf(
      "    peak = %.3f C, R_th = %.4f K/W, injected = %.3f W, flux = %.6f W\n",
      r.peak_celsius,
      r.r_th_k_per_w,
      injected,
      flux);
}

}  // namespace

int main()
{
  std::printf("thm_core steady-state FDM solver - validation\n");
  testColumnVsLadder();
  testEnergyConservation();
  std::printf("%s (%d failure(s))\n", g_failures ? "FAILED" : "OK", g_failures);
  return g_failures ? 1 : 0;
}
