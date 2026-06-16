# thm - 3D-IC Thermal Analysis (therm3d)

GPU-accelerated 3D-IC thermal early-exploration engine, embedded in OpenROAD.
Solves the compact FDM heat equation `C*dT/dt + G*T = P` over a stacked-die
grid, consuming the OpenROAD 3D-IC model (3dblox / `dbChip` / `UnfoldedModel`)
and rendering temperature as a GUI heat map. 3D-ICE / HotSpot serve as
validation oracles, not backends.

## Layout (hard two-layer boundary)

- `core/` - **pure numerical engine** (`thm_core`). No OpenROAD headers or
  links; I/O is plain C++ structs/arrays. Independently buildable/testable,
  GPU (Kokkos) work lives here, and it can be extracted to the standalone
  `therm3d` repo unchanged.
- `include/thm`, `src/` - **adapter**: the only OpenROAD touchpoint. Reads the
  DB, exposes Tcl commands, logs under tool tag `THM`, renders heat maps.

## Status

**T0 (scaffold)** - module builds and registers the `analyze_thermal` Tcl
command, which logs one sanity line. No physics yet.

Roadmap: T1 minimal steady-state FDM solver -> T2 consume odb 3D stack ->
T3 transient + heat map -> T4 stack sweep + oracle cross-check -> T5 GPU.

## Command

```
analyze_thermal
# [INFO THM-0001] thermal module alive (core thm-core 0.0.0 ...); design: none
```
