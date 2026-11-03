#include "kami/features/creator.hpp"
#include <cmath>
#include <limits>

namespace kami::creator {
namespace {
struct Vec {double x,y,z;};
Vec sub(Vec a,Vec b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
double squared_cross(Vec a,Vec b) {
    const double x=a.y*b.z-a.z*b.y,y=a.z*b.x-a.x*b.z,z=a.x*b.y-a.y*b.x;
    return x*x+y*y+z*z;
}
} // namespace
Audit inspect(const world::SceneMesh& mesh) noexcept {
    Audit audit;
    audit.vertices=mesh.vertices.size();
    audit.triangles=audit.vertices/3;
    if(audit.vertices==0||audit.vertices%3!=0) {
        audit.problem="Triangle list must be non-empty and divisible by three";
        return audit;
    }
    if(mesh.terrain_count+mesh.avatar_count!=audit.vertices) {
        audit.problem="Scene section counts do not match vertex buffer";
        return audit;
    }
    for(std::size_t i=0;i<audit.vertices;i+=3) {
        for(std::size_t j=0;j<3;++j) {
            const auto& v=mesh.vertices[i+j];
            if(!std::isfinite(v.px)||!std::isfinite(v.py)||!std::isfinite(v.pz) ||
               !std::isfinite(v.nx)||!std::isfinite(v.ny)||!std::isfinite(v.nz)||
               !std::isfinite(v.u)||!std::isfinite(v.v)) {
                audit.problem="Non-finite vertex attribute";return audit;
            }
        }
        const auto& x=mesh.vertices[i];const auto& y=mesh.vertices[i+1];
        const auto& z=mesh.vertices[i+2];
        Vec a{x.px,x.py,x.pz},b{y.px,y.py,y.pz},c{z.px,z.py,z.pz};
        if(squared_cross(sub(b,a),sub(c,a))<1e-14)++audit.degenerate_triangles;
    }
    if(audit.degenerate_triangles!=0) {
        audit.problem="Degenerate triangles in source mesh";return audit;
    }
    audit.valid=true;
    return audit;
}
} // namespace kami::creator
