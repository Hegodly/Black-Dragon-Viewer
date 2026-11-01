#!/usr/bin/env python3
"""Source contract and, with --integrated, native KAMI Smart Sort folder grouping."""
import argparse, ast
from pathlib import Path
import xml.etree.ElementTree as ET
R=Path(__file__).resolve().parents[2]
src=(R/"kami/inventory/apply-brand-folder-sort.py").read_text(encoding="utf-8")
ast.parse(src)
for term in ("BRAND_FOLDERS = 3","kami_preview_brand_folders","kami_folder_brand_key",
             "KamiSmartSortFolderPreview","gInventory.changeCategoryParent",
             "getPreferredType() != LLFolderType::FT_NONE",
             "getParentUUID() != root_id","KAMI_SORT_MAX_ITEMS_PER_RUN",
             "if (++inspected > 500)"):
    assert term in src,term
def integrated():
    c=(R/"indra/newview/llpanelmaininventory.cpp").read_text(encoding="utf-8-sig")
    for term in ("BRAND_FOLDERS = 3", "kami_preview_brand_folders()",
                 "void kami_move_brand_folders(", "kami_brand_group_label(",
                 "gInventory.changeCategoryParent(folder, dest_id, false)",
                 "if (mode == KamiSortMode::BRAND_FOLDERS)",
                 "kami_smart_sort_key(item, mode)"):
        assert term in c,term
    x=R/"indra/newview/skins/default/xui/en"
    n=ET.parse(x/"notifications.xml").getroot()
    mode=[z for z in n.iter("notification") if z.get("name")=="KamiSmartSortChooseMode"]
    preview=[z for z in n.iter("notification") if z.get("name")=="KamiSmartSortFolderPreview"]
    assert len(mode)==len(preview)==1
    opts=[z.get("index") for z in mode[0].iter("button")]
    assert opts==["0","1","2","3","4"],opts
    assert "Original folder" in (mode[0].text or "")
    for name in ("menu_inventory_gear_default.xml","panel_main_inventory.xml"):
        ET.parse(x/name)
    print("PASS Brand Folder Sort: source code, preview, protected root move, four modes")
p=argparse.ArgumentParser();p.add_argument("--integrated",action="store_true")
args=p.parse_args()
print("PASS Brand Folder Sort patch source syntax and safety anchors")
if args.integrated:integrated()
else: print("Compile and in-world inventory synchronization not yet verified.")
