// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026, The OpenROAD Authors
//
// Standalone validation of the thm_core steady-state FDM solver. Links only
// thm_core (no OpenROAD, no test framework), so it runs regardless of
// OpenROAD's ENABLE_TESTS. Returns non-zero on any failure.

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "thm_core/SteadySolver.h"
#include "thm_core/TransientSolver.h"

namespace {

using thm::core::Layer;
using thm::core::SteadyProblem;
using thm::core::SteadyResult;
using thm::core::solveSteady;
using thm::core::solveTransient;
using thm::core::TransientResult;

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

// Test C: a single-cell, single-layer problem is a first-order RC system. Check
// the backward-Euler step response against the analytic exponential
// T(t) = T_inf - (T_inf - T_init) e^{-t/tau}, with tau = R*C, R = the half-cell
// resistance to the boundary and C = rho*c_p * cell volume.
void testRcStepResponse()
{
  std::printf("Test C: single-cell RC step response vs analytic exponential\n");
  SteadyProblem p;
  p.layers = {{100e-6, 130.0, 1.63e6}};  // thickness, k, rho*c_p
  p.nx = 1;
  p.ny = 1;
  p.dx_m = 1e-3;
  p.dy_m = 1e-3;
  p.t_bc_celsius = 25.0;
  p.power_w = {1.0};

  const double area = p.dx_m * p.dy_m;
  const Layer& layer = p.layers[0];
  const double g_bc
      = area / (layer.thickness_m / (2.0 * layer.conductivity_w_per_mk));
  const double capacitance
      = layer.volumetric_heat_capacity_j_per_m3k * (area * layer.thickness_m);
  const double tau = capacitance / g_bc;
  const double t_inf = p.t_bc_celsius + p.power_w[0] / g_bc;

  const int steps = 2000;
  const double dt = tau / steps;  // integrate to exactly t = tau
  const TransientResult r = solveTransient(p, dt, steps, p.t_bc_celsius);

  const double t_analytic
      = p.t_bc_celsius + (t_inf - p.t_bc_celsius) * (1.0 - std::exp(-1.0));
  const double rel = std::fabs(r.peak_celsius - t_analytic)
                     / std::fabs(t_inf - p.t_bc_celsius);
  // Backward-Euler is first order in dt, so the error is O(dt/tau) ~ 1/steps.
  check(rel < 2e-3, "T(tau) vs analytic (rel err)", rel, 2e-3);
}

// Test D: backward-Euler is unconditionally stable and its fixed point is the
// steady solution, so a long-enough transient must converge to solveSteady.
void testTransientConvergesToSteady()
{
  std::printf("Test D: transient converges to the steady solution\n");
  SteadyProblem p;
  p.layers = {{200e-6, 150.0, 1.6e6},
              {30e-6, 1.0, 1.5e6},
              {80e-6, 120.0, 1.6e6}};
  p.nx = 4;
  p.ny = 3;
  p.dx_m = 120e-6;
  p.dy_m = 90e-6;
  p.t_bc_celsius = 30.0;
  const int nxy = p.nx * p.ny;
  const int nz = static_cast<int>(p.layers.size());
  p.power_w.assign(static_cast<std::size_t>(nxy) * nz, 0.0);
  p.power_w[(nz - 1) * nxy + 1 * p.nx + 2] = 0.5;

  const SteadyResult steady = solveSteady(p);
  // dt large vs the thermal time constants -> converges within a few steps.
  const TransientResult tr = solveTransient(p, 1e-2, 200, p.t_bc_celsius);

  double max_abs = 0.0;
  for (std::size_t i = 0; i < steady.temperature_celsius.size(); ++i) {
    max_abs = std::max(
        max_abs,
        std::fabs(tr.temperature_celsius[i] - steady.temperature_celsius[i]));
  }
  check(max_abs < 1e-6, "max |transient - steady| [K]", max_abs, 1e-6);
}

}  // namespace

int main()
{
  std::printf("thm_core thermal solver - validation\n");
  testColumnVsLadder();
  testEnergyConservation();
  testRcStepResponse();
  testTransientConvergesToSteady();
  std::printf("%s (%d failure(s))\n", g_failures ? "FAILED" : "OK", g_failures);
  return g_failures ? 1 : 0;
}
