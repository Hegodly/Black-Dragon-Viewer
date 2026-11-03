#include "kami/world/scene.hpp"
#include <algorithm>
#include <cmath>
#include <array>

namespace kami::world {
namespace {
struct P { float x,y,z; };
struct C { float r,g,b; };
P sub(P a,P b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
P normal(P a,P b,P c) {
    const P u=sub(b,a),v=sub(c,a);
    const P n={u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
    const float mag=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    return mag>0.00001f ? P{n.x/mag,n.y/mag,n.z/mag} : P{0,1,0};
}
void triangle(std::vector<Vertex>& out,P a,P b,P c,C color,
              float metallic=0.f,float roughness=.85f) {
    const P n=normal(a,b,c);
    for(const P p: {a,b,c})
        out.push_back({p.x,p.y,p.z,n.x,n.y,n.z,color.r,color.g,color.b,
                       metallic,roughness,p.x*.18f,p.z*.18f});
}
void quad(std::vector<Vertex>& out,P a,P b,P c,P d,C color,
          float metallic,float roughness) {
    triangle(out,a,b,c,color,metallic,roughness);
    triangle(out,a,c,d,color,metallic,roughness);
}
void box(std::vector<Vertex>& out,P center,P half,C color,
         float metallic,float roughness) {
    const float x=center.x,y=center.y,z=center.z;
    const float a=half.x,b=half.y,c=half.z;
    const std::array<P,8> p={{
        {x-a,y-b,z-c},{x+a,y-b,z-c},{x+a,y+b,z-c},{x-a,y+b,z-c},
        {x-a,y-b,z+c},{x+a,y-b,z+c},{x+a,y+b,z+c},{x-a,y+b,z+c}
    }};
    quad(out,p[4],p[5],p[6],p[7],color,metallic,roughness);
    quad(out,p[1],p[0],p[3],p[2],color,metallic,roughness);
    quad(out,p[0],p[4],p[7],p[3],color,metallic,roughness);
    quad(out,p[5],p[1],p[2],p[6],color,metallic,roughness);
    quad(out,p[3],p[7],p[6],p[2],color,metallic,roughness);
    quad(out,p[0],p[1],p[5],p[4],color,metallic,roughness);
}
// Low-cost colored fitted-physics markers on the original six-box mannequin.
// Markers are an offline visualization, not skinned Second Life mesh.
void patch_triangle(std::vector<Vertex>& out,P a,P b,P c,
                    P center,P radius,C color,float region) {
    for(const P point:{a,b,c}) {
        const P n={(point.x-center.x)/(radius.x*radius.x),
                   (point.y-center.y)/(radius.y*radius.y),
                   (point.z-center.z)/(radius.z*radius.z)};
        const float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        const P normal=len>0.00001f?
            P{n.x/len,n.y/len,n.z/len}:P{0.f,1.f,0.f};
        out.push_back({point.x,point.y,point.z,
                       normal.x,normal.y,normal.z,
                       color.r,color.g,color.b,.1f,.48f,
                       point.x*.18f,point.z*.18f,region});
    }
}
void physics_patch(std::vector<Vertex>& out,P center,P radius,C color,float region) {
    constexpr int bands=6,slices=10;
    constexpr float pi=3.14159265358979323846f;
    auto point=[&](int row,int column)->P {
        const float latitude=-pi*.5f+
            pi*static_cast<float>(row)/static_cast<float>(bands);
        const float longitude=2.f*pi*
            static_cast<float>(column)/static_cast<float>(slices);
        return {center.x+radius.x*std::cos(latitude)*std::cos(longitude),
                center.y+radius.y*std::sin(latitude),
                center.z+radius.z*std::cos(latitude)*std::sin(longitude)};
    };
    for(int y=0;y<bands;++y)for(int x=0;x<slices;++x) {
        const P a=point(y,x),b=point(y+1,x);
        const P d=point(y,x+1),cc=point(y+1,x+1);
        patch_triangle(out,a,b,cc,center,radius,color,region);
        patch_triangle(out,a,cc,d,center,radius,color,region);
    }
}
float height(float x,float z) {
    const float wave=0.36f*std::sin(x*.47f)*std::cos(z*.40f);
    const float hill=0.5f*std::exp(-(x*x+z*z)/85.f);
    return wave+hill-.55f;
}
} // namespace

float sample_test_terrain_height(float x, float z) noexcept {
    if (x < -14.f || x > 14.f || z < -14.f || z > 14.f)
        return -1000.f; // Outside the bounded demo region: no terrain hit.
    return height(x, z);
}

SceneMesh build_test_scene(unsigned divisions) {
    SceneMesh scene;
    divisions=std::clamp(divisions,8U,64U);
    const float step=28.f/static_cast<float>(divisions);
    scene.vertices.reserve(divisions*divisions*6+6*36+5*6*10*6);
    for(int z=0;z<divisions;++z) {
        for(int x=0;x<divisions;++x) {
            const float x0=-14.f+x*step,x1=x0+step;
            const float z0=-14.f+z*step,z1=z0+step;
            const P a{x0,height(x0,z0),z0},b{x0,height(x0,z1),z1};
            const P c{x1,height(x1,z1),z1},d{x1,height(x1,z0),z0};
            const float tint=0.035f*std::sin(x0*.87f+z0*.31f);
            const C grass{.14f+tint,.36f+tint,.31f+tint};
            triangle(scene.vertices,a,b,c,grass);
            triangle(scene.vertices,a,c,d,grass);
        }
    }
    scene.terrain_count=scene.vertices.size();

    // A stylized, deliberately anonymous mannequin demonstrates avatar-like
    // geometry, materials and lighting. No SL appearance is represented.
    box(scene.vertices,{0,1.75f,0},{.50f,.75f,.26f},{.12f,.38f,.86f},.32f,.28f);
    box(scene.vertices,{0,2.77f,0},{.35f,.35f,.34f},{.78f,.57f,.41f},0.f,.65f);
    box(scene.vertices,{-.74f,1.70f,0},{.19f,.67f,.20f},{.17f,.44f,.78f},.35f,.28f);
    box(scene.vertices,{.74f,1.70f,0},{.19f,.67f,.20f},{.17f,.44f,.78f},.35f,.28f);
    box(scene.vertices,{-.24f,.55f,0},{.19f,.56f,.23f},{.15f,.23f,.39f},.05f,.83f);
    box(scene.vertices,{.24f,.55f,0},{.19f,.56f,.23f},{.15f,.23f,.39f},.05f,.83f);
    // Slightly inset surfaces make all three physics regions visible in DX12,
    // including when the movement system is running at very high frame rates.
    physics_patch(scene.vertices,{-.225f,2.03f,.268f},{.18f,.17f,.16f},
                  {.36f,.78f,.91f},1.f);
    physics_patch(scene.vertices,{ .225f,2.03f,.268f},{.18f,.17f,.16f},
                  {.36f,.78f,.91f},1.f);
    physics_patch(scene.vertices,{0.f,1.49f,.263f},{.27f,.29f,.15f},
                  {.57f,.89f,.73f},2.f);
    physics_patch(scene.vertices,{-.22f,1.30f,-.265f},{.19f,.21f,.16f},
                  {.66f,.57f,.92f},3.f);
    physics_patch(scene.vertices,{ .22f,1.30f,-.265f},{.19f,.21f,.16f},
                  {.66f,.57f,.92f},3.f);
    scene.avatar_count=scene.vertices.size()-scene.terrain_count;
    return scene;
}
} // namespace kami::world
