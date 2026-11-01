#!/usr/bin/env python3
"""Install preview-first KAMI Smart Sort DEMO Cleaner after Brand Folder Sort.

Moves *clearly named* DEMO folders/items to SL Trash only after confirmation.
Never permanently deletes assets. Scans a bounded number of already loaded
normal My Inventory folders; never scans system folders or hidden inventory.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
C=R/"indra/newview/llpanelmaininventory.cpp"
N=R/"indra/newview/skins/default/xui/en/notifications.xml"
M=R/"indra/newview/skins/default/xui/en/menu_inventory_gear_default.xml"
P=R/"indra/newview/skins/default/xui/en/panel_main_inventory.xml"

def replace_once(p,old,new):
    src=p.read_text(encoding="utf-8-sig")
    count=src.count(old)
    if count!=1:
        raise RuntimeError(f"KAMI Demo Cleaner source changed: {p}: {count} matches for {old[:95]!r}")
    p.write_text(src.replace(old,new,1),encoding="utf-8")

replace_once(C,"    BRAND_FOLDERS = 3\n};",
             "    BRAND_FOLDERS = 3,\n    DEMO_CLEANUP = 4\n};")
replace_once(C,'        case KamiSortMode::BRAND_FOLDERS: return "Brand Folders";',
             '        case KamiSortMode::BRAND_FOLDERS: return "Brand Folders";\n'
             '        case KamiSortMode::DEMO_CLEANUP: return "Demo Cleanup";')

extra=r"""
// KAMI Smart Sort: optional DEMO Cleaner. Only a standalone "demo" token
// qualifies (case-insensitive). E.g. [Brand] Demo Hair qualifies, but
// Demolition and "demonstration" do not. No content/asset inspection.
struct KamiDemoCandidate
{
    LLUUID id;
    LLUUID original_parent;
    bool folder;
    std::string display_name;
};
using KamiDemoPlan = std::vector<KamiDemoCandidate>;

bool kami_name_is_demo(const std::string& original)
{
    std::string name = original;
    LLStringUtil::toLower(name);
    size_t at = 0;
    while ((at = name.find("demo", at)) != std::string::npos)
    {
        const size_t end = at + 4;
        const bool before = at == 0 ||
            !std::isalnum(static_cast<unsigned char>(name[at - 1]));
        const bool after = end == name.size() ||
            !std::isalnum(static_cast<unsigned char>(name[end]));
        if (before && after) return true;
        ++at;
    }
    return false;
}

void kami_move_demo_candidates_to_trash(const LLUUID& root_id,
                                       const std::shared_ptr<KamiDemoPlan>& plan)
{
    if (gInventory.getRootFolderID() != root_id ||
        !gInventory.isInventoryUsable()) return;
    const LLUUID trash = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
    if (trash.isNull() || !gInventory.getCategory(trash)) return;
    for (const KamiDemoCandidate& entry : *plan)
    {
        // Keep move safeguards even when the user edits inventory while
        // the confirmation prompt is open.
        if (!gInventory.getCategory(entry.original_parent) ||
            !(entry.original_parent == root_id ||
              gInventory.isObjectDescendentOf(entry.original_parent, root_id)))
            continue;
        if (entry.folder)
        {
            LLViewerInventoryCategory* cat = gInventory.getCategory(entry.id);
            if (!cat || cat->getParentUUID() != entry.original_parent ||
                cat->getPreferredType() != LLFolderType::FT_NONE ||
                !kami_name_is_demo(cat->getName()))
                continue;
            gInventory.changeCategoryParent(cat, trash, false);
        }
        else
        {
            LLViewerInventoryItem* item = gInventory.getItem(entry.id);
            if (!item || item->getParentUUID() != entry.original_parent ||
                item->getIsLinkType() || !kami_name_is_demo(item->getName()))
                continue;
            gInventory.changeItemParent(item, trash, false);
        }
    }
}

