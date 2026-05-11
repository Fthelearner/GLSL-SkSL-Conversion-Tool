#!/usr/bin/env python3
"""Display PNG frames as ASCII art animation in the terminal.

Watches a directory for new frame PNGs and displays them as ASCII art,
clearing the screen between frames for a smooth animation effect.
"""

import argparse
import os
import sys
import time
from pathlib import Path

from PIL import Image

CHARS = " .:-=+*#%@"  # dark to bright


def image_to_ascii(img: Image.Image, cols: int, rows: int) -> str:
    """Convert a PIL Image to an ASCII string with given terminal dimensions."""
    img = img.resize((cols, rows), Image.LANCZOS).convert("L")
    pixels = list(img.getdata())
    lines = []
    for y in range(rows):
        line = []
        for x in range(cols):
            brightness = pixels[y * cols + x]
            idx = int(brightness / 255.0 * (len(CHARS) - 1))
            line.append(CHARS[idx])
        lines.append("".join(line))
    return "\n".join(lines)


def get_terminal_size():
    try:
        return os.get_terminal_size()
    except (OSError, ValueError):
        return os.terminal_size((80, 24))


def main():
    parser = argparse.ArgumentParser(
        description="Display PNG frames as ASCII art in the terminal."
    )
    parser.add_argument("--frame-dir", required=True, help="Directory containing frame PNGs")
    parser.add_argument("--fps", type=int, default=10, help="Display refresh rate (default: 10)")
    parser.add_argument("--cols", type=int, default=None, help="Terminal columns (auto-detect)")
    parser.add_argument("--rows", type=int, default=None, help="Terminal rows (auto-detect)")
    parser.add_argument("--wait", action="store_true",
                        help="Wait for frames to appear (polling mode)")
    parser.add_argument("--once", action="store_true",
                        help="Display existing frames once and exit")
    args = parser.parse_args()

    frame_dir = Path(args.frame_dir)
    if not frame_dir.is_dir():
        print(f"ERROR: directory not found: {frame_dir}", file=sys.stderr)
        return 1

    term = get_terminal_size()
    cols = args.cols or term.columns
    rows = (args.rows or term.lines) - 2  # reserve 2 lines for status bar

    if cols < 20 or rows < 5:
        print("ERROR: terminal too small for ASCII animation", file=sys.stderr)
        return 1

    frame_interval = 1.0 / args.fps
    displayed = set()
    total_frames = 0

    # Clear screen and hide cursor
    print("\033[2J\033[H\033[?25l", end="", flush=True)

    try:
        while True:
            # Collect available frames
            pngs = sorted(frame_dir.glob("frame_*.png"))
            marker = frame_dir / "render_done.marker"
            done = marker.exists()

            if not pngs and not done and args.wait:
                time.sleep(frame_interval / 2)
                continue

            new_frames = [p for p in pngs if p not in displayed]
            total_frames = max(total_frames, len(pngs))

            for fp in new_frames:
                try:
                    img = Image.open(fp)
                    ascii_art = image_to_ascii(img, cols, rows)
                    # Move cursor home, print frame, print status bar
                    status = f" Frame {len(displayed)+1}"
                    if done:
                        status += f" / {total_frames} [DONE]"
                    else:
                        status += f" (rendering... {total_frames} frames so far)"
                    status = status.ljust(cols)
                    print(f"\033[H{ascii_art}\n\033[7m{status}\033[0m", end="", flush=True)
                    displayed.add(fp)
                except Exception as e:
                    print(f"\033[HERROR: {e}\033[K", flush=True)

            if done and len(displayed) >= total_frames:
                if args.once:
                    break
                time.sleep(1.5)  # brief pause to admire the final frame
                break

            if not args.wait and len(displayed) >= total_frames and total_frames > 0:
                break

            time.sleep(frame_interval / 2)

    except KeyboardInterrupt:
        pass
    finally:
        # Show cursor and move to bottom
        print(f"\033[?25h\033[{rows + 2}H", flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
