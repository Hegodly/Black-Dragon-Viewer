#pragma once
#include "kami/world/scene.hpp"
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
} // namespace kami::world
