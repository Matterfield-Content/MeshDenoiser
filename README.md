MeshDenoiser -- Static/Dynamic Filtering for Mesh Geometry
==========================================================

This repository currently ships a denoiser-focused application built from the
Static/Dynamic mesh filtering method described in:

- Juyong Zhang, Bailin Deng, Yang Hong, Yue Peng, Wenjie Qin, Ligang Liu.
  Static/Dynamic Filtering for Mesh Geometry. arXiv:1712.03574.


1. Build

Current default build target:

- `MeshDenoiser`

Windows release build and packaging:

```powershell
powershell -ExecutionPolicy Bypass -File .\build_release.ps1
```

That script:

- configures CMake
- builds `MeshDenoiser.exe`
- packages the executable, runtime DLLs, options, and docs into `dist\release`

Manual Visual Studio CMake build:

```powershell
cmake -S . -B build-vcpkg -G "Visual Studio 17 2022" -A x64
cmake --build build-vcpkg --config Release --target MeshDenoiser
```

Dependency notes:

- CHOLMOD acceleration is enabled when CMake finds SuiteSparse/CHOLMOD.
- If CHOLMOD is unavailable, the application falls back to CPU LDLT.


2. Usage

```text
MeshDenoiser INPUT_MESH OUTPUT_MESH
MeshDenoiser OPTION_FILE INPUT_MESH OUTPUT_MESH
```

If `OPTION_FILE` is omitted, the executable uses the same built-in defaults that
`--write-default-options` writes to disk.

Optional CLI flags:

- `--write-default-options PATH`
- `--obj-export-precision N`
- `--metrics-json PATH`
- `--metrics-csv PATH`
- `--deterministic`

Option-file notes:

- Use `DenoisingOptions.txt` as the default template.
- Or generate a fresh copy with:

```powershell
MeshDenoiser.exe --write-default-options my_options.txt
```

- Supported solver/runtime tuning includes:
  - `DeterministicMode`
  - `LinearSolverType`
  - `LinearSolverMaxIterations`
  - `LinearSolverTolerance`
  - `MeshUpdateDisplacementEps`

OBJ I/O notes:

- Input/output is OBJ-only.
- Import uses rapidOBJ.
- Export writes ASCII OBJ.
- Non-triangle faces are triangulated on import.
- Malformed faces and non-finite coordinates fail with explicit errors or warnings.


3. Benchmarking

Run the benchmark harness from repo root:

```powershell
python tools/benchmark_runner.py --manifest benchmarks/benchmark_manifest.json --build-dir build-vcpkg/Release --out-dir benchmarks/results
```

Run the determinism check:

```powershell
python tools/test_determinism.py --build-dir build-vcpkg/Release --options DenoisingOptions.txt --input TestData/noisy.obj --out-dir benchmarks/results
```

Run the smoke test:

```powershell
python tools/perf_smoke.py
```


4. Additional Docs

- `TUNING_GUIDE.md`
- `MIGRATION_GUIDE.md`
- `OPTIMIZATION_ROADMAP.md`


5. License

The project code is released under the BSD 3-Clause License.
