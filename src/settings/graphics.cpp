#include "kami/graphics/settings.hpp"
#include <windows.h>

namespace kami::graphics {
namespace {
constexpr wchar_t kPath[]=L"Software\\KAMI\\OriginalCore\\Graphics";
DWORD read(HKEY key,const wchar_t* name,DWORD fallback) {
    DWORD result=fallback,bytes=sizeof(result),type=0;
    if(RegQueryValueExW(key,name,nullptr,&type,
                        reinterpret_cast<BYTE*>(&result),&bytes)!=ERROR_SUCCESS ||
       type!=REG_DWORD || bytes!=sizeof(result)) return fallback;
    return result;
}
void write(HKEY key,const wchar_t* name,DWORD number) {
    (void)RegSetValueExW(key,name,0,REG_DWORD,
                         reinterpret_cast<const BYTE*>(&number),sizeof(number));
}
}
Settings normalize(Settings value) noexcept {
    const auto preset=static_cast<std::uint32_t>(value.preset);
    if(preset>2) value.preset=Preset::Balanced;
    switch(value.render_scale_percent) {
        case 50: case 75: case 100: break;
        default: value.render_scale_percent=100; break;
    }
    switch(value.fps_limit) {
        case 0: case 30: case 60: case 120: case 144: break;
        default: value.fps_limit=60; break;
    }
    return value;
}
Settings load() {
    Settings settings{};
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,kPath,0,KEY_READ,&key)!=ERROR_SUCCESS)
        return settings;
    settings.preset=static_cast<Preset>(read(key,L"Preset",1));
    settings.render_scale_percent=read(key,L"RenderScalePercent",100);
    settings.fps_limit=read(key,L"FrameLimit",60);
    settings.vsync=read(key,L"VSync",1)!=0;
    RegCloseKey(key);
    return normalize(settings);
}
void save(Settings settings) noexcept {
    settings=normalize(settings);
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,kPath,0,nullptr,0,
                       KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return;
    write(key,L"Preset",static_cast<DWORD>(settings.preset));
    write(key,L"RenderScalePercent",settings.render_scale_percent);
    write(key,L"FrameLimit",settings.fps_limit);
    write(key,L"VSync",settings.vsync?1U:0U);
    RegCloseKey(key);
}
const wchar_t* preset_label(Preset preset) noexcept {
    switch(preset) {
        case Preset::Performance: return L"PERFORMANCE  /  16x16 TERRAIN";
        case Preset::Balanced: return L"BALANCED  /  32x32 TERRAIN";
        case Preset::Quality: return L"QUALITY  /  64x64 TERRAIN";
    }
    return L"BALANCED";
}
}
