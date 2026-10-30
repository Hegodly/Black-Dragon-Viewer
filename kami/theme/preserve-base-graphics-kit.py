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
    # The native Black Dragon panel legitimately provides some settings in
    # multiple sections. Preserve ALL of those functional controls.
    assert len(matches)>=1,(name,[n.tag for n in all_controls.get(name,[])])
    assert any(node.get("visible")!="false" and node.get("enabled")!="false"
               for node in matches),(name,"all original controls hidden or disabled")
    for node in matches:
        assert node.get("control_name")==name
        if node.tag=="combo_box":
            assert len(node.findall("combo_box.item"))>=2,(name,"lost options")
# Preserve original listeners such as RenderSkyAutoAdjustLegacy's HDR dependency.
auto=next(n for n in root.iter() if n.get("control_name")=="RenderSkyAutoAdjustLegacy")
assert auto.get("enabled_control")=="RenderHDREnabled"

# Brand the existing basic graphics section without moving or replacing ANY
# of the functional controls. The user explicitly asked to leave toggles.
headers=[x for x in root.iter("text") if x.get("name")=="general_label"]
if not headers:
    # Some themed/compact UI layouts remove the legacy Basic Settings
    # heading. Preserve those layouts instead of failing the entire build.
    headers=[x for x in root.iter("text") if (x.text or "").strip().lower() in
             {"basic settings:","kami graphics","kombatant graphics"}]
if headers:
    headers[0].text="KAMI CORE GRAPHICS"
    headers[0].set("tool_tip","Built-in Fire Garden themed viewer graphics. Original functional switches, sliders and menu items remain accessible.")
    headers[0].set("text_color","0.72 0.85 0.96 1")
else:
    # Preserve all remaining required underlying controls. A harmless heading is appended
    # to the end of the scroll area rather than shifting their positions.
    stack=[x for x in root.iter() if x.get("name")=="kombatant_display_quick"]
    assert len(stack)==1, "KAMI graphics scroll area missing"
    import xml.etree.ElementTree as ET
    heading=ET.SubElement(stack[0],"text",{"name":"kami_core_graphics_label",
        "left":"12","top":"8","width":"465","height":"22",
        "font":"SansSerifLarge","text_color":"0.72 0.85 0.96 1"})
    heading.text="KAMI CORE GRAPHICS"

# October 30 in-world QA identified the following nonworking UI controls.
# Remove only their UI widgets (slider and paired default/reset button).
# Do not delete native renderer settings or shaders; only retire nonfunctional UI.
# "Bloom strength" in testing corresponds to native "Glow Strength".
unresponsive = {
    "RenderExposure": "Exposure",
    "RenderCASSharpness": "CAS Sharpness",
    "RenderGlowStrength": "Glow/Bloom Strength",
}
removed = {key: [] for key in unresponsive}
for parent in root.iter():
    for child in list(parent):
        key = child.get("control_name")
        if key in unresponsive:
            removed[key].append(child.tag)
            parent.remove(child)
for key, label in unresponsive.items():
    # Some KAMI render patches introduce a second Exposure slider; remove
    # all copies instead of assuming the original Black Dragon UI is unique.
    assert "slider" in removed[key] and "button" in removed[key] and all(
        tag in ("slider", "button") for tag in removed[key]
    ), ("Expected one or more sliders with reset buttons", label, removed[key])
    assert not any(n.get("control_name") == key for n in root.iter()), (
        "Nonfunctional UI control still visible", label
    )

# Remove entire inactive effect subpanels, not just the master toggles:
# otherwise DoF/motion-blur sliders and glow fine-tuning become orphaned.
# Preserve adjacent Tonemapping, SSAO, Volumetric Lighting and Post Effects.
effect_sections = (
    ("Depth of Field", "Volumetric Lighting"),
    ("Motion Blur", "Tone Mapping"),
    ("Post Processing Glow", "Post Effects"),
)
for start_name, end_name in effect_sections:
    starts = [n for n in root.iter("text") if (n.text or "").strip() == start_name]
    ends = [n for n in root.iter("text") if (n.text or "").strip() == end_name]
    assert len(starts) == len(ends) == 1, (start_name, end_name, len(starts), len(ends))
    first, last = starts[0], ends[0]
    parents = [p for p in root.iter() if first in list(p) and last in list(p)]
    assert len(parents) == 1, ("Effect heading layout changed", start_name, end_name)
    parent = parents[0]
    siblings = list(parent)
    begin, finish = siblings.index(first), siblings.index(last)
    assert begin < finish and finish - begin >= 3, ("Unexpected effect section", start_name)
    for child in siblings[begin:finish]:
        parent.remove(child)

