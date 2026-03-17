import json
import subprocess
from pathlib import Path


def main():
    repo = Path(__file__).resolve().parents[1]
    exe = repo / "build-vcpkg/Release/MeshDenoiser.exe"
    options = repo / "DenoisingOptions.txt"
    input_mesh = repo / "TestData/small_manifold.obj"
    output_mesh = repo / "TestData/small_manifold_smoke_output.obj"
    metrics = repo / "benchmarks/results/smoke_metrics.json"
    metrics.parent.mkdir(parents=True, exist_ok=True)

    cmd = [
        str(exe),
        str(options),
        str(input_mesh),
        str(output_mesh),
        "--metrics-json",
        str(metrics),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise SystemExit(f"Perf smoke run failed\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}")

    if not metrics.exists():
        raise SystemExit("Perf smoke failed: metrics json not written")

    with metrics.open("r", encoding="utf-8") as f:
        data = json.load(f)

    total_secs = float(data["timing"]["total_secs"])
    if total_secs > 60.0:
        raise SystemExit(f"Perf smoke failed: total_secs too high ({total_secs})")

    print(f"Perf smoke OK: total_secs={total_secs:.3f}")


if __name__ == "__main__":
    main()
