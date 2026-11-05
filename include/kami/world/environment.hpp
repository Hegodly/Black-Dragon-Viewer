#pragma once
#include "kami/protocol/object_spatial.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <map>
namespace kami::world {
struct EnvironmentProbe {
    std::array<float,3> center{},half{32.f,32.f,32.f};
    std::array<std::array<float,3>,3> axis{{{1,0,0},{0,1,0},{0,0,1}}};
    float ambiance=1.f,clip=.1f;
    bool box=false,server=false;
};
inline EnvironmentProbe select_environment_probe(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::array<float,3>& origin,const std::array<float,3>& eye){
    EnvironmentProbe result;result.center=eye;float best=1.e30f;
    for(const auto& [id,t]:roots){
        (void)id;if(!t.reflection_probe||t.parent_id!=0U)continue;
        const std::array<float,3> p{t.x-origin[0],t.z-origin[2],t.y-origin[1]};
        const float dx=p[0]-eye[0],dy=p[1]-eye[1],dz=p[2]-eye[2];
        const float radius=std::max(std::max(t.sx,t.sy),t.sz)*.5f;
        const float distance=dx*dx+dy*dy+dz*dz;
        if(!std::isfinite(distance)||radius<=0.f||distance>radius*radius*4.f||distance>=best)continue;
        best=distance;result.center=p;result.half={t.sx*.5f,t.sz*.5f,t.sy*.5f};
        result.clip=t.probe_clip;result.ambiance=t.probe_ambiance;
        result.box=(t.probe_flags&1U)!=0U;result.server=true;
        // Quaternion basis converted from SL Z-up to KAMI Y-up.
        const float x=t.rx,y=t.ry,z=t.rz,w=t.rw;
        result.axis={{{1-2*(y*y+z*z),2*(x*z-y*w),2*(x*y+z*w)},
                      {2*(x*z+y*w),1-2*(x*x+y*y),2*(y*z-x*w)},
                      {2*(x*y-z*w),2*(y*z+x*w),1-2*(x*x+z*z)}}};
    }return result;
}
}
