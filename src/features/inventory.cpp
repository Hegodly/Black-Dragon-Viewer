#include "kami/features/inventory.hpp"
#include <algorithm>
#include <cwctype>
#include <map>

namespace kami::inventory {
namespace {
std::wstring trim(std::wstring_view s) {
    while(!s.empty() && std::iswspace(s.front())) s.remove_prefix(1);
    while(!s.empty() && std::iswspace(s.back())) s.remove_suffix(1);
    return std::wstring(s);
}
std::wstring upper_ascii(std::wstring s) {
    for(wchar_t& c:s) if(c>=L'a'&&c<=L'z') c-=32;
    return s;
}
bool word_char(wchar_t c) noexcept {
    return std::iswalnum(c) != 0;
}
bool token(std::wstring_view text, std::wstring_view needle) noexcept {
    for(std::size_t pos=0;pos+needle.size()<=text.size();++pos) {
        bool same=true;
        for(std::size_t i=0;i<needle.size();++i)
            if(std::towupper(text[pos+i])!=needle[i]) {same=false;break;}
        if(same && (pos==0||!word_char(text[pos-1])) &&
           (pos+needle.size()==text.size()||
             !word_char(text[pos+needle.size()]))) return true;
    }
    return false;
}
} // namespace

std::vector<std::size_t> filter_entries(
    const std::vector<protocol::caps::InventoryEntry>& entries,
    std::string_view parent_folder_id,std::wstring_view query,SortMode sort,int asset_type_filter) {
    std::vector<std::size_t> result;
    const auto matches=[&](std::wstring_view value){
        if(query.empty())return true;
        for(std::size_t p=0;p+query.size()<=value.size();++p){
            bool equal=true;
            for(std::size_t i=0;i<query.size();++i)
                if(std::towlower(value[p+i])!=std::towlower(query[i])){equal=false;break;}
            if(equal)return true;
        }
        return false;
    };
    for(std::size_t i=0;i<entries.size();++i)
        if(entries[i].parent_id==parent_folder_id&&
           (asset_type_filter<0||entries[i].folder||entries[i].asset_type==asset_type_filter)&&
           (matches(entries[i].name)||matches(entries[i].description)))
            result.push_back(i);
    std::stable_sort(result.begin(),result.end(),[&](std::size_t a,std::size_t b){
        const auto& x=entries[a];const auto& y=entries[b];
        if(x.folder!=y.folder)return x.folder;
        if(sort==SortMode::Creator&&x.creator_id!=y.creator_id)return x.creator_id<y.creator_id;
        if(sort==SortMode::Type&&x.asset_type!=y.asset_type)return x.asset_type<y.asset_type;
        auto lx=x.name,ly=y.name;
        std::transform(lx.begin(),lx.end(),lx.begin(),[](wchar_t c){return std::towlower(c);});
        std::transform(ly.begin(),ly.end(),ly.begin(),[](wchar_t c){return std::towlower(c);});
        return lx<ly;
    });
    return result;
}
std::vector<CreatorGroup> group_entries_by_creator(
    const std::vector<protocol::caps::InventoryEntry>& entries,
    const std::vector<std::size_t>& indices) {
    std::map<std::wstring,std::vector<std::size_t>> groups;
    for(const auto i:indices){
        if(i>=entries.size()||entries[i].folder)continue;
        const auto& id=entries[i].creator_id;
        const std::wstring creator=id.empty()?L"[UNKNOWN CREATOR]":std::wstring(id.begin(),id.end());
        groups[creator].push_back(i);
    }
    std::vector<CreatorGroup> result;
    for(auto& group:groups)result.push_back({group.first,std::move(group.second)});
    return result;
}
std::wstring folder_for(const Item& item) {
    std::wstring_view name(item.name);
    if(name.size()>2 && name.front()==L'[') {
        const auto end=name.find(L']');
        if(end!=std::wstring_view::npos && end>1 && end<60) {
            auto brand=trim(name.substr(1,end-1));
            if(!brand.empty() && brand.find_first_of(L"\\/:*?\"<>|")==std::wstring::npos)
                return L"["+upper_ascii(brand)+L"]";
        }
    }
    const auto author=trim(item.creator);
    if(!author.empty() && author.size()<=96 &&
       author.find_first_of(L"\\/:*?\"<>|")==std::wstring::npos)
        return L"["+upper_ascii(author)+L"]";
    return L"[UNCATEGORIZED]";
}
bool is_demo_item(std::wstring_view name) noexcept {
    return token(name,L"DEMO") || token(name,L"TRIAL");
}
std::vector<MovePlan> preview_smart_sort(const std::vector<Item>& items) {
    std::vector<MovePlan> result;
    result.reserve(items.size());
    for(std::size_t i=0;i<items.size();++i) {
        if(items[i].is_folder) continue;
        result.push_back({i,folder_for(items[i])});
    }
    return result;
}
std::vector<std::size_t> preview_demo_deletions(const std::vector<Item>& items) {
    std::vector<std::size_t> result;
    for(std::size_t i=0;i<items.size();++i)
        if(!items[i].is_folder && is_demo_item(items[i].name)) result.push_back(i);
    return result;
}
} // namespace kami::inventory
