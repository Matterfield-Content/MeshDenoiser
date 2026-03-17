import argparse
import csv
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def load_manifest(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def run_case(case, repo_root: Path, build_dir: Path, out_dir: Path):
    mode = case.get("mode", "denoise")
    if mode != "denoise":
        raise RuntimeError(f"Unsupported benchmark mode '{mode}'. This repository now ships denoise-only.")

    exe = build_dir / "MeshDenoiser.exe"
    if not exe.exists():
        raise RuntimeError(f"Executable not found: {exe}")

    input_mesh = repo_root / case["input_mesh"]
    options = repo_root / case["options_file"]
    required = bool(case.get("required", False))
    expect_failure = bool(case.get("expect_failure", False))
    if not input_mesh.exists():
        if required:
            raise RuntimeError(f"Required benchmark mesh missing: {input_mesh}")
        return None

    if not options.exists():
        raise RuntimeError(f"Options file missing: {options}")

    output_mesh = out_dir / f"{case['name']}_output.obj"
    metrics_json = out_dir / f"{case['name']}_metrics.json"

    cmd = [
        str(exe),
        str(options),
        str(input_mesh),
        str(output_mesh),
        "--obj-export-precision",
        "16",
        "--metrics-json",
        str(metrics_json),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        if expect_failure:
            return {
                "name": case["name"],
                "category": case["category"],
                "topology": case["topology"],
                "mode": mode,
                "input_mesh": case["input_mesh"],
                "output_mesh": "",
                "status": "expected_failure",
                "timing": {},
                "solver": {},
                "stdout": proc.stdout,
                "stderr": proc.stderr,
            }
        raise RuntimeError(
            f"Case {case['name']} failed with code {proc.returncode}\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )

    if not metrics_json.exists():
        raise RuntimeError(f"Metrics JSON missing for case {case['name']}: {metrics_json}")

    with metrics_json.open("r", encoding="utf-8") as f:
        metrics = json.load(f)

    result = {
        "name": case["name"],
        "category": case["category"],
        "topology": case["topology"],
        "mode": mode,
        "input_mesh": case["input_mesh"],
        "output_mesh": str(output_mesh.relative_to(repo_root)),
        "status": "success",
        "timing": metrics["timing"],
        "solver": metrics["solver"],
    }
    return result


def write_outputs(results, out_dir: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    json_path = out_dir / "benchmark_results.json"
    csv_path = out_dir / "benchmark_results.csv"

    with json_path.open("w", encoding="utf-8") as f:
        json.dump({"results": results}, f, indent=2)

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(
            [
                "name",
                "category",
                "topology",
                "mode",
                "status",
                "import_secs",
                "normalize_secs",
                "algorithm_secs",
                "restore_secs",
                "export_secs",
                "total_secs",
                "preprocessing_secs",
                "filtering_secs",
                "mesh_update_secs",
                "mesh_filter_total_secs",
                "denoise_total_secs",
                "solver_iterations",
                "solver_converged",
                "outer_iterations",
            ]
        )
        for r in results:
            t = r["timing"]
            s = r["solver"]
            writer.writerow(
                [
                    r["name"],
                    r["category"],
                    r["topology"],
                    r["mode"],
                    r.get("status", "success"),
                    t.get("import_secs", ""),
                    t.get("normalize_secs", ""),
                    t.get("algorithm_secs", ""),
                    t.get("restore_secs", ""),
                    t.get("export_secs", ""),
                    t.get("total_secs", ""),
                    t.get("preprocessing_secs", ""),
                    t.get("filtering_secs", ""),
                    t.get("mesh_update_secs", ""),
                    t.get("mesh_filter_total_secs", ""),
                    t.get("denoise_total_secs", ""),
                    s.get("iterations", ""),
                    int(bool(s["converged"])) if "converged" in s else "",
                    s.get("outer_iterations", ""),
                ]
            )

    return json_path, csv_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="benchmarks/benchmark_manifest.json")
    parser.add_argument("--build-dir", default="build-vcpkg/Release")
    parser.add_argument("--out-dir", default="benchmarks/results")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    manifest = load_manifest(repo_root / args.manifest)
    build_dir = repo_root / args.build_dir
    out_dir = repo_root / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    results = []
    for case in manifest["cases"]:
        result = run_case(case, repo_root, build_dir, out_dir)
        if result is not None:
            results.append(result)

    json_path, csv_path = write_outputs(results, out_dir)
    print(f"Benchmark results written to {json_path}")
    print(f"Benchmark results written to {csv_path}")


if __name__ == "__main__":
    main()
