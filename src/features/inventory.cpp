#include "kami/features/inventory.hpp"
#include <algorithm>
#include <cwctype>

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
