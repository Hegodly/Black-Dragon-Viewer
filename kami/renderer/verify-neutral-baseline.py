#!/usr/bin/env python3
"""Static safety checks for KAMI's neutral, FPS-conscious base renderer.

Run against checked-out sources before and after native KAMI patch scripts.
This does not replace shader compilation or measured in-world FPS tests.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
SETTINGS = ROOT / "indra/newview/app_settings/settings.xml"
BD_SETTINGS = ROOT / "indra/newview/app_settings/settings_blackdragon.xml"
TONEMAP = ROOT / "indra/newview/app_settings/shaders/class1/deferred/tonemapUtilF.glsl"


def setting(path: Path, key: str):
    root = ET.parse(path).getroot()
    entries = list(root.find("map"))
    for i, item in enumerate(entries[:-1]):
        if item.tag != "key" or item.text != key:
            continue
        value_map = entries[i + 1]
        assert value_map.tag == "map", f"{key}: expected map"
        fields = list(value_map)
        for j, field in enumerate(fields[:-1]):
            if field.tag == "key" and field.text == "Value":
                value = fields[j + 1]
                return value.text.strip() if value.text else ""
        raise AssertionError(f"{key}: missing Value")
    raise AssertionError(f"{key}: setting missing in {path.name}")


def expect(path: Path, key: str, expected: str):
    actual = setting(path, key)
    assert actual == expected, f"{path.name} / {key}: expected {expected}, got {actual}"
    print(f"PASS {key} = {actual}")


def main():
    # Base image: PBR is left intact. No artificial look or extra post passes.
    expect(SETTINGS, "RenderTonemapType", "0")  # Khronos Neutral
    expect(SETTINGS, "RenderTonemapMix", "1.0")
    expect(SETTINGS, "RenderGlow", "0")
    expect(SETTINGS, "RenderDeferredSSAO", "0")
    expect(SETTINGS, "RenderScreenSpaceReflections", "0")
    expect(SETTINGS, "RenderDepthOfField", "0")
    expect(SETTINGS, "RenderMotionBlur", "0")

    # Keep core quality and PBR probes instead of disabling base shaders.
    expect(SETTINGS, "RenderReflectionProbeCount", "128")
    expect(SETTINGS, "RenderFSAAType", "2")  # Existing SMAA, not broken FXAA

    # Use display-native resolution. 1440p is a goal, not a forced minimum;
    # never re-enable experimental automatic world-buffer downscaling.
    expect(SETTINGS, "RenderResolutionDivisor", "1")
    expect(BD_SETTINGS, "RenderResolutionMultiplier", "1.0")

    shader = TONEMAP.read_text(encoding="utf-8")
    assert "if (luma <= 1e-6)" in shader, "Reinhard black pixel needs NaN guard"
    assert shader.count("PBRReinhardToneMapping(exposed_color)") == 1, (
        "Reinhard must honor exposure when toneMap() supplies exposure"
    )
    print("PASS Reinhard black-pixel guard and exposure consistency")
    print("KAMI neutral base renderer static checks passed; in-world QA still required.")


if __name__ == "__main__":
    main()
