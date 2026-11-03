#!/usr/bin/env python3
"""Bake native KAMI light/dark appearance into the Windows viewer build.

Runs during CI after Workspaces XUI generation, not on user machines.
Light mode changes only named UI color tokens. It does not add a world render
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
    if (!gSavedSettings.getBOOL("KamiLightMode")) return;

    // Original KAMI palette: cool silver, soft frost, graphite typography.
    // Mutate existing named colors so controls holding LLUIColor references
    // update without being recreated. User-customized colors retain priority.
    struct Swatch { const char* name; LLColor4 value; };
    const Swatch light[] = {
        {"FloaterDefaultBackgroundColor", LLColor4(.94f,.96f,.985f,.94f)},
        {"FloaterFocusBackgroundColor", LLColor4(.96f,.97f,.995f,.97f)},
        {"FloaterFocusBorderColor", LLColor4(.40f,.53f,.67f,.72f)},
        {"FloaterUnfocusBorderColor", LLColor4(.63f,.70f,.77f,.44f)},
        {"PanelDefaultBackgroundColor", LLColor4(.94f,.96f,.985f,.94f)},
        {"PanelFocusBackgroundColor", LLColor4(.96f,.97f,.995f,.97f)},
        {"MenuBarBgColor", LLColor4(.91f,.94f,.975f,.96f)},
        {"MenuDefaultBgColor", LLColor4(.94f,.96f,.985f,.97f)},
        {"MenuPopupBgColor", LLColor4(.95f,.97f,.99f,.98f)},
        {"MenuItemEnabledColor", LLColor4(.10f,.14f,.22f,1.f)},
        {"MenuItemDisabledColor", LLColor4(.42f,.48f,.55f,.78f)},
        {"MenuItemHighlightBgColor", LLColor4(.74f,.83f,.94f,.95f)},
        {"MenuItemHighlightFgColor", LLColor4(.06f,.11f,.19f,1.f)},
        {"ButtonColor", LLColor4(.85f,.90f,.96f,.98f)},
        {"ButtonLabelColor", LLColor4(.09f,.14f,.22f,1.f)},
        {"ButtonLabelSelectedColor", LLColor4(.07f,.12f,.20f,1.f)},
        {"LabelTextColor", LLColor4(.12f,.17f,.25f,1.f)},
        {"LabelSelectedColor", LLColor4(.06f,.12f,.20f,1.f)},
        {"InventoryBackgroundColor", LLColor4(.96f,.97f,.99f,.98f)},
        {"ChatHistoryBgColor", LLColor4(.95f,.97f,.99f,.95f)},
        {"ChatHistoryTextColor", LLColor4(.10f,.15f,.22f,1.f)},
        {"FilterBackgroundColor", LLColor4(.93f,.95f,.98f,.96f)},
        {"FilterTextColor", LLColor4(.13f,.24f,.37f,1.f)},
        {"TextDefaultColor", LLColor4(.09f,.14f,.22f,1.f)},
        {"TextFgColor", LLColor4(.09f,.14f,.22f,1.f)},
        {"TextFgReadOnlyColor", LLColor4(.27f,.32f,.39f,1.f)},
        {"TextBgWriteableColor", LLColor4(.99f,.995f,1.f,.98f)},
        {"TextBgReadOnlyColor", LLColor4(.93f,.95f,.97f,.96f)},
        {"ScrollBgReadOnlyColor", LLColor4(.95f,.96f,.98f,.98f)},
        {"ScrollBgWriteableColor", LLColor4(.99f,.995f,1.f,.98f)},
        {"ScrollSelectedBGColor", LLColor4(.69f,.81f,.94f,.90f)},
        {"ScrollSelectedFGColor", LLColor4(.05f,.10f,.18f,1.f)},
        {"ScrollUnselectedColor", LLColor4(.10f,.15f,.22f,1.f)},
        {"ToolTipBgColor", LLColor4(.94f,.96f,.985f,.97f)},
        {"ToolTipBorderColor", LLColor4(.49f,.57f,.68f,.7f)},
        {"ToolTipTextColor", LLColor4(.10f,.15f,.22f,1.f)}
    };
    for (const auto& swatch : light)
        palette.setKAMIRuntimeColor(swatch.name, swatch.value);
}

void settings_setup_listeners()
{""")
patch(viewer, "void settings_setup_listeners()\n{\n\t//BD - Special Debugs and handles",
"""void settings_setup_listeners()
{
    setting_setup_signal_listener(gSavedSettings, "KamiLightMode",
        [](const LLSD&) { kami_apply_appearance(); });
	//BD - Special Debugs and handles""")

app = VIEW / "llappviewer.cpp"
patch(app, "void LLAppViewer::loadColorSettings()\n{\n    LLUIColorTable::instance().loadFromSettings();",
"""void LLAppViewer::loadColorSettings()
{
    LLUIColorTable::instance().loadFromSettings();
    kami_apply_appearance();""")

settings = VIEW / "app_settings/settings.xml"
patch(settings, "  <key>KamiDX12LivePreview</key>\n",
"""  <key>KamiLightMode</key>
  <map>
    <key>Comment</key>
    <string>Use KAMI's original silver light interface; off restores the native dark palette. UI only, no world-rendering cost.</string>
    <key>Persist</key>
    <integer>1</integer>
    <key>Type</key>
    <string>Boolean</string>
    <key>Value</key>
    <integer>0</integer>
  </map>
  <key>KamiDX12LivePreview</key>
""")
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
