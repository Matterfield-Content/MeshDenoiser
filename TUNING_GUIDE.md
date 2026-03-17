# Tuning Guide

## Recommended Defaults
- Solver backend default: `LinearSolverType 2` (CHOLMOD), with fallback to `1` if CHOLMOD is unavailable in the build.
- Keep `DeterministicMode 0` for fastest runs.
- Set `DeterministicMode 1` only for reproducibility checks.

## Mesh Size Presets
- `< 1M triangles`
  - `OuterIterations 2-5`
  - `MeshUpdateIterations 5-20`
  - `MeshUpdateDisplacementEps 1e-2` to `1e-1`
- `1M - 5M triangles`
  - `LinearSolverType 2`
  - `MeshUpdateDisplacementEps 1e-1`
- `5M+ triangles`
  - `LinearSolverType 2`
  - start with lower `OuterIterations`
  - keep `MeshUpdateDisplacementEps` relatively loose and compare quality against runtime

## Recommended Workflow
1. Run once with `--metrics-json` enabled.
2. Inspect `timing.filtering_secs` and `timing.mesh_update_secs`.
3. If mesh update dominates, prefer CHOLMOD and tune `MeshUpdateDisplacementEps`.
4. If filtering dominates, reduce outer iterations only after checking output quality.

## Reproducibility
- Use `DeterministicMode 1` in the option file or pass `--deterministic`.
- Run `python tools/test_determinism.py ...` for a two-run hash check.
