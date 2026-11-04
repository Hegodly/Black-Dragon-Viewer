#include "kami/world/object_proxies.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace kami::world {
namespace {
struct P {float x,y,z;};
void face(std::vector<Vertex>& vertices,P a,P b,P c,P d,P normal) {
    constexpr float red=.98f,green=.63f,blue=.14f;
    const auto append=[&](P p){
        vertices.push_back({p.x,p.y,p.z,normal.x,normal.y,normal.z,
            red,green,blue,.02f,.8f,0.f,0.f});
    };
    append(a);append(b);append(c);
    append(a);append(c);append(d);
}
void beam(std::vector<Vertex>& out,P center,P half){
    // Keep the entire box in the original DX12 terrain pass:
    // 6 faces * 6 vertices, unskinned and unaffected by avatar heading.
    const float x=center.x,y=center.y,z=center.z;
    const float a=half.x,b=half.y,c=half.z;
    const P p0{x-a,y-b,z-c},p1{x+a,y-b,z-c};
    const P p2{x+a,y+b,z-c},p3{x-a,y+b,z-c};
    const P p4{x-a,y-b,z+c},p5{x+a,y-b,z+c};
    const P p6{x+a,y+b,z+c},p7{x-a,y+b,z+c};
    face(out,p4,p5,p6,p7,{0,0,1});
    face(out,p1,p0,p3,p2,{0,0,-1});
    face(out,p0,p4,p7,p3,{-1,0,0});
    face(out,p5,p1,p2,p6,{1,0,0});
    face(out,p3,p7,p6,p2,{0,1,0});
    face(out,p0,p1,p5,p4,{0,-1,0});
}
void frame(std::vector<Vertex>& out,P c,P h){
    // Outline, not opaque fabricated buildings. 12 slender frame beams
    // communicate object LOCATION and SIZE without asserting its shape.
    constexpr float r=.035f;
    for(float sy:{-1.f,1.f})
        for(float sz:{-1.f,1.f})
            beam(out,{c.x,c.y+sy*h.y,c.z+sz*h.z},
                 {h.x,r,r});
    for(float sx:{-1.f,1.f})
        for(float sz:{-1.f,1.f})
            beam(out,{c.x+sx*h.x,c.y,c.z+sz*h.z},
                 {r,h.y,r});
    for(float sx:{-1.f,1.f})
        for(float sy:{-1.f,1.f})
            beam(out,{c.x+sx*h.x,c.y+sy*h.y,c.z},
                 {r,r,h.z});
}
} // namespace
SceneMesh build_live_object_spatial_proxies(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& objects,
    float agent_x,float agent_y,float agent_z,
    std::size_t max_proxies){
    SceneMesh scene{};
    if(!std::isfinite(agent_x)||!std::isfinite(agent_y)||
       !std::isfinite(agent_z))return scene;
    max_proxies=std::min<std::size_t>(max_proxies,64U);
    std::vector<const protocol::objectspatial::Transform*> near;
    near.reserve(objects.size());
    for(const auto& [id,t]:objects){
        (void)id;
        // Render near the verified initial spawn, not all 256m around it.
        const float dx=t.x-agent_x,dz=t.y-agent_y;
        if(std::abs(dx)>96.f||std::abs(dz)>96.f||
           std::abs(t.z-agent_z)>60.f)continue;
        near.push_back(&t);
    }
    std::sort(near.begin(),near.end(),[&](auto* a,auto* b){
        const float ax=a->x-agent_x,az=a->y-agent_y;
        const float bx=b->x-agent_x,bz=b->y-agent_y;
        return ax*ax+az*az<bx*bx+bz*bz;
    });
    if(near.size()>max_proxies)near.resize(max_proxies);
    scene.vertices.reserve(near.size()*432U);
    for(const auto* t:near){
        // SL X/Y/Z -> KAMI DX12 X/Z/Y
        const P center{t->x-agent_x,t->z-agent_z,t->y-agent_y};
        const P half{t->sx*.5f,t->sz*.5f,t->sy*.5f};
        frame(scene.vertices,center,half);
    }
    scene.terrain_count=scene.vertices.size();
    return scene;
}
} // namespace kami::world
