# Benchmark Harness

This folder contains the denoiser benchmark harness and retained benchmark outputs.

## Files
- `benchmark_manifest.json`: canonical mesh corpus metadata.
- `results/`: generated CSV/JSON benchmark reports.

## Run
From repo root:

```powershell
python tools/benchmark_runner.py --manifest benchmarks/benchmark_manifest.json --build-dir build-vcpkg/Release --out-dir benchmarks/results
```

Outputs:
- `benchmarks/results/benchmark_results.json`
- `benchmarks/results/benchmark_results.csv`

Notes:
- Required cases must exist or the run fails.
- Optional cases are skipped if missing.
- The current harness is denoise-only and targets `MeshDenoiser.exe`.
