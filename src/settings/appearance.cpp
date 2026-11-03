#include "kami/appearance.hpp"
#include <windows.h>

namespace kami::settings {
namespace {
constexpr wchar_t kPath[] = L"Software\\KAMI\\OriginalCore";
constexpr wchar_t kValue[] = L"LightAppearance";
}
bool light_appearance() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,kPath,0,KEY_QUERY_VALUE,&key) != ERROR_SUCCESS)
        return false;
    DWORD value=0;
    DWORD length=sizeof(value);
    DWORD type=0;
    const auto result=RegQueryValueExW(key,kValue,nullptr,&type,
                                      reinterpret_cast<BYTE*>(&value),&length);
    RegCloseKey(key);
    return result==ERROR_SUCCESS && type==REG_DWORD &&
           length==sizeof(value) && value!=0;
}
void set_light_appearance(bool enabled) {
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,kPath,0,nullptr,0,
                       KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return;
    DWORD value=enabled?1U:0U;
    RegSetValueExW(key,kValue,0,REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value),sizeof(value));
    RegCloseKey(key);
}
}
