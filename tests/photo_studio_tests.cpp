#include "kami/features/photo_studio.hpp"
#include <cmath>
int main() {
    using namespace kami::photo;
    auto cinematic=for_preset(Preset::Cinematic);
    if(cinematic.exposure_tenths!=-2||
       cinematic.contrast_percent!=118||!cinematic.enabled)return 1;
    const auto noir=for_preset(Preset::Noir);
    if(noir.saturation_percent!=0||noir.vignette_percent!=30)return 2;
    auto invalid=normalize({static_cast<Preset>(500),-100,999,999,999,false});
    if(invalid.preset!=Preset::Cinematic||invalid.exposure_tenths!=-20||
       invalid.contrast_percent!=200||invalid.saturation_percent!=200||
       invalid.vignette_percent!=100)return 3;
    if(write_bgra_png(L"",{},0,0))return 4;
    return 0;
}
