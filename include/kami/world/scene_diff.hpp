#pragma once
#include "kami/world/scene.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace kami::world {
// Compare the fully decoded *visible* vertex buffer. Revision counters
// can advance for irrelevant simulator traffic (distant objects, duplicate
// updates, textures outside the 24-layer GPU atlas). Skip GPU allocation and
// upload when the renderable world has not changed at all.
[[nodiscard]] inline bool same_scene_geometry(
    const SceneMesh& a,const SceneMesh& b) noexcept {
    static_assert(sizeof(Vertex)==68U);
    static_assert(std::is_trivially_copyable_v<Vertex>);
    if(a.terrain_count!=b.terrain_count||
       a.avatar_count!=b.avatar_count||
       a.vertices.size()!=b.vertices.size())return false;
    return a.vertices.empty()||
        std::memcmp(a.vertices.data(),b.vertices.data(),
                    a.vertices.size()*sizeof(Vertex))==0;
}
// Rebuild local-origin geometry only after a meaningful simulator
// displacement (or a true world/mesh/texture revision). Tracking every
// 4cm own-avatar terse packet as a new region origin caused 800ms storms.
[[nodiscard]] inline bool world_scene_rebase_due(
    const std::array<float,3>& verified_position,
    const std::array<float,3>& last_upload_position,
    bool last_upload_valid,float threshold_m=12.f) noexcept {
    for(unsigned i=0;i<3U;++i)
        if(!std::isfinite(verified_position[i]))return false;
    if(!last_upload_valid)return true;
    if(!std::isfinite(threshold_m)||threshold_m<=0.f)return false;
    double d2=0.;
    for(unsigned i=0;i<3U;++i){
        if(!std::isfinite(last_upload_position[i]))return true;
        const double d=double(verified_position[i])-last_upload_position[i];
        d2+=d*d;
    }
    return d2>=double(threshold_m)*threshold_m;
}
} // namespace kami::world
