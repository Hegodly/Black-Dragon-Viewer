#!/usr/bin/env python3
"""GLSL 330 standalone smoke validation for the native PBR quality functions."""
import subprocess
import tempfile
from pathlib import Path

source = Path(__file__).with_name("pbr-fidelity.glsl").read_text(encoding="utf-8")
assert source.count("float kamiQualityRoughnessFloor(") == 1
assert source.count("float kamiQualityProbeWeight(") == 1
shader = """#version 330 core
layout(location=0) out vec4 frag_color;
""" + source + """
void main() {
  float q = 3.0;
  frag_color = vec4(kamiQualityRoughnessFloor(q),
                    kamiQualityProbeWeight(q), 0.0, 1.0);
}
"""
with tempfile.TemporaryDirectory(prefix="kami-preset-") as directory:
    filepath = Path(directory) / "presets.frag"
    filepath.write_text(shader, encoding="utf-8")
    run = subprocess.run(["glslangValidator", "-S", "frag", str(filepath)],
                         capture_output=True, text=True)
    if run.returncode:
        raise AssertionError(run.stdout + "\n" + run.stderr)
print("PASS five native PBR material fidelity tiers compile in GLSL 330")