void kami_preview_demo_cleanup()
{
    const LLUUID root_id = gInventory.getRootFolderID();
    const LLUUID trash = gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH);
    if (root_id.isNull() || trash.isNull() ||
        !gInventory.isInventoryUsable() || !gInventory.getCategory(trash) ||
        !gInventory.isCategoryComplete(root_id))
    {
        LLSD args;
        args["MESSAGE"] = "KAMI Demo Cleaner: inventory or Trash is still loading. No changes were made. Try again when loaded.";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    auto plan = std::make_shared<KamiDemoPlan>();
    struct Scope { LLUUID folder; U32 depth; };
    std::vector<Scope> scopes;
    scopes.push_back({root_id, 0});
    size_t cursor = 0;
    size_t inspected = 0;
    size_t skipped_incomplete = 0;
    // Avoid long viewer stalls and unbounded traversal. No background process.
    constexpr size_t KAMI_DEMO_MAX_FOLDERS = 240;
    constexpr size_t KAMI_DEMO_MAX_SCANNED = 1500;
    constexpr size_t KAMI_DEMO_MAX_MOVES = 60;
    constexpr U32 KAMI_DEMO_MAX_DEPTH = 5;
    while (cursor < scopes.size() &&
           cursor < KAMI_DEMO_MAX_FOLDERS &&
           inspected < KAMI_DEMO_MAX_SCANNED &&
           plan->size() < KAMI_DEMO_MAX_MOVES)
    {
        const Scope scope = scopes[cursor++];
        if (!gInventory.isCategoryComplete(scope.folder))
        {
            ++skipped_incomplete;
            continue;
        }
        LLInventoryModel::cat_array_t* folders = nullptr;
        LLInventoryModel::item_array_t* items = nullptr;
        gInventory.getDirectDescendentsOf(scope.folder, folders, items);
        if (folders)
        {
            for (const auto& cat : *folders)
            {
                if (++inspected > KAMI_DEMO_MAX_SCANNED ||
                    plan->size() >= KAMI_DEMO_MAX_MOVES) break;
                if (!cat || cat->getPreferredType() != LLFolderType::FT_NONE)
                    continue; // no system, Trash, Current Outfit etc.
                const LLUUID id = cat->getUUID();
                if (id == trash) continue;
                if (kami_name_is_demo(cat->getName()))
                {
                    plan->push_back({id, scope.folder, true, cat->getName()});
                    // A demo folder is moved whole: don't double-move children.
                }
                else if (scope.depth < KAMI_DEMO_MAX_DEPTH &&
                         scopes.size() < KAMI_DEMO_MAX_FOLDERS)
                {
                    scopes.push_back({id, scope.depth + 1});
                }
            }
        }
        if (items && inspected < KAMI_DEMO_MAX_SCANNED &&
            plan->size() < KAMI_DEMO_MAX_MOVES)
        {
            for (const auto& item : *items)
            {
                if (++inspected > KAMI_DEMO_MAX_SCANNED ||
                    plan->size() >= KAMI_DEMO_MAX_MOVES) break;
                if (!item || item->getIsLinkType() ||
                    !kami_name_is_demo(item->getName())) continue;
                plan->push_back({item->getUUID(), scope.folder, false,
                                 item->getName()});
            }
        }
    }

    if (plan->empty())
    {
        LLSD args;
        args["MESSAGE"] = "KAMI Demo Cleaner: No clear DEMO names found among the loaded folders and items scanned. Nothing moved.";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    std::ostringstream details;
    size_t folder_count = 0;
    size_t item_count = 0;
    size_t shown = 0;
    for (const KamiDemoCandidate& entry : *plan)
    {
        if (entry.folder) ++folder_count;
        else ++item_count;
        if (shown++ < 18)
        {
            details << "\n" << (entry.folder ? "Folder: " : "Item: ")
                    << entry.display_name.substr(0, 70);
        }
    }
    if (plan->size() > 18) details << "\n...and additional DEMO matches.";
    LLSD args;
    args["COUNT"] = static_cast<S32>(plan->size());
    args["FOLDERS"] = static_cast<S32>(folder_count);
    args["ITEMS"] = static_cast<S32>(item_count);
    args["DETAILS"] = details.str();
    args["LIMITATION"] = llformat(
        "Scan limits: %u folders / %u entries / %u levels, 60 moves per run. %u folders skipped because not fully loaded.",
        (U32)KAMI_DEMO_MAX_FOLDERS, (U32)KAMI_DEMO_MAX_SCANNED,
        (U32)KAMI_DEMO_MAX_DEPTH, (U32)skipped_incomplete);

    LLNotificationsUtil::add("KamiSmartSortDemoCleanupPreview", args, LLSD(),
        [root_id, plan](const LLSD& notification, const LLSD& response) -> bool
        {
            // This never runs unless the user explicitly confirms.
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                kami_move_demo_candidates_to_trash(root_id, plan);
            return false;
        });
}
"""
replace_once(C,"} // anonymous namespace\n\nstatic LLPanelInjector<LLPanelMainInventory>",
             extra+"} // anonymous namespace\n\nstatic LLPanelInjector<LLPanelMainInventory>")
replace_once(C,"selected <= static_cast<S32>(KamiSortMode::BRAND_FOLDERS)",
             "selected <= static_cast<S32>(KamiSortMode::DEMO_CLEANUP)")
replace_once(C,"selected_mode > static_cast<S32>(KamiSortMode::BRAND_FOLDERS)) return;",
             "selected_mode > static_cast<S32>(KamiSortMode::DEMO_CLEANUP)) return;")
replace_once(C,
"""    if (mode == KamiSortMode::BRAND_FOLDERS)
    {
        kami_preview_brand_folders();
        return;
    }""",
"""    if (mode == KamiSortMode::BRAND_FOLDERS)
    {
        kami_preview_brand_folders();
        return;
    }
    if (mode == KamiSortMode::DEMO_CLEANUP)
    {
        kami_preview_demo_cleanup();
        return;
    }""")
replace_once(C,
'''    if (command_name == "kami_smart_sort")
    {
        onKamiSmartSort();
        return;
    }''',
'''    if (command_name == "kami_smart_sort")
    {
        onKamiSmartSort();
        return;
    }
    if (command_name == "kami_demo_cleanup")
    {
        runKamiSmartSort(static_cast<S32>(KamiSortMode::DEMO_CLEANUP));
        return;
    }''')

# Do not match indentation in notifications; other patches reflow XML.
tree=ET.parse(N)
root=tree.getroot()
modes=[e for e in root.iter("notification") if e.get("name")=="KamiSmartSortChooseMode"]
assert len(modes)==1
form=modes[0].find("form")
buttons=list(form.iter("button"))
assert [x.get("index") for x in buttons]==["0","1","2","3","4"]
assert buttons[3].get("name")=="ByBrandFolders" and buttons[4].get("name")=="Cancel"
buttons[4].set("index","5")
form.insert(list(form).index(buttons[4]),ET.Element("button",
    {"index":"4","name":"ByDemoCleanup","text":"Demo Deleter"}))
modes[0].text=(modes[0].text or "").replace(
    "Every move requires a separate preview and confirmation.",
    "Demo Deleter: preview clearly named DEMO items and folders before moving them to Trash.\n\nEvery move requires a separate preview and confirmation.")
assert "Demo Deleter" in modes[0].text
new=ET.Element("notification",{"icon":"alertmodal.tga",
    "name":"KamiSmartSortDemoCleanupPreview","type":"alertmodal"})
new.text="""KAMI Smart Sort - DEMO DELETER

Found [COUNT] DEMO candidates: [FOLDERS] folders and [ITEMS] loose items.

Move these to the Second Life Trash (NOT permanent deletion)?
[DETAILS]

[LIMITATION]

Only separate 'DEMO' words in names are recognized. A DEMO folder may contain other items:
CHECK THE LIST CAREFULLY before approving. No-copy items in an identified
demo folder may also move with that folder. You can restore them from Trash
until Trash is emptied. Never empty Trash automatically.
    """
ET.SubElement(new,"usetemplate",{"name":"okcancelbuttons",
    "notext":"Keep All","yestext":"Move DEMOs to Trash"})
other=[e for e in root.iter("notification") if e.get("name")=="KamiSmartSortFolderPreview"]
assert len(other)==1
root.insert(list(root).index(other[0]),new)
tree.write(N,encoding="utf-8",xml_declaration=True)

menu=ET.parse(M)
gear=menu.getroot()
items=[e for e in gear if e.tag=="menu_item_call" and e.get("name")=="kami_smart_sort"]
assert len(items)==1
idx=list(gear).index(items[0])
act=ET.Element("menu_item_call",
               {"label":"KAMI Demo Deleter (preview first)...",
                "layout":"topleft","name":"kami_demo_cleanup"})
ET.SubElement(act,"on_click",
              {"function":"Inventory.GearDefault.Custom.Action",
               "parameter":"kami_demo_cleanup"})
gear.insert(idx+1,act)
menu.write(M,encoding="utf-8",xml_declaration=True)

replace_once(P,
"Group existing [Store] product folders or sort loose items. Preview and approve before moving.",
"Group [Store] product folders, sort loose items or preview DEMO cleanup; confirm all changes.")
for path in (N,M,P):
    ET.parse(path)
print("PASS Demo Deleter integrated: bounded loaded-inventory scan, confirm-before-Trash, no permanent deletion")
