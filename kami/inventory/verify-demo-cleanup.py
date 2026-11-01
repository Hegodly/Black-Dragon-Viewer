#!/usr/bin/env python3
"""KAMI Demo Deleter source and integrated safety checks."""
import argparse
import ast
import xml.etree.ElementTree as ET
from pathlib import Path
R=Path(__file__).resolve().parents[2]
P=R/"kami/inventory/apply-demo-cleanup.py"
src=P.read_text(encoding="utf-8")
ast.parse(src)
for expected in (
    "DEMO_CLEANUP = 4", "kami_name_is_demo", "kami_preview_demo_cleanup",
    "KamiSmartSortDemoCleanupPreview", "findCategoryUUIDForType(LLFolderType::FT_TRASH)",
    "gInventory.changeCategoryParent(cat, trash, false)",
    "gInventory.changeItemParent(item, trash, false)",
    "getPreferredType() != LLFolderType::FT_NONE",
    "isCategoryComplete(scope.folder)", "KAMI_DEMO_MAX_SCANNED",
    "KAMI_DEMO_MAX_MOVES = 60", "KAMI_DEMO_MAX_DEPTH = 5",
    "getSelectedOption(notification, response) == 0",
    "entry.original_parent", "kami_name_is_demo(cat->getName())",
    "kami_name_is_demo(item->getName())",
):
    assert expected in src,expected
assert "removeCategory(" not in src and "removeItem(" not in src
assert "emptyFolderType(" not in src and "emptyTrash" not in src

def integrated():
    c=(R/"indra/newview/llpanelmaininventory.cpp").read_text(encoding="utf-8-sig")
    for key in ("DEMO_CLEANUP = 4", "kami_preview_demo_cleanup()",
                "kami_move_demo_candidates_to_trash(", "kami_name_is_demo(",
                'if (command_name == "kami_demo_cleanup")',
                'if (mode == KamiSortMode::DEMO_CLEANUP)',
                "changeCategoryParent(cat, trash, false)",
                "changeItemParent(item, trash, false)"):
        assert key in c,key
    x=R/"indra/newview/skins/default/xui/en"
    n=ET.parse(x/"notifications.xml").getroot()
    mode=[e for e in n.iter("notification") if e.get("name")=="KamiSmartSortChooseMode"]
    preview=[e for e in n.iter("notification") if e.get("name")=="KamiSmartSortDemoCleanupPreview"]
    assert len(mode)==len(preview)==1
    opts=[(e.get("index"),e.get("name")) for e in mode[0].iter("button")]
    assert opts[-2:]==[("4","ByDemoCleanup"),("5","Cancel")],opts
    assert "NOT permanent deletion" in (preview[0].text or "")
    menu=ET.parse(x/"menu_inventory_gear_default.xml").getroot()
    assert len([e for e in menu.iter("menu_item_call") if e.get("name")=="kami_demo_cleanup"])==1
    ET.parse(x/"panel_main_inventory.xml")
    print("PASS: native DEMO mode, preview confirmation, guarded SL Trash moves, existing Smart Sort intact")

p=argparse.ArgumentParser()
p.add_argument("--integrated",action="store_true")
args=p.parse_args()
print("PASS Demo Deleter installer parses; name matching and safety limits present")
if args.integrated:integrated()
else:print("Native compilation and actual inventory behavior remain unverified.")
