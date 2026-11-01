#!/usr/bin/env python3
"""KAMI Smart Sort: group existing [Store] product folders, preview first.

Run on Windows source before compilation. Does not touch any SL inventory itself.
Only the user-confirmed native viewer command can create/move categories.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

R=Path(__file__).resolve().parents[2]
C=R/"indra/newview/llpanelmaininventory.cpp"
N=R/"indra/newview/skins/default/xui/en/notifications.xml"
M=R/"indra/newview/skins/default/xui/en/menu_inventory_gear_default.xml"
P=R/"indra/newview/skins/default/xui/en/panel_main_inventory.xml"

def replace_once(file,old,new):
    s=file.read_text(encoding="utf-8-sig")
    assert s.count(old)==1,(str(file),s.count(old),old[:100])
    file.write_text(s.replace(old,new,1),encoding="utf-8")

replace_once(C,"    STORE = 2\n};","    STORE = 2,\n    BRAND_FOLDERS = 3\n};")
replace_once(C,'        case KamiSortMode::STORE: return "Stores";',
             '        case KamiSortMode::STORE: return "Stores";\n        case KamiSortMode::BRAND_FOLDERS: return "Brand Folders";')

extra=r"""
// KAMI Smart Sort v2: entire top-level user folders organized by [Store] tags.
// No recursive scan, no creator guessing, no folder rename, no asset uploads.
struct KamiBrandFolderGroup
{
    std::string label;
    std::vector<LLUUID> ids;
};
using KamiBrandFolderPlan = std::map<std::string, KamiBrandFolderGroup>;

std::string kami_folder_brand_key(const std::string& raw)
{
    std::string name = raw;
    LLStringUtil::trim(name);
    // Only [Brand] Product; exact bracketed brand folders have no product.
    if (name.empty() || name[0] != '[') return "";
    return kami_store_prefix(name);
}

std::string kami_brand_group_label(const std::string& key)
{
    std::string name = key;
    LLStringUtil::toUpper(name);
    return "[" + name + "]";
}

LLUUID kami_find_brand_group(const LLUUID& root_id, const std::string& key)
{
    LLInventoryModel::cat_array_t* categories = nullptr;
    LLInventoryModel::item_array_t* items = nullptr;
    gInventory.getDirectDescendentsOf(root_id, categories, items);
    if (!categories) return LLUUID::null;
    for (const auto& category : *categories)
    {
        if (!category || category->getPreferredType() != LLFolderType::FT_NONE) continue;
        std::string label = category->getName();
        LLStringUtil::trim(label);
        LLStringUtil::toLower(label);
        if (label == "[" + key + "]") return category->getUUID();
    }
    return LLUUID::null;
}

void kami_move_brand_folders(const LLUUID& root_id, const LLUUID& dest_id,
                            const std::string& brand, const std::vector<LLUUID>& ids)
{
    if (gInventory.getRootFolderID() != root_id || !gInventory.isInventoryUsable()) return;
    LLViewerInventoryCategory* dest = gInventory.getCategory(dest_id);
    if (!dest || dest->getParentUUID() != root_id ||
        dest->getPreferredType() != LLFolderType::FT_NONE) return;
    std::string label = dest->getName();
    LLStringUtil::trim(label);
    LLStringUtil::toLower(label);
    if (label != "[" + brand + "]") return;

    for (const LLUUID& id : ids)
    {
        if (id == dest_id) continue;
        LLViewerInventoryCategory* folder = gInventory.getCategory(id);
        if (!folder || folder->getPreferredType() != LLFolderType::FT_NONE ||
            folder->getParentUUID() != root_id ||
            kami_folder_brand_key(folder->getName()) != brand) continue;
        // Move the existing folder, not its children individually.
        gInventory.changeCategoryParent(folder, dest_id, false);
    }
}

