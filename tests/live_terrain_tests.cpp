#include "kami/world/live_terrain.hpp"
#include <cmath>
#include <cstdint>
#include <map>
#include <limits>
#include <vector>

namespace {
kami::protocol::terrain::Patch patch(unsigned x,unsigned z){
    kami::protocol::terrain::Patch p{};
    p.size=16U;p.x=static_cast<std::uint8_t>(x);
    p.y=static_cast<std::uint8_t>(z);
    for(unsigned row=0;row<16U;++row)
        for(unsigned col=0;col<16U;++col)
            p.elevation[row*16U+col]=20.f+
                static_cast<float>(x*16U+col)*.25f+
                static_cast<float>(z*16U+row)*.5f;
    return p;
}
} // namespace
int main(){
    using namespace kami;
    // Object updates can precede the first land layer. The terrain builder
    // must safely produce an empty terrain batch so the renderer can still
    // upload independently verified spatial geometry in the same scene.
    std::map<std::uint16_t,protocol::terrain::Patch> no_patches;
    std::vector<const protocol::terrain::Patch*> no_selected;
    const auto no_land=world::build_live_terrain_mesh(
        no_patches,no_selected,0.f,0.f,0.f);
    if(no_land.quads!=0U||!no_land.scene.vertices.empty())return 10;
    std::map<std::uint16_t,protocol::terrain::Patch> patches;
    patches.emplace(0U,patch(0,0));
    const auto* a=&patches.find(0U)->second;
    std::vector<const protocol::terrain::Patch*> selected{a};
    const auto one=world::build_live_terrain_mesh(patches,selected,0,0,0);
    if(one.quads!=225U||one.stitched_quads!=0U||
       one.scene.vertices.size()!=1350U)return 1;
    // Two horizontally adjacent patches must fill the previously missing
    // one-meter stripe. A left patch now draws 15 additional quads.
    patches.emplace(1U,patch(1,0));
    const auto* b=&patches.find(1U)->second;
    selected.push_back(b);
    const auto two=world::build_live_terrain_mesh(patches,selected,0,0,0);
    if(two.quads!=465U||two.stitched_quads!=15U||
       two.scene.vertices.size()!=2790U)return 2;
    bool saw_bridge=false;
    for(const auto& v:two.scene.vertices){
        if(std::fabs(v.px-16.f)<.001f&&
           std::fabs(v.pz-7.f)<.001f&&
           std::fabs(v.py-(20.f+16.f*.25f+7.f*.5f))<.001f) {
            saw_bridge=true;break;
        }
    }
    if(!saw_bridge)return 3;
    // Diagnostic shading must remain finite/bounded without requiring
    // texture downloads. These colors do not impersonate SL textures.
    for(const auto& v:two.scene.vertices){
        if(!std::isfinite(v.r)||!std::isfinite(v.g)||
           !std::isfinite(v.b)||v.r<0.f||v.r>1.f||
           v.g<0.f||v.g>1.f||v.b<0.f||v.b>1.f)
            return 7;
    }
    patches.emplace(32U,patch(0,1));
    patches.emplace(33U,patch(1,1));
    selected={&patches.find(0U)->second,&patches.find(1U)->second,
              &patches.find(32U)->second,&patches.find(33U)->second};
    const auto full=world::build_live_terrain_mesh(patches,selected,0,0,0);
    if(full.quads!=31U*31U||full.stitched_quads!=61U||
       full.scene.vertices.size()!=31U*31U*6U)return 4;
    // No guessed vertices beyond missing samples.
    patches.erase(33U);
    selected={&patches.find(0U)->second,&patches.find(1U)->second,
              &patches.find(32U)->second};
    const auto incomplete=world::build_live_terrain_mesh(patches,selected,0,0,0);
    if(incomplete.quads>=full.quads||incomplete.skipped_quads==0U)
        return 5;
    const auto invalid=world::build_live_terrain_mesh(patches,selected,
        std::numeric_limits<float>::infinity(),0.f,0.f);
    if(!invalid.scene.vertices.empty())return 6;
    // R22 regression: one corrupted height must not produce four
    // enormous triangles/walls. Skip the four affected *meter cells*
    // without changing or fabricating any elevation samples.
    patches.clear();
    patches.emplace(0U,patch(0U,0U));
    patches.at(0U).elevation[8U*16U+8U]+=180.f;
    selected={&patches.at(0U)};
    const auto guarded=world::build_live_terrain_mesh(
        patches,selected,0.f,0.f,0.f);
    // 31 edge cells lack a decoded adjacent patch; those are not spikes.
    // Four more local cells are omitted for the deliberately bad height.
    if(guarded.skipped_discontinuity_quads!=4U||
       guarded.skipped_quads!=35U||
       guarded.quads!=221U||
       guarded.scene.vertices.size()!=221U*6U)return 8;
    // Ordinary gradients (including linked patch seams) remain intact.
    patches.at(0U).elevation[8U*16U+8U]-=180.f;
    const auto recovered=world::build_live_terrain_mesh(
        patches,selected,0.f,0.f,0.f);
    if(recovered.quads!=225U||
       recovered.skipped_discontinuity_quads!=0U)return 9;
    return 0;
}
