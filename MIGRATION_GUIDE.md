# Migration Guide

## Runtime I/O Changes
- Legacy mixed-format runtime loading is removed.
- Runtime now supports OBJ-only:
  - Import: rapidOBJ
  - Export: ASCII OBJ writer

## CLI Changes
- Current shipped CLI:
  - `MeshDenoiser OPTION_FILE INPUT_MESH OUTPUT_MESH`
  - `MeshDenoiser --write-default-options PATH`
- Supported optional flags:
  - `--obj-export-precision N`
  - `--metrics-json PATH`
  - `--metrics-csv PATH`
  - `--deterministic`

## Option File Changes
- Added:
  - `DeterministicMode` (`0`/`1`)
- Existing core SD parameters remain unchanged.

## Behavior Changes
- Non-triangle faces are triangulated on import.
- Invalid/malformed faces are skipped with warnings.
- Oversized index/vertex ranges and non-finite coordinates fail fast with explicit errors.

## Product Scope Changes
- The repository now builds and packages `MeshDenoiser` as the primary application.
- Legacy auxiliary executables used during migration work are no longer part of the default product flow.
- CUDA acceleration was removed after benchmark validation showed no net end-to-end speedup for the current production path.
