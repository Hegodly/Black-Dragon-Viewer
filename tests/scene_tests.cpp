#include "kami/world/scene.hpp"
#include <limits>
#include <cmath>
#include <cstddef>
#include <algorithm>

int main() {
    const auto mesh=kami::world::build_test_scene();
    if(mesh.terrain_count!=32U*32U*6U) return 1;
    if(mesh.avatar_count!=6U*36U+5U*10U*10U*3U) return 2;
    if(mesh.vertices.size()!=mesh.terrain_count+mesh.avatar_count) return 3;
    unsigned regions[4]{};
    for(const auto& v:mesh.vertices) {
        if(v.body_region<0.f||v.body_region>3.f||!std::isfinite(v.body_region))
            return 13;
        ++regions[static_cast<unsigned>(v.body_region)];
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||!std::isfinite(v.pz)) return 4;
        if(!std::isfinite(v.nx)||!std::isfinite(v.ny)||!std::isfinite(v.nz)) return 5;
        if(!std::isfinite(v.r)||!std::isfinite(v.g)||!std::isfinite(v.b)) return 6;
        const float n=std::sqrt(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz);
        if(n<.98f||n>1.02f) return 7;
        if(v.r<0.f||v.r>1.f||v.g<0.f||v.g>1.f||v.b<0.f||v.b>1.f) return 8;
        if(!std::isfinite(v.metallic)||!std::isfinite(v.roughness)) return 10;
        if(v.metallic<0.f||v.metallic>1.f||v.roughness<.06f||v.roughness>1.f) return 11;
        if(!std::isfinite(v.u)||!std::isfinite(v.v)) return 12;
    }
    if(regions[0]==0||regions[1]==0||regions[2]==0||regions[3]==0)
        return 14;
    const auto first=mesh.vertices.front();
    if(first.px<-15.f||first.px>15.f||first.pz<-15.f||first.pz>15.f) return 9;
    const auto live=build_live_water_preview(-3.5f);
    if(live.vertices.size()!=6||live.terrain_count!=6||
       live.avatar_count!=0)return 41;
    for(const auto& v:live.vertices){
        if(v.py!=-3.5f||v.ny!=1.f||v.bone_indices!=0||
           v.bone_weights!=0)return 42;
    }
    if(!build_live_water_preview(std::numeric_limits<float>::infinity())
        .vertices.empty())return 43;
    if(!build_live_water_preview(999999.f).vertices.empty())return 44;
    return 0;
}
