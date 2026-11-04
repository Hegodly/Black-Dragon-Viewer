#include "kami/world/live_water.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

namespace {
kami::protocol::terrain::Patch make_patch(float wet,float dry){
    kami::protocol::terrain::Patch p{};
    p.size=16U;p.x=0U;p.y=0U;
    for(unsigned z=0;z<16U;++z)
        for(unsigned x=0;x<16U;++x)
            p.elevation[z*16U+x]=x>=8U?dry:wet;
    return p;
}
}
int main(){
    using namespace kami;
    std::map<std::uint16_t,protocol::terrain::Patch> land;
    land.emplace(0U,make_patch(10.f,30.f));
    std::vector<const protocol::terrain::Patch*> selected{&land.at(0)};
    // Real land height samples control the water mask. Only the half of
    // the patch below Z=20 may be covered by procedurally shaded water.
    const auto shore=world::build_live_masked_water(
        land,selected,5.f,5.f,25.f,20.f);
    if(shore.scene.vertices.empty()||
       shore.scene.terrain_count!=shore.scene.vertices.size()||
       shore.scene.avatar_count!=0U||
       shore.shoreline_triangles==0U)return 1;
    if(shore.scene.vertices.size()>=225U*6U)return 2;
    for(const auto& v:shore.scene.vertices){
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||
           !std::isfinite(v.pz)||!std::isfinite(v.r)||
           std::fabs(v.py-(-5.f+.035f))>1.e-4f||
           v.px>2.6f||v.bone_weights!=0U)return 3;
    }
    // No inundated land -> absolutely NO full-region fake blue sheet.
    const auto dry=world::build_live_masked_water(
        land,selected,5.f,5.f,25.f,5.f);
    if(!dry.scene.vertices.empty())return 4;
    const auto flooded=world::build_live_masked_water(
        land,selected,5.f,5.f,25.f,50.f);
    if(flooded.scene.vertices.size()!=225U*6U)return 5;
    const auto bounded=world::build_live_masked_water(
        land,selected,5.f,5.f,25.f,50.f,72U);
    if(bounded.scene.vertices.empty()||
       bounded.scene.vertices.size()>72U||
       bounded.scene.vertices.size()%3U!=0U)return 6;
    if(!world::build_live_masked_water(
        land,selected,5.f,5.f,25.f,
        std::numeric_limits<float>::quiet_NaN()).scene.vertices.empty())
        return 7;
    return 0;
}