void kami_apply_brand_folders(const LLUUID& root_id,
                              const std::shared_ptr<KamiBrandFolderPlan>& plan)
{
    if (gInventory.getRootFolderID() != root_id ||
        !gInventory.isInventoryUsable() || !gInventory.isCategoryComplete(root_id))
        return;
    for (const auto& entry : *plan)
    {
        const std::string brand = entry.first;
        const KamiBrandFolderGroup group = entry.second;
        const LLUUID existing = kami_find_brand_group(root_id, brand);
        if (existing.notNull())
            kami_move_brand_folders(root_id, existing, brand, group.ids);
        else
            gInventory.createNewCategory(root_id, LLFolderType::FT_NONE, group.label,
                [root_id, brand, group](const LLUUID& created_id)
                {
                    if (created_id.notNull())
                        kami_move_brand_folders(root_id, created_id, brand, group.ids);
                });
    }
}

void kami_preview_brand_folders()
{
    const LLUUID root_id = gInventory.getRootFolderID();
    if (root_id.isNull() || !gInventory.isInventoryUsable() ||
        !gInventory.isCategoryComplete(root_id))
    {
        if (root_id.notNull()) gInventory.fetchDescendentsOf(root_id);
        LLSD args;
        args["MESSAGE"] = "KAMI Smart Sort: My Inventory is still loading. Retry after it finishes.";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    LLInventoryModel::cat_array_t* folders = nullptr;
    LLInventoryModel::item_array_t* items = nullptr;
    gInventory.getDirectDescendentsOf(root_id, folders, items);
    if (!folders) return;
    auto plan = std::make_shared<KamiBrandFolderPlan>();
    S32 skipped = 0;
    S32 limited = 0;
    size_t movable = 0;
    size_t inspected = 0;
    for (const auto& folder : *folders)
    {
        if (++inspected > 500) { limited = 1; break; }
        if (!folder || folder->getPreferredType() != LLFolderType::FT_NONE)
        {
            ++skipped;
            continue;
        }
        const std::string key = kami_folder_brand_key(folder->getName());
        if (key.empty() || movable >= KAMI_SORT_MAX_ITEMS_PER_RUN ||
            (!plan->count(key) && plan->size() >= KAMI_SORT_MAX_GROUPS_PER_RUN))
        {
            ++skipped;
            continue;
        }
        KamiBrandFolderGroup& group = (*plan)[key];
        if (group.label.empty()) group.label = kami_brand_group_label(key);
        group.ids.push_back(folder->getUUID());
        ++movable;
    }

    if (movable == 0)
    {
        LLSD args;
        args["MESSAGE"] = "KAMI Smart Sort: No matching top-level folders such as [Hayabusa] Reed FATPACK UNPACKER found. Nothing changed.";
        LLNotificationsUtil::add("GenericAlert", args);
        return;
    }

    std::ostringstream preview;
    for (const auto& entry : *plan)
    {
        const KamiBrandFolderGroup& group = entry.second;
        preview << "\n" << group.label << " (" << group.ids.size() << "): ";
        size_t shown = 0;
        for (const LLUUID& id : group.ids)
        {
            if (shown >= 2) break;
            const LLViewerInventoryCategory* folder = gInventory.getCategory(id);
            if (!folder) continue;
            if (shown++) preview << "; ";
            preview << folder->getName().substr(0, 46);
        }
        if (group.ids.size() > shown) preview << "; ...";
    }

    LLSD args;
    args["COUNT"] = static_cast<S32>(movable);
    args["SKIPPED"] = skipped;
    args["DETAILS"] = preview.str();
    args["LIMITATION"] = limited ? "Only the first 500 root-level folders were inspected." :
       "Only normal My Inventory folders with an explicit [Store] product prefix are eligible.";
    LLNotificationsUtil::add("KamiSmartSortFolderPreview", args, LLSD(),
        [root_id, plan](const LLSD& notification, const LLSD& response) -> bool
        {
            if (LLNotificationsUtil::getSelectedOption(notification, response) == 0)
                kami_apply_brand_folders(root_id, plan);
            return false;
        });
}
"""
replace_once(C,"} // anonymous namespace\n\nstatic LLPanelInjector<LLPanelMainInventory>",
             extra+"} // anonymous namespace\n\nstatic LLPanelInjector<LLPanelMainInventory>")
replace_once(C,"selected <= static_cast<S32>(KamiSortMode::STORE)",
             "selected <= static_cast<S32>(KamiSortMode::BRAND_FOLDERS)")
replace_once(C,"selected_mode > static_cast<S32>(KamiSortMode::STORE)) return;\n    const KamiSortMode mode = static_cast<KamiSortMode>(selected_mode);",
             "selected_mode > static_cast<S32>(KamiSortMode::BRAND_FOLDERS)) return;\n    const KamiSortMode mode = static_cast<KamiSortMode>(selected_mode);\n    if (mode == KamiSortMode::BRAND_FOLDERS)\n    {\n        kami_preview_brand_folders();\n        return;\n    }")
replace_once(N,"Choose how to organize loose items directly under My Inventory:\n\nCategory: item type and conservative name matches\nCreator: the item's creator ID\nStore: an inferred brand prefix in the item name\n\nNo inventory changes will happen until you review and confirm the next screen.",
"""Organize your existing inventory by one of these rules:

BRAND FOLDERS: Move [Store] Product folders under [STORE] parent folders.
Example: [Hayabusa] Reed FATPACK UNPACKER goes into [HAYABUSA].
Original folder names and folder contents are preserved.

Category: loose items by type
Creator: loose items by creator ID
Store Items: loose items by inferred name prefix

Every move requires a separate preview and confirmation.""")
# Patch notification structure semantically. Earlier workflow scripts may
# reindent XML, so exact newline/indentation matching here is unsafe.
tree=ET.parse(N)
root=tree.getroot()
modes=[x for x in root.iter("notification") if x.get("name")=="KamiSmartSortChooseMode"]
previews=[x for x in root.iter("notification") if x.get("name")=="KamiSmartSortPreview"]
assert len(modes)==len(previews)==1
form=modes[0].find("form")
buttons=list(form.iter("button"))
assert [b.get("index") for b in buttons]==["0","1","2","3"]
assert buttons[2].get("name")=="ByStore" and buttons[3].get("name")=="Cancel"
buttons[2].set("text","Store Items")
buttons[3].set("index","4")
form.insert(list(form).index(buttons[3]),
            ET.Element("button",{"index":"3","name":"ByBrandFolders",
                                 "text":"Brand Folders"}))
notice=ET.Element("notification",{"icon":"alertmodal.tga",
    "name":"KamiSmartSortFolderPreview","type":"alertmodal"})
notice.text="""KAMI Smart Sort - Brand Folders Preview

Move [COUNT] product folders into uppercase bracketed brand folders directly under My Inventory.

Proposed destination groups:
[DETAILS]

Skipped / unrecognized: [SKIPPED] folders.
[LIMITATION]

Only original folders move. Their names, nested folders and items are preserved.
System folders and anything outside the top level are untouched.
Nothing is deleted. Confirm before editing your SL inventory.
    """
ET.SubElement(notice,"usetemplate",{"name":"okcancelbuttons",
                                    "notext":"Cancel","yestext":"Organize Folders"})
root.insert(list(root).index(previews[0]),notice)
tree.write(N,encoding="utf-8",xml_declaration=True)
replace_once(M,"Kami Smart Sort (Category / Creator / Store)...","Kami Smart Sort (Brand Folders / Items)...")
replace_once(P,"Choose Smart Sort by Category, Creator or Store; preview and approve before any move.",
             "Group existing [Store] product folders or sort loose items. Preview and approve before moving.")
for file in (N,M,P): ET.parse(file)
print("PASS KAMI Smart Sort brand folder patch installed; all legacy item modes preserved.")
