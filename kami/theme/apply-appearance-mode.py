#!/usr/bin/env python3
"""Bake native KAMI light/dark appearance into the Windows viewer build.

Runs during CI after Workspaces XUI generation, not on user machines.
Both appearance modes change only named UI color tokens. It does not add a world render
pass, imitate Apple's assets, or claim real-time backdrop blur.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
UI = ROOT / "indra/llui"


def patch(path, needle, replacement):
    content = path.read_text(encoding="utf-8-sig")
    count = content.count(needle)
    if count != 1:
        raise RuntimeError(f"KAMI appearance: {path} expected one anchor, found {count}: {needle[:90]!r}")
    path.write_text(content.replace(needle, replacement, 1), encoding="utf-8")


header = UI / "lluicolortable.h"
patch(header, "    void saveUserSettings() const;\n",
"""    void saveUserSettings() const;

    // Runtime theme values never enter users' saved colors.xml.
    void setKAMIRuntimeColor(std::string_view name, const LLColor4& color);
    void clearKAMIRuntimePalette();
""")
patch(header, "    string_color_map_t mUserSetColors;\n",
"""    string_color_map_t mUserSetColors;
    string_color_map_t mKAMIPaletteOriginals;
""")

colors = UI / "lluicolortable.cpp"
patch(colors, "void LLUIColorTable::clear()\n{",
"""void LLUIColorTable::setKAMIRuntimeColor(std::string_view name, const LLColor4& color)
{
    auto it = mLoadedColors.find(name);
    if (it == mLoadedColors.end()) return;
    // Preserve reference relationships as well as values on restoration.
    mKAMIPaletteOriginals.try_emplace(it->first, it->second);
    it->second.set(color);
}

void LLUIColorTable::clearKAMIRuntimePalette()
{
    for (const auto& entry : mKAMIPaletteOriginals)
    {
        auto it = mLoadedColors.find(entry.first);
        if (it != mLoadedColors.end())
        {
            it->second = entry.second;
        }
    }
    mKAMIPaletteOriginals.clear();
}

