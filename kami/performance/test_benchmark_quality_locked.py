#!/usr/bin/env python3
"""Contract tests for quality-preserving KAMI FPS comparisons."""
import csv
import json
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from benchmark_quality_locked import percentile, read_run, metrics, compare

SETTINGS = {
    "scene_id": "camera-and-world-hash-A",
    "graphics_settings": {
        "RenderShadowDetail": 2,
        "RenderDeferredSSAO": 1,
        "RenderScreenSpaceReflections": 1,
        "RenderMaxTextureResolution": 4096,
        "KombatantRenderWidth": 2560,
        "KAMIQualityPreset": 3
    },
    "render_width": 2560,
    "render_height": 1440,
    "display_width": 2560,
    "display_height": 1440,
    "hardware_id": "RTX5070-12GB",
    "driver_version": "record-before-test",
    "camera_id": "frozen-camera-1",
    "capture_method": "actual-rendered-frames",
    "frame_generation": False,
}

class Tests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name)

    def fixture(self, name, frame_ms, meta=SETTINGS):
        csv_path = self.path / (name+".csv")
        meta_path = self.path / (name+".json")
        with csv_path.open("w",newline="",encoding="utf-8") as fp:
            writer = csv.DictWriter(fp,fieldnames=["frame_ms","cpu_ms","gpu_ms"])
            writer.writeheader()
            for v in frame_ms:
                writer.writerow({"frame_ms":v,"cpu_ms":v*0.55,"gpu_ms":v*0.85})
        meta_path.write_text(json.dumps(meta),encoding="utf-8")
        return read_run(csv_path,meta_path)

    def test_frame_time_percentiles_and_quality_locked_gain(self):
        a,fa = self.fixture("base",[20.0]*1000)
        b,fb = self.fixture("fast",[10.0]*1000)
        m=metrics(fa)
        self.assertEqual(m["median_frame_ms"],20)
        self.assertEqual(m["mean_fps"],50)
        self.assertEqual(m["one_percent_low_fps"],50)
        report=compare(a,fa,b,fb)
        self.assertEqual(report["changes"]["median_frame_ms_change_pct"],-50)
        self.assertEqual(report["changes"]["p99_frame_ms_change_pct"],-50)
        self.assertTrue(report["quality_settings_identical"])

    def test_strict_quality_rejection(self):
        a,fa=self.fixture("base",[20.0]*100)
        for changed in (
            {"render_width":1920},
            {"display_height":1080},
            {"scene_id":"different-crowd"},
            {"graphics_settings":{"RenderShadowDetail":0}},
            {"frame_generation":True},
            {"hardware_id":"integrated-graphics"}
        ):
            meta=dict(SETTINGS,**changed)
            b,fb=self.fixture("test",[5.0]*100,meta)
            with self.assertRaises(ValueError):
                compare(a,fa,b,fb)

    def test_invalid_zero_negative_nan_and_short_data(self):
        for name,frames in (
            ("too-short",[20.0]*99),
            ("zero",[0.0]*100),
            ("negative",[-1.0]*100),
            ("nan",[float("nan")]*100),
        ):
            path=self.path/(name+".csv")
            metadata=self.path/(name+".json")
            metadata.write_text(json.dumps(SETTINGS),encoding="utf-8")
            with path.open("w",newline="",encoding="utf-8") as out:
                w=csv.writer(out)
                w.writerow(["frame_ms"])
                for value in frames: w.writerow([value])
            with self.assertRaises(ValueError):
                read_run(path,metadata)

    def test_frame_tail_changes_one_percent_low(self):
        data={"frame_ms":[10.0]*990+[100.0]*10,"cpu_ms":[],"gpu_ms":[]}
        m=metrics(data)
        self.assertAlmostEqual(m["one_percent_low_fps"],10)
        self.assertGreater(m["p99_frame_ms"],10)
        self.assertEqual(m["frames_over_2x_median"],10)

    def test_interpolated_percentile(self):
        self.assertEqual(percentile([10,20,30,40,50],50),30)
        self.assertEqual(percentile([10,20,30,40,50],95),48)

if __name__ == "__main__":
    unittest.main()
