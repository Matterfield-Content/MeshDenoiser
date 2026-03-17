# Mesh SD Filter Modernization Roadmap

## Scope and Goals
- Modernize SD mesh denoising for high triangle counts (10M+ faces target).
- Improve end-to-end throughput: import, denoise iterations, mesh update solve, export.
- Add explicit ASCII OBJ import and export pathway using rapidOBJ for parsing.
- Keep numerical behavior close to current implementation by default.

## Current Status (2026-03-06)
- M0: Completed
- M1: Completed (rapidOBJ-only OBJ I/O; no non-OBJ runtime path)
- M2: Completed
- M3: Completed
- M4-M5: Prototype work completed, but removed from the shipping product
  - CUDA filter and sparse-solve paths were benchmarked and did not deliver a net end-to-end win for the current production workload.
  - The project now ships CPU-only denoising to simplify deployment and avoid CUDA runtime dependencies.
- M6: Completed (hardening/docs)
  - Implemented determinism mode:
    - option file key `DeterministicMode`
    - CLI override `--deterministic`
    - single-thread OpenMP/Eigen runtime configuration for reproducible runs.
  - Added reproducibility validation utility: `tools/test_determinism.py`.
  - Improved OBJ robustness in rapidOBJ pipeline:
    - explicit handling/reporting for non-triangle triangulation
    - malformed face-index stream detection
    - oversized vertex/index range guardrails
    - non-finite coordinate rejection on import/export.
  - Added production docs:
    - `TUNING_GUIDE.md` (presets + hardware matrix)
    - `MIGRATION_GUIDE.md` (legacy-to-current CLI/options migration)

## Baseline and Success Metrics
Track these before/after every milestone using fixed benchmark meshes:
- Runtime split: import, preprocessing, SD iterations, mesh update, export.
- Peak RSS memory.
- Iteration count to convergence.
- Quality delta against current output:
  - face normal angle stats
  - normalized vertex displacement
  - optional Hausdorff proxy (sampled)

Target outcomes:
- 2-4x faster CPU path on 2M-5M faces.
- 2x faster OBJ ingest/export for large ASCII meshes.

## Milestone Plan

### M0 - Instrumentation and Repro Harness
Deliverables:
- Add benchmark runner and per-stage timers with CSV/JSON outputs.
- Add canonical test set (small/medium/large; manifold and non-manifold cases).
- Add CI perf smoke test (non-regression threshold).

Acceptance:
- Every run reports stage timings and memory.
- Baseline data committed for all benchmark meshes.

### M1 - RapidOBJ ASCII OBJ I/O Integration
Deliverables:
- Add a new I/O layer (`MeshIO.h/.cpp`) with format dispatch.
- OBJ import path uses rapidOBJ parser for `.obj` by default.
- OBJ export path guarantees ASCII OBJ output.
- Preserve current OpenMesh import/export as fallback for non-OBJ formats.
- CLI flags/options:
  - `--obj-export-precision=<int>`

Implementation notes:
- rapidOBJ is excellent for parsing OBJ; verify writer availability in your pinned version.
- If rapidOBJ does not provide writing in the selected version, keep rapidOBJ for import and implement a dedicated fast ASCII OBJ writer in-house (streamed, buffered), while keeping interface labeled as rapidOBJ-backed OBJ pipeline.

Acceptance:
- Reads and writes ASCII OBJ without data loss in vertex/face topology.
- Round-trip tests pass on benchmark set.
- Import speedup >= 1.5x versus current OpenMesh OBJ path on large files.

### M2 - Data Layout and Neighborhood Build Refactor
Deliverables:
- Replace per-face BFS with spatial index on face centroids (radius query).
- Replace repeated `vector<bool>` allocations with stamp-based visited arrays.
- Store neighbor graph in CSR-like arrays for contiguous traversal.
- Remove hot-path `sort/erase` patterns where possible.

Suggested libraries:
- `nanoflann` or `hnswlib` (exact mode preferred first) for centroid radius queries.
- Keep Eigen for math at this stage.

Acceptance:
- Preprocessing time reduced >= 2x on large meshes.
- Memory growth is linear and bounded (no per-face O(F) temporary allocations).

### M3 - CPU Parallel Runtime and Sparse Solve Upgrade
Deliverables:
- Replace OpenMP macro wrappers with backend abstraction (`OpenMP`/`oneTBB` switch).
- Improve scheduling and cache locality for pair-weight and accumulation loops.
- Upgrade sparse solver backend for SPD systems behind existing solver interface.

Suggested high-impact libraries:
- Intel oneTBB: task scheduling and work stealing for irregular mesh workloads.
- oneMKL PARDISO or SuiteSparse CHOLMOD: faster robust sparse factorization/solve than Eigen `SimplicialLDLT` in large systems.
- Optional: `Kokkos Kernels` if portability to GPU backends is a medium-term requirement.

Acceptance:
- Mesh update solve stage reduced >= 1.5x for 1M+ vertices.
- No quality regression beyond tolerance thresholds.

### M6 - Production Hardening
Deliverables:
- Determinism mode and reproducibility tests.
- Robust error handling for invalid OBJ, non-triangle faces, huge indices.
- Docs: tuning guide, recommended presets by mesh size, hardware matrix.

Acceptance:
- Stable outputs across repeated runs in determinism mode.
- Complete migration guide from legacy CLI/options.

## Library Recommendations by Impact
Highest expected benefit first:
1. Sparse solver backend: `oneMKL PARDISO` or `SuiteSparse CHOLMOD`.
2. Task runtime: `oneTBB` (especially for irregular adjacency-heavy phases).
3. OBJ import parser: `rapidOBJ` (ASCII parse throughput and low overhead).
4. Spatial queries: `nanoflann` (fast exact KD-tree for centroid/radius neighborhood).
5. GPU compute: CUDA (`Thrust/CUB`, `cuSPARSE`, `cuSOLVER`) or portability stack (`Kokkos`/SYCL).

## Risks and Mitigations
- Numerical drift from backend changes:
  - Mitigate with strict baseline comparison tests and tolerances.
- rapidOBJ writer capability mismatch by version:
  - Mitigate with version pin + fallback custom ASCII OBJ writer.
- Memory pressure at very high face counts:
  - Mitigate using CSR, chunked processing, and 32-bit index paths where valid.

## Suggested Execution Order
1. M0
2. M1
3. M2
4. M3
5. M6

## Immediate Next Tasks (Sprint 1)
- Implement M0 timing harness and benchmark corpus metadata.
- Implement M1 `MeshIO` abstraction and rapidOBJ import path.
- Add OBJ round-trip tests and performance tests.
