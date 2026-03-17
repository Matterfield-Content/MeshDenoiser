import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        while True:
            block = f.read(1024 * 1024)
            if not block:
                break
            h.update(block)
    return h.hexdigest()


def run_once(exe: Path, options: Path, input_mesh: Path, output_mesh: Path, metrics_json: Path):
    cmd = [
        str(exe),
        str(options),
        str(input_mesh),
        str(output_mesh),
        "--obj-export-precision",
        "16",
        "--metrics-json",
        str(metrics_json),
        "--deterministic",
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(
            f"Determinism run failed ({output_mesh.name})\nSTDOUT:\n{proc.stdout}\nSTDERR:\n{proc.stderr}"
        )


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", default="build-vcpkg/Release")
    parser.add_argument("--options", default="DenoisingOptions.txt")
    parser.add_argument("--input", default="TestData/small_manifold.obj")
    parser.add_argument("--out-dir", default="benchmarks/results")
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[1]
    exe = repo / args.build_dir / "MeshDenoiser.exe"
    options = repo / args.options
    input_mesh = repo / args.input
    out_dir = repo / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    out1 = out_dir / "determinism_denoise_run1.obj"
    out2 = out_dir / "determinism_denoise_run2.obj"
    m1 = out_dir / "determinism_denoise_run1.json"
    m2 = out_dir / "determinism_denoise_run2.json"

    run_once(exe, options, input_mesh, out1, m1)
    run_once(exe, options, input_mesh, out2, m2)

    h1 = sha256_file(out1)
    h2 = sha256_file(out2)
    if h1 != h2:
        raise SystemExit(f"Determinism check FAILED: hash mismatch\nrun1={h1}\nrun2={h2}")

    j1 = load_json(m1)
    j2 = load_json(m2)
    s1 = j1.get("solver", {})
    s2 = j2.get("solver", {})
    if s1.get("iterations") != s2.get("iterations") or s1.get("converged") != s2.get("converged"):
        raise SystemExit(f"Determinism check FAILED: solver stats mismatch\nrun1={s1}\nrun2={s2}")

    print(f"Determinism OK: output hash={h1}")


if __name__ == "__main__":
    main()
