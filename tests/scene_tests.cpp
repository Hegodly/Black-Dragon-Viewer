#include "kami/world/scene.hpp"
#include <cmath>
#include <cstddef>
#include <algorithm>

int main() {
    const auto mesh=kami::world::build_test_scene();
    if(mesh.terrain_count!=32U*32U*6U) return 1;
    if(mesh.avatar_count!=6U*36U) return 2;
    if(mesh.vertices.size()!=mesh.terrain_count+mesh.avatar_count) return 3;
    for(const auto& v:mesh.vertices) {
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||!std::isfinite(v.pz)) return 4;
        if(!std::isfinite(v.nx)||!std::isfinite(v.ny)||!std::isfinite(v.nz)) return 5;
        if(!std::isfinite(v.r)||!std::isfinite(v.g)||!std::isfinite(v.b)) return 6;
        const float n=std::sqrt(v.nx*v.nx+v.ny*v.ny+v.nz*v.nz);
        if(n<.98f||n>1.02f) return 7;
        if(v.r<0.f||v.r>1.f||v.g<0.f||v.g>1.f||v.b<0.f||v.b>1.f) return 8;
    }
    const auto first=mesh.vertices.front();
    if(first.px<-15.f||first.px>15.f||first.pz<-15.f||first.pz>15.f) return 9;
    return 0;
}
