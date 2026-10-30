#!/usr/bin/env python3
"""Static contract checks: KAMI Avatar Quality + FPS; live GPU testing follows."""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
def read(p):return (R/p).read_text(encoding="utf-8-sig")
def check(name,valid):
    if not valid:raise AssertionError("Avatar Quality + FPS: "+name)
    print("PASS:",name)

avatar=read("indra/newview/llvoavatar.cpp")
settings=ET.parse(R/"indra/newview/app_settings/settings_blackdragon.xml").getroot()
ui=ET.parse(R/"indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml").getroot()
keys=[node.text for node in settings.find("map") if node.tag=="key"]
check("one real opt-in persisted setting", keys.count("KAMIAvatarQualityFPS")==1)
check("settings cached in actual avatar LOD update",
      'static LLCachedControl<bool> kami_avatar_quality_fps(' in avatar and
      'gSavedSettings, "KAMIAvatarQualityFPS", false' in avatar and
      'mAdjustedPixelArea = llmax(mAdjustedPixelArea, 262144.f);' in avatar)
check("distant avatars optimized gradually, self untouched by reduction",
      'else if (!mIsDummy && mDrawable.notNull())' in avatar and
      'mAdjustedPixelArea *= factor;' in avatar and
      '0.65f, 1.f' in avatar)
controlcpp=read("indra/newview/llviewercontrol.cpp")
check("live toggle refreshes existing impostor budget",
      'setting_setup_signal_listener(gSavedSettings, "KAMIAvatarQualityFPS", handleKAMIAvatarQualityChanged)' in controlcpp and
      'LLVOAvatar::updateImpostorRendering(' in controlcpp)
check("native crowd impostor cap respects stricter FPS settings",
      'sMaxNonImpostors == 0 || sMaxNonImpostors > 8' in avatar and
      'sMaxNonImpostors = 8;' in avatar and
      'sLimitNonImpostors = (0 != sMaxNonImpostors);' in avatar)
check("native mesh LOD renderer still processes quality changes",
      'root->updateLOD(mAdjustedPixelArea, true);' in avatar and
      'dirtyMesh(2);' in avatar)
check("no extra avatar render pass, texture reload or FPS cap",
      all(name not in read("kami/avatars/apply-avatar-quality.py") for name in (
          'set("RenderResolutionMultiplier"', 'set("FramePerSecondLimit"',
          'set("RenderMaxTextureResolution"')))
check("reversible and no auto-toggle on login",
      'if (kami_avatar_quality_fps)' in avatar and
      'KAMIAvatarQualityFPS' not in read("indra/newview/kami_quality_presets.h"))
byname={n.get("name"):n for n in ui.iter() if n.get("name")}
checkbox=byname.get("kami_avatar_quality_toggle")
check("real native Fire Garden avatar quality toggle",
      checkbox is not None and checkbox.tag=="check_box" and
      checkbox.get("control_name")=="KAMIAvatarQualityFPS")
quick=byname.get("kombatant_display_quick")
scroll=byname.get("gfx_scroll_panel")
stack=byname.get("gfx_stack")
check("all graphics scroll ancestors exist",
      quick is not None and scroll is not None and stack is not None)
check("Avatar Quality visible after Session Recovery",
      int(byname["kami_avatar_quality_heading"].get("top")) >
      int(byname["kami_recovery_status"].get("top")) and
      int(checkbox.get("top"))+int(checkbox.get("height")) <= int(quick.get("height")))
native_panels={"basic_layout_panel","advanced_layout_panel","finetuning_layout_panel"}
children=[x for x in stack if x.tag=="layout_panel"]
check("original Black Dragon graphics sections remain in scroll",
      native_panels <= {x.get("name") for x in children} and
      int(stack.get("height"))>=sum(int(x.get("height","0")) for x in children)+64)
check("scroll child extends beyond entire KAMI controls",
      int(scroll.get("height"))>=int(quick.get("height"))+66)
print("Avatar Quality + FPS static contracts passed; verify live image quality and FPS separately.")
