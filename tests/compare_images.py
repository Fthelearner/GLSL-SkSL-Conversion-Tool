#!/usr/bin/env python3
"""Compare two PNG images pixel-by-pixel with configurable thresholds."""

import argparse
import json
import math
import sys
from pathlib import Path

from PIL import Image


def compute_metrics(img_a: Image.Image, img_b: Image.Image) -> dict:
    px_a = list(img_a.getdata())
    px_b = list(img_b.getdata())

    if len(px_a) != len(px_b):
        raise ValueError(f"Pixel count mismatch: {len(px_a)} vs {len(px_b)}")

    total_pixels = len(px_a)
    channels = len(px_a[0]) if px_a else 1
    sse = 0
    max_diff = 0
    diff_count = 0

    for pa, pb in zip(px_a, px_b):
        pixel_differs = False
        for ca, cb in zip(pa, pb):
            d = abs(int(ca) - int(cb))
            sse += d * d
            if d > max_diff:
                max_diff = d
            if d > 0:
                pixel_differs = True
        if pixel_differs:
            diff_count += 1

    mse = sse / (total_pixels * channels)
    psnr = (20.0 * math.log10(255.0 / math.sqrt(mse))) if mse > 0 else float("inf")

    return {
        "mse": round(mse, 4),
        "psnr": round(psnr, 2),
        "max_pixel_diff": max_diff,
        "pixel_diff_percent": round(100.0 * diff_count / total_pixels, 2),
        "total_pixels": total_pixels,
        "different_pixels": diff_count,
    }


def main():
    parser = argparse.ArgumentParser(description="Compare two PNG images.")
    parser.add_argument("image_a", help="First PNG path")
    parser.add_argument("image_b", help="Second PNG path")
    parser.add_argument("--threshold-psnr", type=float, default=30.0,
                        help="Minimum PSNR to pass (default: 30.0)")
    parser.add_argument("--threshold-diff-percent", type=float, default=5.0,
                        help="Maximum pixel diff %% to pass (default: 5.0)")
    parser.add_argument("--json", action="store_true",
                        help="Output results as JSON to stdout")
    parser.add_argument("--output-json", help="Write JSON report to file")
    args = parser.parse_args()

    if not Path(args.image_a).exists():
        print(f"ERROR: '{args.image_a}' not found", file=sys.stderr)
        return 2

    if not Path(args.image_b).exists():
        print(f"ERROR: '{args.image_b}' not found", file=sys.stderr)
        return 2

    try:
        img_a = Image.open(args.image_a).convert("RGBA")
        img_b = Image.open(args.image_b).convert("RGBA")
    except Exception as e:
        print(f"ERROR: Cannot open images: {e}", file=sys.stderr)
        return 2

    if img_a.size != img_b.size:
        print(f"WARNING: Image sizes differ — {img_a.size} vs {img_b.size}. Resizing.",
              file=sys.stderr)
        img_b = img_b.resize(img_a.size, Image.LANCZOS)

    metrics = compute_metrics(img_a, img_b)
    passed = (
        metrics["psnr"] >= args.threshold_psnr
        and metrics["pixel_diff_percent"] <= args.threshold_diff_percent
    )
    metrics["passed"] = passed
    metrics["threshold_psnr"] = args.threshold_psnr
    metrics["threshold_diff_percent"] = args.threshold_diff_percent

    if args.output_json:
        Path(args.output_json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.output_json).write_text(json.dumps(metrics, indent=2))

    if args.json:
        print(json.dumps(metrics, indent=2))
    else:
        status = "PASS" if passed else "FAIL"
        print(f"{status}: PSNR={metrics['psnr']} dB, "
              f"diff={metrics['pixel_diff_percent']}%, "
              f"max_delta={metrics['max_pixel_diff']}")

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
