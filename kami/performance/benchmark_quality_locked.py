#!/usr/bin/env python3
"""KAMI quality-locked FPS benchmark: compare measured frame times, not presets.

CSV columns: frame_ms; optional cpu_ms, gpu_ms.
Metadata JSON: scene_id, graphics_settings (object), render_width,
render_height, and display_width/display_height. Comparison is REFUSED if
the scene or graphical settings differ, including resolution.
No fabricated measurements, GPU percentages, frame generation or downscaling.
"""
import argparse
import csv
import json
import math
import statistics
from pathlib import Path

REQUIRED_METADATA = ("scene_id", "graphics_settings", "render_width",
                     "render_height", "display_width", "display_height")
METADATA_EXT = ".json"

def percentile(data, p):
    """Linearly interpolated percentile of sorted frame times."""
    if not data:
        raise ValueError("Cannot compute percentile without frames")
    ordered = sorted(data)
    position = (len(ordered) - 1) * p / 100
    low = int(math.floor(position))
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] * (1 - (position - low)) + ordered[high] * (position - low)

def read_run(csv_path, meta_path):
    path, meta = Path(csv_path), Path(meta_path)
    with meta.open(encoding="utf-8") as file:
        info = json.load(file)
    missing = [name for name in REQUIRED_METADATA if name not in info]
    if missing:
        raise ValueError(f"{meta}: missing required fields {missing}")
    if not isinstance(info["graphics_settings"], dict):
        raise ValueError("graphics_settings must be a JSON object")
    for name in REQUIRED_METADATA[2:]:
        if not isinstance(info[name], int) or isinstance(info[name], bool) or info[name] <= 0:
            raise ValueError(f"{meta}: {name} must be a positive integer")
    results = {"frame_ms": [], "cpu_ms": [], "gpu_ms": []}
    with path.open(newline="", encoding="utf-8-sig") as file:
        reader = csv.DictReader(file)
        if not reader.fieldnames or "frame_ms" not in reader.fieldnames:
            raise ValueError(f"{path}: CSV requires a frame_ms header")
        for i, row in enumerate(reader, start=2):
            for key in results:
                raw = row.get(key)
                if raw is None or raw.strip() == "":
                    if key == "frame_ms":
                        raise ValueError(f"{path}:{i}: missing frame_ms")
                    continue
                try:
                    v = float(raw)
                except (ValueError, TypeError) as e:
                    raise ValueError(f"{path}:{i}: bad {key} value {raw!r}") from e
                if not math.isfinite(v) or v <= 0:
                    raise ValueError(f"{path}:{i}: {key} must be positive and finite")
                results[key].append(v)
    if len(results["frame_ms"]) < 100:
        raise ValueError(f"{path}: need at least 100 real frame samples")
    if any(results[k] and len(results[k]) != len(results["frame_ms"]) for k in ("cpu_ms", "gpu_ms")):
        raise ValueError(f"{path}: CPU/GPU timings must be present for all frames or none")
    return info, results

def metrics(run):
    frames = run["frame_ms"]
    median = statistics.median(frames)
    slow_count = max(1, math.ceil(len(frames) * 0.01))
    worst = sorted(frames, reverse=True)[:slow_count]
    out = {
        "samples": len(frames),
        "median_frame_ms": round(median, 3),
        "p95_frame_ms": round(percentile(frames, 95), 3),
        "p99_frame_ms": round(percentile(frames, 99), 3),
        "mean_fps": round(1000 * len(frames) / sum(frames), 2),
        "one_percent_low_fps": round(1000 / statistics.mean(worst), 2),
        "frames_over_2x_median": sum(v > 2 * median for v in frames),
    }
    if run["cpu_ms"]:
        out["median_cpu_ms"] = round(statistics.median(run["cpu_ms"]), 3)
    if run["gpu_ms"]:
        out["median_gpu_ms"] = round(statistics.median(run["gpu_ms"]), 3)
    if run["cpu_ms"] and run["gpu_ms"]:
        cp = statistics.median(run["cpu_ms"])
        gp = statistics.median(run["gpu_ms"])
        # CPU and GPU overlap; this is a diagnostic HINT, not a sum.
        out["likely_bound"] = ("CPU" if cp > gp * 1.2 else
                               "GPU" if gp > cp * 1.2 else "Mixed / inconclusive")
    return out

def compare(base_meta, base_run, new_meta, new_run):
    differing = [k for k in REQUIRED_METADATA if base_meta[k] != new_meta[k]]
    if differing:
        raise ValueError("Refusing invalid same-quality FPS comparison: " +
                         ", ".join(differing) + " differs. " +
                         "Do not claim an efficiency gain by reducing quality or scene complexity.")
    # Optional parameters can affect benchmark validity too: recording method,
    # runtime hardware, capture/camera state, viewing distance, or software.
    for key in ("hardware_id", "driver_version", "camera_id", "capture_method",
                "scene_object_count", "frame_generation"):
        if base_meta.get(key) != new_meta.get(key):
            differing.append(key)
    if differing:
        raise ValueError("Refusing uncontrolled comparison: " + ", ".join(differing))
    a = metrics(base_run)
    b = metrics(new_run)
    lower_is_better = ("median_frame_ms", "p95_frame_ms", "p99_frame_ms")
    delta = {name+"_change_pct": round((b[name]-a[name])*100/a[name], 2)
             for name in lower_is_better if a[name]}
    return {"quality_settings_identical": True, "baseline": a,
            "candidate": b, "changes": delta,
            "note": "Negative frame-time change means faster. Similar image quality "
                    "must also be verified visually (A/B PNG comparison). "
                    "Frame-gen FPS is not an acceptable optimization metric."}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, help="Baseline CSV with frame_ms")
    parser.add_argument("--baseline-meta", required=True, help="Baseline JSON settings")
    parser.add_argument("--candidate", required=True, help="Candidate CSV with frame_ms")
    parser.add_argument("--candidate-meta", required=True, help="Candidate JSON settings")
    parser.add_argument("--out", help="Optional output JSON report path")
    args=parser.parse_args()
    try:
        a_info,a=read_run(args.baseline,args.baseline_meta)
        b_info,b=read_run(args.candidate,args.candidate_meta)
        report=compare(a_info,a,b_info,b)
    except (ValueError, OSError, json.JSONDecodeError) as exc:
        parser.exit(2, f"Benchmark rejected: {exc}\n")
    text=json.dumps(report, indent=2)+"\n"
    if args.out:
        Path(args.out).write_text(text,encoding="utf-8")
    print(text,end="")

if __name__ == "__main__":
    main()
