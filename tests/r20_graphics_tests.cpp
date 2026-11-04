#include "kami/world/shaders.hpp"
#include "kami/world/terrain_palette.hpp"
#include "kami/world/scene.hpp"
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cmath>
#include <cstring>
#include <limits>
#include <string_view>

namespace {
bool compile(const char* entry,const char* profile){
    Microsoft::WRL::ComPtr<ID3DBlob> shader,diagnostics;
    const auto result=D3DCompile(
        kami::world::kSceneShader,
        sizeof(kami::world::kSceneShader)-1,
        "KAMI_R20_visuals.hlsl",nullptr,nullptr,
        entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,
        shader.GetAddressOf(),diagnostics.GetAddressOf());
    return SUCCEEDED(result)&&shader&&shader->GetBufferSize()>0;
}
bool color_valid(kami::world::terraincolor::RGB c){
    return std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b)&&
           c.r>=0.f&&c.r<=1.f&&c.g>=0.f&&c.g<=1.f&&
           c.b>=0.f&&c.b<=1.f;
}
}
int main(){
    // Compile all shader entrypoints against Windows native D3DCompiler.
    if(!compile("sky_vertex_main","vs_5_0"))return 1;
    if(!compile("sky_pixel_main","ps_5_0"))return 2;
    if(!compile("vertex_main","vs_5_0"))return 3;
    if(!compile("pixel_main","ps_5_0"))return 4;
    if(!compile("shadow_vertex_main","vs_5_0"))return 5;

    using namespace kami::world;
    const auto grass=terraincolor::sample(31.f,27.f,40.f,1.f,20.f);
    const auto beach=terraincolor::sample(31.f,20.1f,40.f,1.f,20.f);
    const auto rock=terraincolor::sample(31.f,27.f,40.f,.2f,20.f);
    if(!color_valid(grass)||!color_valid(beach)||!color_valid(rock))
        return 6;
    if(!(beach.r>grass.r)&&!(beach.g>grass.g))return 7;
    if(!(rock.b>grass.b))return 8;
    if(terraincolor::sample(31.f,27.f,40.f,1.f,20.f).g!=grass.g)
        return 9;
    const auto water=build_live_region_water_preview(-7.f,128.f,129.f);
    if(water.avatar_count!=0U||water.terrain_count!=6U||
       water.vertices.size()!=6U)return 10;
    float minx=999.f,maxx=-999.f,minz=999.f,maxz=-999.f;
    for(const auto& v:water.vertices){
        if(v.py!=-7.f||v.ny!=1.f||!std::isfinite(v.u)||
           !std::isfinite(v.v))return 11;
        minx=std::min(minx,v.px);maxx=std::max(maxx,v.px);
        minz=std::min(minz,v.pz);maxz=std::max(maxz,v.pz);
    }
    if(minx!=-128.f||maxx!=128.f||minz!=-129.f||maxz!=127.f)
        return 12;
    if(!build_live_region_water_preview(
        std::numeric_limits<float>::infinity(),128.f,129.f
        ).vertices.empty())return 13;
    return 0;
}
