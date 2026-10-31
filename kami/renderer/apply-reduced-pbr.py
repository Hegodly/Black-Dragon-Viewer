#!/usr/bin/env python3
"""KAMI native reduced-PBR compatibility toggle.

This intentionally does NOT claim to remove the GLTF material renderer.
Second Life no longer offers an independent old ALM forward renderer.
Instead an opt-in checkbox genuinely disables advanced HDR/emissive buffers,
reflection probe rendering, SSR and mirror passes with the existing native
shader/buffer invalidation callbacks. Prior choices are saved and restored.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
SETTINGS=R/"indra/newview/app_settings/settings_blackdragon.xml"
CONTROL=R/"indra/newview/llviewercontrol.cpp"
UI=R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"

def replace_one(path,before,after):
    s=path.read_text(encoding="utf-8-sig")
    n=s.count(before)
    if n!=1:
        raise RuntimeError(f"{path}: expected one exact anchor, got {n}: {before[:90]!r}")
    path.write_text(s.replace(before,after,1),encoding="utf-8")

tree=ET.parse(SETTINGS)
settings=tree.getroot().find("map")
if settings is None:
    raise RuntimeError("KAMI compatibility settings map not found")
entry=list(settings)
existing={n.text for n in entry if n.tag=="key"}
new_keys=[
    ("KAMIReducedPBR","0","KAMI opt-in reduced-PBR lighting. This does not disable all GLTF material processing."),
    ("KAMIPBRBackupValid","0","Saved original rendering options exist while the reduced PBR mode is active."),
    ("KAMIPBRBackupVintage","1","Saved advanced PBR HDR/emissive preference."),
    ("KAMIPBRBackupReflections","1","Saved reflection probes preference."),
    ("KAMIPBRBackupSSR","0","Saved screen space reflections preference."),
    ("KAMIPBRBackupMirrors","0","Saved mirror-rendering preference."),
    ("KAMIPBRBackupHDR","1","Saved internal HDR buffer preference."),
    ("KAMIPBRBackupEmissive","1","Saved internal emissive-buffer preference."),
]
for name,value,comment in new_keys:
    if name in existing: raise RuntimeError("Duplicate KAMI PBR preference "+name)
    key=ET.SubElement(settings,"key"); key.text=name
    value_map=ET.SubElement(settings,"map")
    for tag,s in (("key","Comment"),("string",comment),
                  ("key","Persist"),("integer","1"),
                  ("key","Type"),("string","Boolean"),
                  ("key","Value"),("integer",value)):
        e=ET.SubElement(value_map,tag);e.text=s
ET.indent(tree,space=" ")
tree.write(SETTINGS,encoding="utf-8",xml_declaration=True)

before="static bool handleEnableHDR(const LLSD& newvalue)\n{"
after=r'''// KAMI PBR compatibility: native one-click tradeoff, not a fake renderer
// switch. It reduces expensive optional PBR lighting passes without changing
// world resolution, texture LOD, avatar geometry or original GLTF assets.
// Existing Render* handlers invalidate shader/FBO/probe state as required.
static bool handleKAMIReducedPBR(const LLSD& newvalue)
{
    const bool reduced = newvalue.asBoolean();
    if (reduced)
    {
        if (!gSavedSettings.getBOOL("KAMIPBRBackupValid"))
        {
            gSavedSettings.setBOOL("KAMIPBRBackupVintage",
                gSavedSettings.getBOOL("RenderDisableVintageMode"));
            gSavedSettings.setBOOL("KAMIPBRBackupReflections",
                gSavedSettings.getBOOL("RenderReflectionsEnabled"));
            gSavedSettings.setBOOL("KAMIPBRBackupSSR",
                gSavedSettings.getBOOL("RenderScreenSpaceReflections"));
            gSavedSettings.setBOOL("KAMIPBRBackupMirrors",
                gSavedSettings.getBOOL("RenderMirrors"));
            gSavedSettings.setBOOL("KAMIPBRBackupHDR",
                gSavedSettings.getBOOL("RenderHDREnabled"));
            gSavedSettings.setBOOL("KAMIPBRBackupEmissive",
                gSavedSettings.getBOOL("RenderEnableEmissiveBuffer"));
            gSavedSettings.setBOOL("KAMIPBRBackupValid", true);
        }
        // Avoid unnecessary shader/buffer churn by changing only real values.
        if (gSavedSettings.getBOOL("RenderScreenSpaceReflections"))
            gSavedSettings.setBOOL("RenderScreenSpaceReflections", false);
        if (gSavedSettings.getBOOL("RenderMirrors"))
            gSavedSettings.setBOOL("RenderMirrors", false);
        if (gSavedSettings.getBOOL("RenderReflectionsEnabled"))
            gSavedSettings.setBOOL("RenderReflectionsEnabled", false);
        if (gSavedSettings.getBOOL("RenderDisableVintageMode"))
            gSavedSettings.setBOOL("RenderDisableVintageMode", false);
        // RenderDisableVintageMode's existing listener also disables HDR
        // and emissive attachments. Handle cases where it was already false
        // but an independent advanced setting remains true.
        if (gSavedSettings.getBOOL("RenderHDREnabled"))
            gSavedSettings.setBOOL("RenderHDREnabled", false);
        if (gSavedSettings.getBOOL("RenderEnableEmissiveBuffer"))
            gSavedSettings.setBOOL("RenderEnableEmissiveBuffer", false);
    }
    else if (gSavedSettings.getBOOL("KAMIPBRBackupValid"))
    {
        // Restore exact prior state, even if the original user had selected
        // partial reflection, vintage or HDR settings before using KAMI.
        const bool old_vintage = gSavedSettings.getBOOL("KAMIPBRBackupVintage");
        gSavedSettings.setBOOL("RenderDisableVintageMode", old_vintage);
        gSavedSettings.setBOOL("RenderHDREnabled",
            gSavedSettings.getBOOL("KAMIPBRBackupHDR"));
        gSavedSettings.setBOOL("RenderEnableEmissiveBuffer",
            gSavedSettings.getBOOL("KAMIPBRBackupEmissive"));
        gSavedSettings.setBOOL("RenderReflectionsEnabled",
            gSavedSettings.getBOOL("KAMIPBRBackupReflections"));
        gSavedSettings.setBOOL("RenderScreenSpaceReflections",
            gSavedSettings.getBOOL("KAMIPBRBackupSSR"));
        gSavedSettings.setBOOL("RenderMirrors",
            gSavedSettings.getBOOL("KAMIPBRBackupMirrors"));
        gSavedSettings.setBOOL("KAMIPBRBackupValid", false);
    }
    return true;
}

static bool handleEnableHDR(const LLSD& newvalue)
{'''
replace_one(CONTROL,before,after)
replace_one(CONTROL,
    '    setting_setup_signal_listener(gSavedSettings, "RenderDisableVintageMode", handleDisableVintageMode);',
    '''    setting_setup_signal_listener(gSavedSettings, "RenderDisableVintageMode", handleDisableVintageMode);
    setting_setup_signal_listener(gSavedSettings, "KAMIReducedPBR", handleKAMIReducedPBR);''')

ui=ET.parse(UI);root=ui.getroot()
old=[n for n in root.iter("check_box") if n.get("name")=="RenderDisableVintageMode"]
if len(old)!=1: raise RuntimeError("Expected Black Dragon vintage checkbox")
existing=old[0]
parent=[p for p in root.iter() if existing in list(p)]
if len(parent)!=1: raise RuntimeError("Vintage checkbox parent not unique")
parent=parent[0]
index=list(parent).index(existing)
new=ET.Element("check_box",{
    "control_name":"KAMIReducedPBR",
    "height":"23", "initial_value":"false",
    "label":"Reduced PBR / Vintage Lighting (fewer GPU effects)",
    "tool_tip":(
        "Opt-in performance compatibility. Disables HDR and emissive buffers, "
        "reflection probes, screen-space reflections, and mirror rendering. "
        "Restores your exact original values when switched off. "
        "PBR/glTF material assets still render via the current viewer pipeline; "
        "this is NOT a fully non-PBR engine or guaranteed FPS gain. "
        "May reduce reflectivity, glow, lighting quality and PBR-only visual effects."),
    "layout":"topleft", "left":"10", "top_pad":"6",
    "name":"kami_reduced_pbr", "width":"460",
})
parent.insert(index,new)
# Avoid presenting two independent conflicting switches at the same visual
# hierarchy: the original advanced vintage checkbox is retained for source
# compatibility, but advanced settings can be reached through debug controls.
existing.set("visible","false")
existing.set("tool_tip","Controlled by the KAMI Reduced PBR toggle; internal legacy HDR/emissive setting.")
ET.indent(ui,space=" ")
ui.write(UI,encoding="utf-8",xml_declaration=True)
ET.parse(UI)
print("Native KAMI reduced-PBR toggle wired; world materials, geometry and resolution are preserved.")
