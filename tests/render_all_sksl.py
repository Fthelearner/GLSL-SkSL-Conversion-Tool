#!/usr/bin/env python3
"""Render all SKSL files to results/sksl_render using their .params.json files."""

import json
import os
import subprocess
import sys
from pathlib import Path

SHADERS_DIR = Path(__file__).resolve().parent / "shaders"
RESULTS_DIR = Path(__file__).resolve().parent.parent / "results" / "sksl_render"
RENDER_SCRIPT = Path(__file__).resolve().parent / "render_sksl.py"

def has_subshader_children(params):
    """Check if any texture is marked as a child shader (not a real texture)."""
    textures = params.get("textures", {})
    for name, val in textures.items():
        if isinstance(val, dict) and val.get("childShader"):
            return True
    return False

def resolve_asset(rel_path):
    """Resolve an asset path relative to the tests directory."""
    tests_dir = SHADERS_DIR.parent
    candidates = [
        tests_dir / rel_path,
        tests_dir / "assets" / Path(rel_path).name,
        SHADERS_DIR / rel_path,
    ]
    for c in candidates:
        if c.exists():
            return c
    return None

def get_texture_args(params):
    """Generate --texture arguments from params."""
    args = []
    textures = params.get("textures", {})
    for name, val in textures.items():
        if isinstance(val, str):
            full_path = resolve_asset(val)
            if full_path:
                args.extend(["--texture", f"{name}={full_path}"])
            else:
                print(f"  WARNING: Texture not found: {val} (for {name})")
        elif isinstance(val, dict):
            if val.get("raw"):
                path = val["path"]
                full_path = resolve_asset(path)
                if full_path:
                    args.extend(["--texture", f"{name}={full_path}"])
                    args.extend(["--raw", name])
    return args

def get_uniform_args(params):
    """Generate --uniform arguments from params."""
    args = []
    uniforms = params.get("uniforms", {})
    for name, val in uniforms.items():
        if isinstance(val, list):
            str_val = ",".join(str(v) for v in val)
        else:
            str_val = str(val)
        args.extend(["--uniform", f"{name}={str_val}"])
    return args

def main():
    results = {"rendered": [], "skipped": [], "failed": []}
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)

    categories = ['filter', 'mask', 'shader', 'shape']

    for category in categories:
        cat_dir = SHADERS_DIR / category
        if not cat_dir.exists():
            continue

        for folder in sorted(cat_dir.iterdir()):
            if not folder.is_dir():
                continue

            for sksl_file in sorted(folder.glob("*.sksl")):
                params_file = sksl_file.with_suffix(".params.json")
                if not params_file.exists():
                    print(f"SKIP (no params): {sksl_file.relative_to(SHADERS_DIR)}")
                    results["skipped"].append(str(sksl_file.relative_to(SHADERS_DIR)))
                    continue

                with open(params_file) as f:
                    params = json.load(f)

                if has_subshader_children(params):
                    print(f"SKIP (sub-shader children): {sksl_file.relative_to(SHADERS_DIR)}")
                    results["skipped"].append(str(sksl_file.relative_to(SHADERS_DIR)))
                    continue

                # Determine output path
                rel = sksl_file.relative_to(SHADERS_DIR)
                out_dir = RESULTS_DIR / rel.parent
                out_dir.mkdir(parents=True, exist_ok=True)
                out_file = out_dir / f"{sksl_file.stem}.png"

                # Build command
                dims = params.get("dimensions", {"width": 1280, "height": 720})
                cmd = [
                    sys.executable, str(RENDER_SCRIPT),
                    "--sksl", str(sksl_file),
                    "--output", str(out_file),
                    "--width", str(dims["width"]),
                    "--height", str(dims["height"]),
                ]
                cmd.extend(get_texture_args(params))
                cmd.extend(get_uniform_args(params))

                print(f"RENDER: {rel}")
                try:
                    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                    if result.returncode == 0:
                        print(f"  OK -> {out_file}")
                        results["rendered"].append(str(rel))
                    else:
                        print(f"  FAILED: {result.stderr.strip()}")
                        results["failed"].append({"file": str(rel), "error": result.stderr.strip()})
                except subprocess.TimeoutExpired:
                    print(f"  TIMEOUT: {rel}")
                    results["failed"].append({"file": str(rel), "error": "timeout"})
                except Exception as e:
                    print(f"  ERROR: {rel} - {e}")
                    results["failed"].append({"file": str(rel), "error": str(e)})

    # Summary
    print(f"\n=== SUMMARY ===")
    print(f"Rendered: {len(results['rendered'])}")
    print(f"Skipped (sub-shader): {len(results['skipped'])}")
    print(f"Failed: {len(results['failed'])}")

    if results['failed']:
        print("\nFailed files:")
        for f in results['failed']:
            print(f"  {f['file']}: {f['error'][:100]}")

    # Save report
    report_path = RESULTS_DIR / "render_report.json"
    with open(report_path, 'w') as f:
        json.dump(results, f, indent=2)
    print(f"\nReport saved to {report_path}")

if __name__ == "__main__":
    main()
