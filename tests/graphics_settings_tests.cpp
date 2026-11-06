#include "kami/graphics/settings.hpp"
#include "kami/world/scene.hpp"
#include <cstdint>
int main() {
    using namespace kami::graphics;
    Settings def;
    if(!def.pbr || !def.animate_avatar || !def.shadows) return 6;
    if(def.preset!=Preset::Balanced || def.render_scale_percent!=100 ||
       def.fps_limit!=60 || !def.vsync) return 1;
    Settings bad{};
    bad.preset=static_cast<Preset>(1234);
    bad.render_scale_percent=999;
    bad.fps_limit=17;
    bad=normalize(bad);
    if(bad.preset!=Preset::Balanced||bad.render_scale_percent!=100||bad.fps_limit!=60) return 2;
    for(auto preset:{Preset::Performance,Preset::Balanced,Preset::Quality}) {
        Settings s; s.preset=preset;
        const auto mesh=kami::world::build_test_scene(s.terrain_divisions());
        const auto div=s.terrain_divisions();
        if(mesh.terrain_count != static_cast<std::size_t>(div)*div*6U) return 3;
        if(mesh.avatar_count!=216U+1500U) return 4;
    }
    if(Settings{Preset::Performance}.sun_strength()>=Settings{Preset::Quality}.sun_strength())
        return 5;
    // Quality can admit more nearby SL meshes than the previous 1M
    // triangle cap, but must stay below the global 4M-vertex scene limit.
    Settings quality;quality.preset=Preset::Quality;
    Settings balanced;balanced.preset=Preset::Balanced;
    if(quality.mesh_triangles()!=1300000U||
       quality.mesh_triangles()<=balanced.mesh_triangles()||
       quality.mesh_triangles()*3U>4000000U)return 7;
    if(normalize(quality).mesh_triangles()!=1300000U)return 8;
    return 0;
}
