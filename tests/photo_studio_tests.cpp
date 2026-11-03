#include "kami/features/photo_studio.hpp"
#include <windows.h>
#include <objbase.h>
#include <filesystem>
#include <fstream>
#include <iterator>
int main() {
    using namespace kami::photo;
    const auto cinematic=for_preset(Preset::Cinematic);
    if(cinematic.exposure_tenths!=-2 ||
       cinematic.contrast_percent!=118||!cinematic.enabled)return 1;
    const auto noir=for_preset(Preset::Noir);
    if(noir.saturation_percent!=0||noir.vignette_percent!=30)return 2;
    const auto invalid=normalize({static_cast<Preset>(500),-100,999,999,999,false});
    if(invalid.preset!=Preset::Cinematic||invalid.exposure_tenths!=-20||
       invalid.contrast_percent!=200||invalid.saturation_percent!=200||
       invalid.vignette_percent!=100)return 3;
    if(write_bgra_png(L"",{},0,0))return 4;
    const HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(init))return 5;
    wchar_t folder[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,folder)) {CoUninitialize();return 6;}
    std::filesystem::path out=std::filesystem::path(folder)/L"KAMI-photo-studio-test.png";
    const std::vector<std::uint8_t> testImage={
        0,0,255,255, 0,255,0,255,
        255,0,0,255, 255,255,255,255
    };
    if(!write_bgra_png(out.wstring(),testImage,2,2)){
        CoUninitialize();return 7;
    }
    std::ifstream file(out,std::ios::binary);
    const std::vector<char> bytes{std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>()};
    std::error_code ignored;
    std::filesystem::remove(out,ignored);
    CoUninitialize();
    if(bytes.size()<40)return 8;
    constexpr unsigned char signature[]={137,80,78,71,13,10,26,10};
    for(unsigned i=0;i<8;++i)
        if(static_cast<unsigned char>(bytes[i])!=signature[i])return 9;
    return 0;
}