# The stock basic page contains additional HDR Glow and Glow Quality controls
# outside the full Glow fine-tuning section. Remove their widgets too.
for key, tags in (
    ("RenderGlowHDR", ("check_box",)),
    ("RenderGlowResolutionPow", ("combo_box",)),
    ("GlowResolution", ("text",)),
):
    matches = [(p, n) for p in root.iter() for n in list(p)
               if n.get("control_name") == key or n.get("name") == key]
    assert len(matches) == 1 and matches[0][1].tag in tags, (key, matches)
    matches[0][0].remove(matches[0][1])

# Do not leave an enabled DoF, Bloom or Motion Blur widget lurking elsewhere
# in the Display panel. This does not alter the renderer or saved user settings.
retired_settings = {
    "RenderDepthOfField", "RenderDepthOfFieldAlphas",
    "RenderDepthOfFieldFront", "RenderDepthOfFieldInEditMode",
    "RenderDepthOfFieldChroma", "RenderDepthOfFieldHighQuality",
    "CameraFNumber", "CameraFocalLength", "CameraMaxCoF",
    "CameraFocusTransitionTime", "CameraDoFResScale",
    "RenderMotionBlur", "RenderMotionBlurStrength",
    "RenderGlowHDR", "RenderGlowResolutionPow", "RenderGlowIterations",
    "RenderGlowStrength", "RenderGlowMinLuminance", "RenderGlowWarmthAmount",
    "RenderGlowWidth", "RenderGlowMaxExtractAlpha", "RenderGlowLumWeights",
    "RenderGlowWarmthWeights",
}
# Other build patches may append duplicate widgets outside the original
# Black Dragon subpanels. Remove those too rather than aborting the release.
# This only changes UI presentation, not native shader/settings definitions.
orphaned = []
for parent in root.iter():
    for child in list(parent):
        if child.get("control_name") in retired_settings:
            orphaned.append((child.tag, child.get("control_name")))
            parent.remove(child)
assert not any(n.get("control_name") in retired_settings for n in root.iter()), (
    "DoF, motion blur or bloom UI remained visible")
print("Retired duplicate DoF/Bloom/MotionBlur widgets:", orphaned)
assert not any((n.text or "").strip() in
               {"Depth of Field", "Motion Blur", "Post Processing Glow", "Glow Quality:"}
               for n in root.iter("text")), "Orphaned effect heading"

# In-world QA: the optional private MXAO controls are not functional.
# GShade/KAMI-FX uses the SAME ReShade runtime, so retire only MXAO's own
# controls. Keep the GShade importer, effect pack and native SSAO/HBAO.
mxao_widgets = {
    "kami_mxao_heading", "kami_mxao_enable", "kami_mxao_amount",
    "kami_mxao_radius", "kami_mxao_quality", "kami_mxao_overlay",
    "kami_mxao_status", "kami_mxao_install", "kami_mxao_help",
}
quick = next((n for n in root.iter("layout_panel")
              if n.get("name") == "kombatant_display_quick"), None)
assert quick is not None, "KAMI Display scroll panel missing"
removed_mxao = []
for parent in root.iter():
    for child in list(parent):
        if child.get("name") in mxao_widgets:
            removed_mxao.append(child.get("name"))
            parent.remove(child)
assert set(removed_mxao) == mxao_widgets and len(removed_mxao) == len(mxao_widgets), (
    "Unexpected private MXAO controls; prevent orphaned widgets", removed_mxao)

# Private MXAO originally reserved 400 px between native graphics and
# GShade FX. Close that gap while preserving all later widgets and their
# relative placement. This also keeps the Display scroll bounds accurate.
old_height = int(quick.get("height"))
for widget in quick:
    top = widget.get("top")
    if top is not None:
        try:
            value = int(top)
        except ValueError:
            continue
        if value >= 4650:
            widget.set("top", str(value - 400))
quick.set("height", str(old_height - 400))
for ancestor in root.iter():
    if ancestor is not quick and ancestor.get("height") == str(old_height + 4):
        ancestor.set("height", str(old_height - 396))
assert not any(n.get("name") in mxao_widgets for n in root.iter()), (
    "Nonfunctional private MXAO widget remained")

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
    "User-controlled FPS cap preserved. No graphics preset forces FPS cap.\n"
    "Removed broken DoF, Bloom/Glow and Motion Blur settings and fine-tuning UI.\n"
    "Retired nonworking private MXAO panel and installer controls; GShade remains.\n",
    encoding="utf-8")
print(f"KAMI graphics base kit: {len(controls)} core controls preserved; removed nonworking Exposure/Sharpening, DoF, Bloom, Motion Blur, and private MXAO UI.")
