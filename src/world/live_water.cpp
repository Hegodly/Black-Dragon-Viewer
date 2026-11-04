#include "kami/world/live_water.hpp"
#include "kami/world/terrain_quality.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace kami::world {
namespace {
struct Sample {float x=0.f,y=0.f,z=0.f;};
bool read(const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
          unsigned size,unsigned gx,unsigned gz,
          float agent_x,float agent_y,float agent_z,Sample& out) {
    const unsigned px=gx/size,pz=gz/size;
    if(px>=32U||pz>=32U)return false;
    const auto key=static_cast<std::uint16_t>(pz*32U+px);
    const auto it=decoded.find(key);
    if(it==decoded.end()||it->second.size!=size||
       it->second.x!=px||it->second.y!=pz)return false;
    const auto localx=gx%size,localz=gz%size;
    const float height=it->second.elevation[localz*size+localx];
    if(!std::isfinite(height)||height< -4096.f||height>8192.f)
        return false;
    out={static_cast<float>(gx)-agent_x,height-agent_z,
         static_cast<float>(gz)-agent_y};
    return std::isfinite(out.x)&&std::isfinite(out.y)&&
           std::isfinite(out.z);
}
[[nodiscard]] Sample crossing(Sample a,Sample b,float level) {
    const float delta=b.y-a.y;
    const float t=std::abs(delta)<1.e-5f?.5f:
        std::clamp((level-a.y)/delta,0.f,1.f);
    return {a.x+(b.x-a.x)*t,level,a.z+(b.z-a.z)*t};
}
void clip_triangle(LiveMaskedWater& result,Sample a,Sample b,Sample c,
                   float level,std::size_t limit) {
    const std::array<Sample,3> in={a,b,c};
    std::array<Sample,6> polygon{};
    unsigned count=0;
    for(unsigned i=0;i<3;++i){
        const auto p=in[i],q=in[(i+1)%3];
        const bool p_wet=p.y<=level,q_wet=q.y<=level;
        if(p_wet)polygon[count++]=p;
        if(p_wet!=q_wet)polygon[count++]=crossing(p,q,level);
    }
    if(count<3U)return;
    if(result.scene.vertices.size()+(count-2U)*3U>limit)return;
    constexpr float eps=.035f;
    const auto emit=[&](Sample p){
        // The points of the clipped polygon share the same known water
        // height; a small bias prevents z-fighting at the shoreline.
        result.scene.vertices.push_back({
            p.x,level+eps,p.z,0.f,1.f,0.f,
            .055f,.35f,.49f,.14f,.2f,p.x*.025f,p.z*.025f
        });
    };
    for(unsigned i=1;i+1<count;++i){
        emit(polygon[0]);emit(polygon[i]);emit(polygon[i+1]);
        ++result.submerged_triangles;
        if(count!=3U)++result.shoreline_triangles;
    }
}
} // namespace
LiveMaskedWater build_live_masked_water(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    const std::vector<const protocol::terrain::Patch*>& selected,
    float agent_x,float agent_y,float agent_z,float water_z,
    std::size_t max_vertices){
    LiveMaskedWater result{};
    if(!std::isfinite(agent_x)||!std::isfinite(agent_y)||
       !std::isfinite(agent_z)||!std::isfinite(water_z)||
       agent_x<0.f||agent_x>=256.f||
       agent_y<0.f||agent_y>=256.f||
       selected.size()>64U)return result;
    const auto level=water_z-agent_z;
    max_vertices=std::min<std::size_t>(max_vertices,120000U);
    max_vertices-=max_vertices%3U;
    for(const auto* patch:selected){
        if(!patch||(patch->size!=16U&&patch->size!=32U)||
           patch->x>=32U||patch->y>=32U)continue;
        if(result.scene.vertices.size()+12U>max_vertices)break;
        for(unsigned z=0;z<patch->size;++z){
            for(unsigned x=0;x<patch->size;++x){
                if(result.scene.vertices.size()+12U>max_vertices)break;
                const unsigned gx=static_cast<unsigned>(patch->x)*
                                  patch->size+x;
                const unsigned gz=static_cast<unsigned>(patch->y)*
                                  patch->size+z;
                Sample a{},b{},c{},d{};
                if(!read(decoded,patch->size,gx,gz,
                         agent_x,agent_y,agent_z,a)||
                   !read(decoded,patch->size,gx,gz+1,
                         agent_x,agent_y,agent_z,b)||
                   !read(decoded,patch->size,gx+1,gz+1,
                         agent_x,agent_y,agent_z,c)||
                   !read(decoded,patch->size,gx+1,gz,
                         agent_x,agent_y,agent_z,d))continue;
                // Match the terrain triangle safety rule, so rejected
                // spike cells cannot leave giant floating water wedges.
                if(!terrainquality::renderable_cell(a.y,b.y,c.y,d.y)){
                    ++result.skipped_discontinuity_quads;
                    continue;
                }
                clip_triangle(result,a,b,c,level,max_vertices);
                clip_triangle(result,a,c,d,level,max_vertices);
            }
        }
    }
    result.scene.terrain_count=result.scene.vertices.size();
    return result;
}
} // namespace kami::world
