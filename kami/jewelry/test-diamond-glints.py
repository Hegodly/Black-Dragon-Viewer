#!/usr/bin/env python3
"""Compile the exact authored diamond glint function with glslangValidator."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
effect = (root / "kami/jewelry/diamond-glint.glsl").read_text(encoding="utf-8")
if effect.count("vec3 kamiDiamondGlints(") != 1:
    raise AssertionError("KAMI shader snippet missing or duplicated")
source = """#version 330 core
uniform float kami_diamond_glint_strength;
uniform float kami_diamond_glint_fire;
layout(location = 0) out vec4 frag_color;
""" + effect + """
void main() {
    frag_color = vec4(kamiDiamondGlints(
        vec3(0, 0, 1), vec3(0, 0, 1), vec3(0, 0, 1),
        vec3(1), 0.08, 0.0, 2.0, vec3(1), 1.0), 1.0);
}
"""
with tempfile.TemporaryDirectory(prefix="kami-glints-") as td:
    frag = Path(td) / "glints.frag"
    frag.write_text(source, encoding="utf-8")
    result = subprocess.run(["glslangValidator", "-S", "frag", str(frag)],
                            text=True, capture_output=True, check=False)
    if result.returncode:
        raise RuntimeError(result.stdout + "\n" + result.stderr)
print("PASS native diamond GLSL 330 fragment syntax")
