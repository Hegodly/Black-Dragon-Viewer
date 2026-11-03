#!/usr/bin/env python3
"""Contract tests: only user-visible branding may be rewritten."""
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET
sys.path.insert(0, str(Path(__file__).resolve().parent))
import importlib.util
spec = importlib.util.spec_from_file_location("kami_native_identity",
    Path(__file__).with_name("finalize-native-identity.py"))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

sample = ET.fromstring('''<menu_bar>
  <menu name="Me" label="Dragon">
    <menu_item_call name="BlackDragon_Internal" label="Black Dragon Camera"
      tool_tip="Use the Black Dragon camera">
      <menu_item_call.on_click function="Dragon.CameraLock"
        parameter="BlackDragon.Camera" />
    </menu_item_call>
    <text name="credit">Based on Black Dragon by NiranV Dean</text>
  </menu>
</menu_bar>''')
before = module.control_signature(sample)
assert module.finalize_tree(sample) == 3
assert before == module.control_signature(sample)
menu = sample.find("menu")
assert menu.get("label") == "KAMI"
button = menu.find("menu_item_call")
assert button.get("label") == "KAMI Camera"
assert button.get("tool_tip") == "Use the KAMI camera"
assert button.get("name") == "BlackDragon_Internal"
assert button.find("menu_item_call.on_click").get("function") == "Dragon.CameraLock"
assert "Black Dragon by NiranV" in menu.find("text").text
print("PASS: native identity safely changes labels but preserves controls and credits")