void LLUIColorTable::clear()
{""")
patch(colors, "void LLUIColorTable::clear()\n{\n    clearTable(mLoadedColors);",
"""void LLUIColorTable::clear()
{
    mKAMIPaletteOriginals.clear();
    clearTable(mLoadedColors);""")

viewer_header = VIEW / "llviewercontrol.h"
patch(viewer_header, "void settings_setup_listeners();\n",
"""void settings_setup_listeners();
void kami_apply_appearance();
""")

viewer = VIEW / "llviewercontrol.cpp"
patch(viewer, '#include "llviewercontrol.h"\n',
'#include "llviewercontrol.h"\n#include "lluicolortable.h"\n')
patch(viewer, "void settings_setup_listeners()\n{",
"""// No compositor pass or per-frame theme work; this runs at startup or
// when the resident changes the light-mode preference.
void kami_apply_appearance()
{
    auto& palette = LLUIColorTable::instance();
    palette.clearKAMIRuntimePalette();
    const F32 opacity = llclamp(gSavedSettings.getF32("KamiGlassOpacity"), .80f, 1.f);
    // Original KAMI interface, inspired by iOS system colors, not Apple's assets.
    // Live mode switching only edits existing UI colors; never touches world lighting.
    const bool light = gSavedSettings.getBOOL("KamiLightMode");
    struct Swatch { const char* name; LLColor4 value; };
    const Swatch dark[] = {
        {"FloaterDefaultBackgroundColor", LLColor4(.075f,.085f,.11f,opacity)},
        {"FloaterFocusBackgroundColor", LLColor4(.10f,.11f,.14f,.97f)},
        {"FloaterFocusBorderColor", LLColor4(.31f,.63f,.99f,.77f)},
        {"FloaterUnfocusBorderColor", LLColor4(.46f,.48f,.53f,.42f)},
        {"PanelDefaultBackgroundColor", LLColor4(.10f,.11f,.14f,opacity)},
        {"PanelFocusBackgroundColor", LLColor4(.13f,.14f,.17f,.96f)},
        {"MenuBarBgColor", LLColor4(.08f,.09f,.12f,.96f)},
        {"MenuDefaultBgColor", LLColor4(.10f,.11f,.14f,.97f)},
        {"MenuPopupBgColor", LLColor4(.13f,.14f,.17f,.97f)},
        {"MenuItemEnabledColor", LLColor4(.96f,.97f,.99f,1.f)},
        {"MenuItemDisabledColor", LLColor4(.53f,.56f,.62f,.9f)},
        {"MenuItemHighlightBgColor", LLColor4(.04f,.51f,1.f,.78f)},
        {"MenuItemHighlightFgColor", LLColor4(1.f,1.f,1.f,1.f)},
        {"ButtonColor", LLColor4(.17f,.19f,.24f,.97f)},
        {"ButtonLabelColor", LLColor4(.94f,.96f,1.f,1.f)},
        {"ButtonLabelSelectedColor", LLColor4(1.f,1.f,1.f,1.f)},
        {"LabelTextColor", LLColor4(.93f,.94f,.98f,1.f)},
        {"InventoryBackgroundColor", LLColor4(.10f,.11f,.14f,.96f)},
        {"ChatHistoryBgColor", LLColor4(.09f,.10f,.13f,.96f)},
        {"ChatHistoryTextColor", LLColor4(.94f,.95f,.97f,1.f)},
        {"FilterBackgroundColor", LLColor4(.17f,.18f,.22f,.97f)},
        {"FilterTextColor", LLColor4(.88f,.92f,.98f,1.f)},
        {"TextDefaultColor", LLColor4(.95f,.96f,.99f,1.f)},
        {"TextFgColor", LLColor4(.94f,.95f,.98f,1.f)},
        {"TextFgReadOnlyColor", LLColor4(.72f,.76f,.83f,1.f)},
        {"TextBgWriteableColor", LLColor4(.16f,.17f,.21f,.97f)},
        {"TextBgReadOnlyColor", LLColor4(.13f,.14f,.17f,.96f)},
        {"ScrollBgReadOnlyColor", LLColor4(.12f,.13f,.17f,.98f)},
        {"ScrollBgWriteableColor", LLColor4(.16f,.17f,.21f,.98f)},
        {"ScrollSelectedBGColor", LLColor4(.04f,.51f,1.f,.78f)},
        {"ScrollSelectedFGColor", LLColor4(1.f,1.f,1.f,1.f)},
        {"ScrollUnselectedColor", LLColor4(.91f,.93f,.97f,1.f)},
        {"ToolTipBgColor", LLColor4(.17f,.18f,.22f,.98f)},
        {"ToolTipBorderColor", LLColor4(.38f,.52f,.73f,.75f)},
        {"ToolTipTextColor", LLColor4(.97f,.98f,1.f,1.f)},
        {"EmphasisColor", LLColor4(.04f,.51f,1.f,.5f)},
        {"EmphasisColor_13", LLColor4(.04f,.51f,1.f,.13f)},
        {"EmphasisColor_35", LLColor4(.04f,.51f,1.f,.35f)}
    };
    const Swatch lightColors[] = {
        {"FloaterDefaultBackgroundColor", LLColor4(.95f,.96f,.98f,opacity)},
        {"FloaterFocusBackgroundColor", LLColor4(.98f,.985f,1.f,.98f)},
        {"FloaterFocusBorderColor", LLColor4(.0f,.48f,1.f,.72f)},
        {"FloaterUnfocusBorderColor", LLColor4(.62f,.65f,.72f,.46f)},
        {"PanelDefaultBackgroundColor", LLColor4(.96f,.965f,.985f,opacity)},
        {"PanelFocusBackgroundColor", LLColor4(.98f,.985f,1.f,.98f)},
        {"MenuBarBgColor", LLColor4(.96f,.97f,.99f,.97f)},
        {"MenuDefaultBgColor", LLColor4(.97f,.975f,.995f,.97f)},
        {"MenuPopupBgColor", LLColor4(.98f,.985f,1.f,.99f)},
        {"MenuItemEnabledColor", LLColor4(.10f,.12f,.17f,1.f)},
        {"MenuItemDisabledColor", LLColor4(.49f,.52f,.58f,.9f)},
        {"MenuItemHighlightBgColor", LLColor4(.02f,.48f,.99f,.88f)},
        {"MenuItemHighlightFgColor", LLColor4(1.f,1.f,1.f,1.f)},
        {"ButtonColor", LLColor4(.89f,.91f,.95f,.98f)},
        {"ButtonLabelColor", LLColor4(.10f,.13f,.19f,1.f)},
        {"ButtonLabelSelectedColor", LLColor4(.04f,.27f,.60f,1.f)},
        {"LabelTextColor", LLColor4(.12f,.14f,.20f,1.f)},
        {"InventoryBackgroundColor", LLColor4(.99f,.99f,1.f,.98f)},
        {"ChatHistoryBgColor", LLColor4(.97f,.98f,.995f,.96f)},
        {"ChatHistoryTextColor", LLColor4(.12f,.14f,.19f,1.f)},
        {"FilterBackgroundColor", LLColor4(.92f,.94f,.97f,.98f)},
        {"FilterTextColor", LLColor4(.15f,.22f,.35f,1.f)},
        {"TextDefaultColor", LLColor4(.10f,.12f,.17f,1.f)},
        {"TextFgColor", LLColor4(.10f,.12f,.17f,1.f)},
        {"TextFgReadOnlyColor", LLColor4(.34f,.37f,.44f,1.f)},
        {"TextBgWriteableColor", LLColor4(1.f,1.f,1.f,.99f)},
        {"TextBgReadOnlyColor", LLColor4(.94f,.95f,.98f,.98f)},
        {"ScrollBgReadOnlyColor", LLColor4(.95f,.96f,.98f,.99f)},
        {"ScrollBgWriteableColor", LLColor4(1.f,1.f,1.f,.99f)},
        {"ScrollSelectedBGColor", LLColor4(.02f,.48f,.99f,.88f)},
        {"ScrollSelectedFGColor", LLColor4(1.f,1.f,1.f,1.f)},
        {"ScrollUnselectedColor", LLColor4(.14f,.17f,.23f,1.f)},
        {"ToolTipBgColor", LLColor4(.99f,.995f,1.f,.98f)},
        {"ToolTipBorderColor", LLColor4(.55f,.62f,.75f,.76f)},
        {"ToolTipTextColor", LLColor4(.12f,.14f,.2f,1.f)},
        {"EmphasisColor", LLColor4(.0f,.48f,1.f,.5f)},
        {"EmphasisColor_13", LLColor4(.0f,.48f,1.f,.13f)},
        {"EmphasisColor_35", LLColor4(.0f,.48f,1.f,.35f)}
    };
    const Swatch* swatches = light ? lightColors : dark;
    const size_t count = light ? sizeof(lightColors)/sizeof(lightColors[0])
                               : sizeof(dark)/sizeof(dark[0]);
    for (size_t i = 0; i < count; ++i)
        palette.setKAMIRuntimeColor(swatches[i].name, swatches[i].value);
}

void settings_setup_listeners()
{""")
patch(viewer, "void settings_setup_listeners()\n{",
"""void settings_setup_listeners()
{
    setting_setup_signal_listener(gSavedSettings, "KamiLightMode",
        [](const LLSD&) { kami_apply_appearance(); });
    setting_setup_signal_listener(gSavedSettings, "KamiGlassOpacity",
        [](const LLSD&) { kami_apply_appearance(); });
""")

app = VIEW / "llappviewer.cpp"
patch(app, "void LLAppViewer::loadColorSettings()\n{\n    LLUIColorTable::instance().loadFromSettings();",
"""void LLAppViewer::loadColorSettings()
{
    LLUIColorTable::instance().loadFromSettings();
    kami_apply_appearance();""")

settings = VIEW / "app_settings/settings.xml"
settings_tree = ET.parse(settings)
settings_map = settings_tree.getroot().find("map")
if settings_map is None:
    raise RuntimeError("KAMI settings root is not an LLSD map")

def add_setting(name, typename, element, value, comment):
    if any(key.text == name for key in settings_map.findall("key")):
        raise RuntimeError(f"Duplicate KAMI preference: {name}")
    ET.SubElement(settings_map, "key").text = name
    setting = ET.SubElement(settings_map, "map")
    for key, tag, text in (
        ("Comment", "string", comment),
        ("Persist", "integer", "1"),
        ("Type", "string", typename),
        ("Value", element, value),
    ):
        ET.SubElement(setting, "key").text = key
        ET.SubElement(setting, tag).text = text

add_setting(
    "KamiLightMode", "Boolean", "integer", "0",
    "Use KAMI iOS-style light glass; off selects iOS-style dark glass with system blue accents."
)
add_setting(
    "KamiGlassOpacity", "F32", "real", "0.96",
    "KAMI native glass opacity; changes the panel color alpha without fullscreen blur."
)
ET.indent(settings_tree, space=" ")
settings_tree.write(settings, encoding="utf-8", xml_declaration=True)
ET.parse(settings)

# Workspaces is already generated by apply-nextgen-workspaces.py.
menu = VIEW / "skins/default/xui/en/menu_viewer.xml"
tree = ET.parse(menu)
matches = [e for e in tree.getroot().iter("menu") if e.get("name") == "KAMI_Workspaces"]
if len(matches) != 1:
    raise RuntimeError("KAMI Workspaces menu is missing or duplicated")
workspace = matches[0]
if any(e.get("name") == "KamiLightAppearance" for e in workspace.iter()):
    raise RuntimeError("Light appearance menu already installed")
ET.SubElement(workspace, "menu_item_separator")
item = ET.SubElement(workspace, "menu_item_check", {
    "name": "KamiLightAppearance",
    "label": "LIGHT APPEARANCE  /  DARK WHEN OFF",
})
ET.SubElement(item, "menu_item_check.on_check", {"control": "KamiLightMode"})
ET.SubElement(item, "menu_item_check.on_click", {
    "function": "ToggleControl", "parameter": "KamiLightMode",
})
ET.indent(tree, space=" ")
tree.write(menu, encoding="utf-8", xml_declaration=True)
ET.parse(menu)
print("PASS: native KAMI light/dark palette, persistent menu toggle, no renderer changes")
