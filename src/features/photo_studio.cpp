#include "kami/features/photo_studio.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <limits>

namespace kami::photo {
namespace {
constexpr wchar_t kPath[]=L"Software\\KAMI\\OriginalCore\\Photo";
DWORD read(HKEY key,const wchar_t* name,DWORD fallback) noexcept {
    DWORD value=fallback,bytes=sizeof(value),type=0;
    if(RegQueryValueExW(key,name,nullptr,&type,
                        reinterpret_cast<BYTE*>(&value),&bytes)!=ERROR_SUCCESS||
       type!=REG_DWORD||bytes!=sizeof(value))return fallback;
    return value;
}
void write(HKEY key,const wchar_t* name,DWORD value) noexcept {
    (void)RegSetValueExW(key,name,0,REG_DWORD,
                         reinterpret_cast<const BYTE*>(&value),sizeof(value));
}
} // namespace

Settings normalize(Settings value) noexcept {
    if(static_cast<unsigned>(value.preset)>3)
        value.preset=Preset::Cinematic;
    value.exposure_tenths=std::clamp(value.exposure_tenths,-20,20);
    value.contrast_percent=std::clamp(value.contrast_percent,50U,200U);
    value.saturation_percent=std::min(value.saturation_percent,200U);
    value.vignette_percent=std::min(value.vignette_percent,100U);
    return value;
}
Settings for_preset(Preset preset) noexcept {
    switch(preset) {
    case Preset::Cinematic: return {preset,-2,118,96,22,true};
    case Preset::Natural: return {preset,0,100,100,0,true};
    case Preset::Noir: return {preset,-1,125,0,30,true};
    case Preset::NightStudio: return {preset,-5,132,82,40,true};
    }
    return for_preset(Preset::Cinematic);
}
const wchar_t* preset_name(Preset preset) noexcept {
    switch(preset) {
    case Preset::Cinematic: return L"CINEMATIC";
    case Preset::Natural: return L"NATURAL";
    case Preset::Noir: return L"MONOCHROME";
    case Preset::NightStudio: return L"NIGHT STUDIO";
    }
    return L"CINEMATIC";
}
Settings load() {
    HKEY key=nullptr;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,kPath,0,KEY_READ,&key)!=ERROR_SUCCESS)
        return for_preset(Preset::Cinematic);
    Settings value;
    value.preset=static_cast<Preset>(read(key,L"Preset",0));
    value.exposure_tenths=static_cast<int>(
        static_cast<std::int32_t>(read(key,L"ExposureTenths",static_cast<DWORD>(-2))));
    value.contrast_percent=read(key,L"ContrastPercent",118);
    value.saturation_percent=read(key,L"SaturationPercent",96);
    value.vignette_percent=read(key,L"VignettePercent",22);
    value.enabled=read(key,L"LiveGrading",1)!=0;
    RegCloseKey(key);
    return normalize(value);
}
void save(Settings value) noexcept {
    value=normalize(value);
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,kPath,0,nullptr,0,
                       KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)return;
    write(key,L"Preset",static_cast<DWORD>(value.preset));
    write(key,L"ExposureTenths",static_cast<DWORD>(value.exposure_tenths));
    write(key,L"ContrastPercent",value.contrast_percent);
    write(key,L"SaturationPercent",value.saturation_percent);
    write(key,L"VignettePercent",value.vignette_percent);
    write(key,L"LiveGrading",value.enabled?1U:0U);
    RegCloseKey(key);
}
bool write_bgra_png(const std::wstring& path,
                    const std::vector<std::uint8_t>& pixels,
                    unsigned width,unsigned height) noexcept {
    if(path.empty()||width==0||height==0||
       width>16384||height>16384||pixels.size()!=
       static_cast<std::size_t>(width)*height*4U||
       pixels.size()>std::numeric_limits<UINT>::max())return false;
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(factory.GetAddressOf()))))return false;
    ComPtr<IWICStream> stream;
    if(FAILED(factory->CreateStream(stream.GetAddressOf()))||
       FAILED(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE)))return false;
    ComPtr<IWICBitmapEncoder> encoder;
    if(FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,
                                     encoder.GetAddressOf()))||
       FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)))return false;
    ComPtr<IWICBitmapFrameEncode> frame;
    ComPtr<IPropertyBag2> options;
    if(FAILED(encoder->CreateNewFrame(frame.GetAddressOf(),options.GetAddressOf()))||
       FAILED(frame->Initialize(options.Get()))||
       FAILED(frame->SetSize(width,height)))return false;
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&format))||
       !IsEqualGUID(format,GUID_WICPixelFormat32bppBGRA))return false;
    if(FAILED(frame->WritePixels(height,width*4U,
              static_cast<UINT>(pixels.size()),
              const_cast<BYTE*>(pixels.data()))) ||
       FAILED(frame->Commit()) || FAILED(encoder->Commit()))return false;
    return true;
}
} // namespace kami::photo
