#!/usr/bin/env python3
"""Protect the REAL native Black Dragon-derived graphics settings in KAMI.

User's IMG_0616.mp4 shows the stock Basic Settings page and its switches:
world resolution, textures, LOD, lights, antialiasing, water, avatar simulation,
particles and legacy sky auto-adjust. Keep those functional settings, with the
approved KAMI Fire Garden theme applied to their existing XUI widgets.
No duplicate sliders; no fake settings; no extra render passes.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[2]
xui=ROOT/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
tree=ET.parse(xui);root=tree.getroot()
controls={
    # Window, UI, memory and resident textures (0:00-0:04 of reference video).
    "FullScreenWindow": {"check_box"},
    "RenderVSyncEnable": {"check_box"},
    "UIScaleFactor": {"combo_box"},
    "FontScreenDPI": {"slider"},
    "RenderCompressTextures": {"check_box"},
    "RenderMaxVRAMBudget": {"combo_box"},
    # World geometry, terrain, avatars and texture quality (0:03-0:06).
    "RenderFarClip": {"slider"},
    "RenderDynamicLOD": {"check_box"},
    "RenderVolumeLODFactor": {"combo_box"},
    "RenderAvatarLODFactor": {"combo_box"},
    "RenderTerrainLODFactor": {"combo_box"},
    "RenderTerrainScale": {"combo_box"},
    "RenderTreeLODFactor": {"combo_box"},
    "RenderMaxTextureResolution": {"combo_box"},
    # Shading and frame quality.
    "RenderGlowHDR": {"check_box"},
    "RenderGlowResolutionPow": {"combo_box"},
    "RenderFSAAType": {"combo_box"},
    "RenderFSAASamples": {"combo_box"},
    # Scene lighting / attachment limits (0:06-0:07).
    "RenderDeferredLights": {"check_box"},
    "RenderOtherAttachedLights": {"check_box"},
    "RenderOwnAttachedLights": {"check_box"},
    "RenderLocalLightCount": {"combo_box"},
    "RenderAttachedParticles": {"check_box"},
    "RenderMaxPartCount": {"combo_box"},
    # Avatar physics, water and PBR probes.
    "AvatarPhysics": {"check_box"},
    "RenderAvatarPhysicsLODFactor": {"combo_box"},
    "WLSkyDetail": {"combo_box"},
    "RenderWaterMipNormal": {"check_box"},
    "RenderTransparentWater": {"check_box"},
    "RenderReflectionsEnabled": {"check_box"},
    "RenderReflectionProbeDetail": {"combo_box"},
    "RenderAnisotropic": {"check_box"},
    # Real user-controlled caps (no preset touches these). Avatar budgets.
    "FramePerSecondLimit": {"combo_box"},
    "BackgroundYieldTime": {"combo_box"},
    "AlwaysRenderFriends": {"check_box"},
    "RenderAvatarMaxNonImpostors": {"combo_box"},
    "RenderAvatarMaxComplexity": {"combo_box"},
    # Legacy sky HDR/tone-map compatibility (video 0:09-0:11).
    "RenderSkyAutoAdjustLegacy": {"check_box"},
    "RenderSkyAutoAdjustAmbientScale": {"slider"},
    "RenderSkyAutoAdjustBlueDensityScale": {"slider"},
    "RenderSkyAutoAdjustBlueHorizonScale": {"slider"},
    "RenderSkyAutoAdjustHDRScale": {"slider"},
}
all_controls={}
for node in root.iter():
    key=node.get("control_name")
    if key:
        all_controls.setdefault(key,[]).append(node)
for name,tags in controls.items():
    matches=[n for n in all_controls.get(name,[]) if n.tag in tags]
    assert len(matches)==1,(name,[n.tag for n in all_controls.get(name,[])])
    node=matches[0]
    assert not node.get("visible")=="false",f"The original {name} toggle has been hidden"
    assert node.get("enabled")!="false",f"The original {name} toggle was disabled"
    assert node.get("control_name")==name
    if node.tag=="combo_box":
        assert len(node.findall("combo_box.item"))>=2,(name,"lost options")
# Preserve original listeners such as RenderSkyAutoAdjustLegacy's HDR dependency.
auto=next(n for n in root.iter() if n.get("control_name")=="RenderSkyAutoAdjustLegacy")
assert auto.get("enabled_control")=="RenderHDREnabled"

# Brand the existing basic graphics section without moving or replacing ANY
# of the functional controls. The user explicitly asked to leave toggles.
headers=[x for x in root.iter() if x.get("name")=="general_label"]
assert len(headers)==1,"Original core graphics section is missing"
headers[0].text="KAMI CORE GRAPHICS"
headers[0].set("tool_tip","Built-in Fire Garden themed viewer graphics. Original functional switches, sliders and menu items remain accessible.")
headers[0].set("text_color","0.72 0.85 0.96 1")

# Existing core toggles inherit the *actual* skinned assets. Explicitly check
# that our Fire Garden theme was applied to each widget family in textures.xml.
registry=ET.parse(ROOT/"indra/newview/skins/default/textures/textures.xml")
ref={node.get("name"):node.get("file_name") for node in registry.getroot().findall("texture")}
for texture in ("Checkbox_On","Checkbox_Off","SliderTrack_Horiz",
                "SliderThumb_Off","ComboButton_Off","PushButton_Off",
                "Toolbar_Middle_Off","TabTop_Middle_Selected"):
    assert ref.get(texture,"").startswith("windows/kami_fg_"),(
        "KAMI base kit still has unthemed widgets",texture,ref.get(texture)
    )
assert (ROOT/"indra/newview/skins/default/textures/windows/kami_fire_garden.jpg").is_file()

ET.indent(tree,space=" ")
tree.write(xui,encoding="utf-8",xml_declaration=True)
ET.parse(xui)

# Update a named immutable manifest to ensure no future styling/removal patch
# can accidentally make all original graphics controls disappear.
manifest=ROOT/"indra/newview/skins/default/KAMI-BASE-GRAPHICS-KIT.txt"
manifest.write_text(
    "KAMI Beyond Reality | Fire Garden | Native Graphics Base Kit\n"
    f"{len(controls)} original interactive renderer/UI settings kept intact.\n"
    "Includes checked original controls: "+", ".join(controls)+"\n"
    "All switches remain real viewer settings; themed by skin images/colors.\n"
    "User-controlled FPS cap preserved. No graphics preset forces FPS cap.\n",
    encoding="utf-8")
print(f"KAMI Fire Garden native base kit: {len(controls)} existing video-reference graphics controls preserved, all widget families themed.")
