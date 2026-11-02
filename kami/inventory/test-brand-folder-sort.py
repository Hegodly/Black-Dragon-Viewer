#!/usr/bin/env python3
"""Execute real generated parser/scope/move functions against inventory doubles."""
from pathlib import Path
import ast,json,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
src=(ROOT/'kami/inventory/apply-brand-folder-sort.py').read_text()
extra=next(n.value.value for n in ast.parse(src).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='extra' for t in n.targets))
original=(ROOT/'indra/newview/llpanelmaininventory.cpp').read_text(encoding='utf-8-sig')
parser=original[original.index('std::string kami_store_prefix('):original.index('std::string kami_smart_sort_key(')]
actual=extra[:extra.index('void kami_apply_brand_folders(')]
STUBS=r'''
#include <algorithm>
#include <cassert>
#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>
using S32=int;
struct LLUUID {
 std::string id;
 LLUUID()=default;LLUUID(const char* s):id(s){}
 bool isNull()const{return id.empty();}bool notNull()const{return !isNull();}
 bool operator==(const LLUUID& r)const{return id==r.id;}
 bool operator!=(const LLUUID& r)const{return !(*this==r);}
 bool operator<(const LLUUID& r)const{return id<r.id;}
 static const LLUUID null;
};const LLUUID LLUUID::null;
struct LLStringUtil {
 static void trim(std::string& s){auto a=s.find_first_not_of(" \t\r\n");if(a==s.npos){s.clear();return;}s=s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
 static void toLower(std::string& s){for(auto& c:s)c=std::tolower((unsigned char)c);}
 static void toUpper(std::string& s){for(auto& c:s)c=std::toupper((unsigned char)c);}
};
namespace LLFolderType {enum EType{FT_NONE,FT_OBJECT,FT_CLOTHING,FT_BODYPART,FT_ROOT,FT_TRASH,FT_OUTFIT,FT_LIBRARY};}
struct LLViewerInventoryCategory {
 LLUUID id,parent;LLFolderType::EType type;std::string name;
 auto getPreferredType()const{return type;}auto getUUID()const{return id;}
 auto getParentUUID()const{return parent;}std::string getName()const{return name;}
};
struct LLInventoryModel {
 using cat_array_t=std::vector<std::shared_ptr<LLViewerInventoryCategory>>;using item_array_t=std::vector<int>;
 std::map<LLUUID,std::shared_ptr<LLViewerInventoryCategory>> cats;LLUUID root="root";bool usable=true;int moves=0;cat_array_t children;item_array_t items;
 LLUUID getRootFolderID()const{return root;}bool isInventoryUsable()const{return usable;}
 LLViewerInventoryCategory* getCategory(const LLUUID& id){auto i=cats.find(id);return i==cats.end()?nullptr:i->second.get();}
 void add(LLUUID id,LLUUID parent,LLFolderType::EType type,std::string name){cats[id]=std::make_shared<LLViewerInventoryCategory>(LLViewerInventoryCategory{id,parent,type,name});}
 void getDirectDescendentsOf(LLUUID id,cat_array_t*& out,item_array_t*& it){children.clear();for(auto& p:cats)if(p.second->parent==id)children.push_back(p.second);out=&children;it=&items;}
 void changeCategoryParent(LLViewerInventoryCategory* cat,LLUUID parent,bool){cat->parent=parent;++moves;}
} gInventory;
'''
TEST=r'''
int main(){
 assert(kami_folder_brand_key("[Hayabusa] Reed FATPACK UNPACKER")=="hayabusa");
 assert(kami_folder_brand_key("-Pretty Liars- Real Kisser")=="pretty liars");
 assert(kami_folder_brand_key(".:LW:. Eli Evo X Bento Mesh Head 1.3")=="lw");
 assert(kami_folder_brand_key("::LV:: Eli Evo X Bento Mesh Head")=="lv");
 assert(kami_folder_brand_key("Store - Product")=="store");
 assert(kami_folder_brand_key("[LW]").empty());
 assert(kami_folder_brand_key("-Pretty Liars-").empty());
 assert(kami_folder_brand_key("[DEMO] Product").empty());
 assert(kami_folder_brand_key("The P V3.0 by ASA Studios & Session Skins").empty());
 assert(kami_folder_brand_key("[Unclosed Product").empty());
 assert(kami_brand_group_label("pretty liars")=="[PRETTY LIARS]");
 using namespace LLFolderType;
 gInventory.add("root",{},FT_ROOT,"My Inventory");
 gInventory.add("objects","root",FT_OBJECT,"Objects");
 gInventory.add("source","objects",FT_NONE,"Unsorted purchases");
 gInventory.add("trash","root",FT_TRASH,"Trash");gInventory.add("bad","trash",FT_NONE,"Deleted purchases");
 gInventory.add("outfit","root",FT_OUTFIT,"Outfit");
 gInventory.add("library",{},FT_LIBRARY,"Library");
 assert(kami_brand_scope_valid("source") && kami_brand_scope_valid("objects") && kami_brand_scope_valid("root"));
 assert(!kami_brand_scope_valid("bad") && !kami_brand_scope_valid("outfit") && !kami_brand_scope_valid("library"));
 gInventory.add("group","source",FT_NONE,"[PRETTY LIARS]");
 gInventory.add("product","source",FT_NONE,"-Pretty Liars- Real Kisser");
 gInventory.add("nested","product",FT_NONE,"Accessories");
 gInventory.add("unrelated","objects",FT_NONE,"-Pretty Liars- Other product");
 assert(kami_find_brand_group("source","pretty liars")==LLUUID("group"));
 kami_move_brand_folders("source","group","pretty liars",{LLUUID("product"),LLUUID("unrelated")});
 assert(gInventory.moves==1 && gInventory.getCategory("product")->parent==LLUUID("group"));
 assert(gInventory.getCategory("nested")->parent==LLUUID("product"));
 assert(gInventory.getCategory("unrelated")->parent==LLUUID("objects"));
 kami_move_brand_folders("source","group","pretty liars",{LLUUID("product")});assert(gInventory.moves==1);
 gInventory.getCategory("source")->parent="trash";
 kami_move_brand_folders("source","group","pretty liars",{LLUUID("unrelated")});assert(gInventory.moves==1);
 std::cout<<"PASS brand formats, existing-group reuse, selected-folder scope, protected trees, preserved children and idempotent moves\n";
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'sort.cpp';p.write_text(STUBS+parser+actual+TEST)
 binary=Path(d)/'sort';subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p),'-o',str(binary)],check=True);subprocess.run([str(binary)],check=True)
