#include "kami/world/live_primitive_geometry.hpp"
#include "kami/world/texture_uv.hpp"
#include "kami/protocol/texture_budget.hpp"
#include "kami/protocol/volume_params.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <set>
#include <map>

namespace kami::world {
namespace {
using Transform=protocol::objectspatial::Transform;
using Kind=protocol::objectspatial::PrimitiveKind;
struct V {float x,y,z;};
struct Q {float x,y,z,w;};
struct Pose {V pos;Q rot;};
V add(V a,V b) noexcept {return {a.x+b.x,a.y+b.y,a.z+b.z};}
V sub(V a,V b) noexcept {return {a.x-b.x,a.y-b.y,a.z-b.z};}
V scale(V a,float k) noexcept {return {a.x*k,a.y*k,a.z*k};}
V cross(V a,V b) noexcept {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
float dot(V a,V b) noexcept {return a.x*b.x+a.y*b.y+a.z*b.z;}
Q quat(const Transform& t) noexcept {return {t.rx,t.ry,t.rz,t.rw};}
V pos(const Transform& t) noexcept {return {t.x,t.y,t.z};}
Q multiply(Q a,Q b) noexcept {
    return {a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,
            a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};
}
V rotate(Q q,V v) noexcept {
    const V axis{q.x,q.y,q.z};
    return add(v,scale(add(scale(cross(axis,v),q.w),
                           cross(axis,cross(axis,v))),2.f));
}
bool finite(V p) noexcept {
    return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);
}
bool valid_quat(Q q) noexcept {
    const float sq=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    return std::isfinite(sq)&&sq>.99f&&sq<1.01f;
}
enum class RootResult : std::uint8_t {
    Found, Missing, Avatar, Cycle, Invalid, TooDeep
};
RootResult resolve(const Transform& t,
    const std::map<std::uint32_t,Transform>& roots,
    const std::map<std::uint32_t,Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    Pose& out,std::uint32_t& missing_id) noexcept {
    missing_id=0U;
    if(t.parent_id==0U||t.local_id==0U||!valid_quat(quat(t)))
        return RootResult::Invalid;
    Pose next{pos(t),quat(t)};
    std::array<std::uint32_t,16> seen{};
    std::size_t count=0U;
    seen[count++]=t.local_id;
    std::uint32_t parent=t.parent_id;
    while(parent!=0U&&count<seen.size()){
        if(std::find(seen.begin(),seen.begin()+count,parent)!=
           seen.begin()+count)return RootResult::Cycle;
        seen[count++]=parent;
        if(avatar_roots.contains(parent))return RootResult::Avatar;
        const Transform* candidate=nullptr;
        const auto r=roots.find(parent);
        if(r!=roots.end())candidate=&r->second;
        else {
            const auto c=children.find(parent);
            if(c!=children.end())candidate=&c->second;
        }
        if(!candidate){
            missing_id=parent;
            return RootResult::Missing;
        }
        if(!valid_quat(quat(*candidate)))return RootResult::Invalid;
        next.pos=add(pos(*candidate),rotate(quat(*candidate),next.pos));
        next.rot=multiply(quat(*candidate),next.rot);
        if(!finite(next.pos)||!valid_quat(next.rot))
            return RootResult::Invalid;
        if(candidate->parent_id==0U){
            // Only verified region-space volume roots may anchor solids.
            if(r==roots.end())return RootResult::Invalid;
            out=next;
            return RootResult::Found;
        }
        parent=candidate->parent_id;
    }
    return RootResult::TooDeep;
}
using UV=std::array<float,2U>;
struct Emitter {
    std::vector<Vertex>& out;
    const Transform& material;
    const std::map<std::string,std::uint16_t>& layers;
    Pose pose;
    V half;
    V origin;
    V profile_scale{1.f,1.f,1.f};
    std::size_t& textured_triangles;
    const PBRFaceIndices& pbr_faces;
    float red=.78f,green=.83f,blue=.92f;
    V transform(V v) const noexcept {
        return add(pose.pos,rotate(pose.rot,
            {v.x*half.x*2.f*profile_scale.x,
             v.y*half.y*2.f*profile_scale.y,
             v.z*half.z*2.f}));
    }
    V render(V v) const noexcept {
        return {v.x-origin.x,v.z-origin.z,v.y-origin.y};
    }
    void tri(V x,V y,V z,unsigned face,UV uvx,UV uvy,UV uvz){
        const V local_center=scale(add(add(x,y),z),1.f/3.f);
        V a=render(transform(x)),b=render(transform(y)),c=render(transform(z));
        V normal=cross(sub(b,a),sub(c,a));
        const V expected=render(add(pose.pos,rotate(pose.rot,
            {local_center.x*half.x*2.f*profile_scale.x,
             local_center.y*half.y*2.f*profile_scale.y,
             local_center.z*half.z*2.f})));
        const V center=render(pose.pos);
        if(dot(normal,sub(expected,center))<0.f){
            std::swap(b,c);
            std::swap(uvy,uvz);
            normal=scale(normal,-1.f);
        }
        const float len=std::sqrt(dot(normal,normal));
        if(len<1.e-6f||!std::isfinite(len))return;
        normal=scale(normal,1.f/len);
        if(!finite(a)||!finite(b)||!finite(c))return;
        const auto pbr=pbr_faces.find(pbr_face_key(material.local_id,face));
        const auto material_index=pbr==pbr_faces.end()?0U:pbr->second;
        // Legacy face opacity does not replace glTF material alpha.
        if(material_index==0U&&material.face_rgba_present&&face<material.face_rgba.size()&&
           material.face_rgba[face][3]<=0.f)return;
        const auto& name=face<material.face_texture_override.size()&&
                           material.face_texture_override[face]?
            material.face_texture_asset_ids[face]:
            material.default_texture_asset_id;
        const auto found=layers.find(name);
        const std::uint16_t layer=name.empty()||found==layers.end()?
            0U:found->second;
        protocol::texture::UVTransform uv_param{};
        if(face<material.face_uv.size()&&material.face_rgba_present)
            uv_param=material.face_uv[face];
        UV coords[]={uvx,uvy,uvz};
        V positions[]={a,b,c};
        float r=red,g=green,bl=blue;
        if(face<material.face_rgba.size()&&material.face_rgba_present){
            r=material.face_rgba[face][0];
            g=material.face_rgba[face][1];
            bl=material.face_rgba[face][2];
        }
        for(unsigned i=0U;i<3U;++i){
            const auto uv=material_index?coords[i]:transform_diffuse_uv(coords[i][0],coords[i][1],uv_param);
            out.push_back({positions[i].x,positions[i].y,positions[i].z,
                normal.x,normal.y,normal.z,r,g,bl,.06f,.79f,
                uv[0],uv[1],0.f,0U,0U,static_cast<float>(layer),material_index});
        }
        if(layer>0U)++textured_triangles;
    }
    void quad(V a,V b,V c,V d,unsigned face,
              UV ua={0.f,1.f},UV ub={1.f,1.f},
              UV uc={1.f,0.f},UV ud={0.f,0.f}){
        tri(a,b,c,face,ua,ub,uc);
        tri(a,c,d,face,ua,uc,ud);
    }
};
void build_box(Emitter& e) {
    // Six independently addressable planar diffuse faces.
    const V z0{-.5f,-.5f,-.5f},z1{.5f,-.5f,-.5f},
            z2{.5f,.5f,-.5f},z3{-.5f,.5f,-.5f},
            z4{-.5f,-.5f,.5f},z5{.5f,-.5f,.5f},
            z6{.5f,.5f,.5f},z7{-.5f,.5f,.5f};
    // Official SL box face order: 0 top +Z, 1 -Y, 2 +X,
    // 3 +Y, 4 -X, 5 bottom -Z. Never derive it from loop order.
    e.quad(z4,z5,z6,z7,0U);
    e.quad(z0,z1,z5,z4,1U);
    e.quad(z5,z1,z2,z6,2U);
    e.quad(z3,z7,z6,z2,3U);
    e.quad(z0,z4,z7,z3,4U);
    e.quad(z1,z0,z3,z2,5U);
}
void build_cylinder(Emitter& e){
    constexpr unsigned kSides=20U;
    constexpr float pi=3.14159265358979323846f;
    for(unsigned i=0;i<kSides;++i){
        const float a=2.f*pi*static_cast<float>(i)/kSides;
        const float b=2.f*pi*static_cast<float>(i+1)/kSides;
        const V lo{std::cos(a)*.5f,std::sin(a)*.5f,-.5f};
        const V hi{lo.x,lo.y,.5f};
        const V lon{std::cos(b)*.5f,std::sin(b)*.5f,-.5f};
        const V hin{lon.x,lon.y,.5f};
        const float u0=float(i)/kSides,u1=float(i+1U)/kSides;
        e.quad(lo,lon,hin,hi,1U,{u0,1.f},{u1,1.f},
               {u1,0.f},{u0,0.f});
        e.tri({0,0,.5f},hi,hin,0U,
              {.5f,.5f},{hi.x+.5f,1.f-(hi.y+.5f)},
              {hin.x+.5f,1.f-(hin.y+.5f)});
        e.tri({0,0,-.5f},lon,lo,2U,
              {.5f,.5f},{lon.x+.5f,1.f-(lon.y+.5f)},
              {lo.x+.5f,1.f-(lo.y+.5f)});
    }
}
// Dependency-light subset of LLVolume's linear-path/circular-profile
// sweep. It preserves real cut, hollow, taper, twist and shear parameters;
// unsupported curve/profile families remain on the conservative legacy path.
float signed_wire(std::uint8_t value) noexcept {
    // LLVolumeParams uses U8_TO_F32: reinterpret the wire byte as S8.
    // These values are signed fixed-point, not unsigned bytes centered at 128.
    return static_cast<float>(static_cast<std::int8_t>(value));
}
void build_linear_circle_volume(Emitter& e,
        const protocol::volume::Parameters& p){
    constexpr float pi=3.14159265358979323846f;
    constexpr unsigned around=32U,base_along=12U;
    const float begin=p.profile_begin*2.f*pi;
    const float end=p.profile_end*2.f*pi;
    const float sweep=end-begin;
    if(!(sweep>0.0001f))return;
    const bool hollow=(p.profile_curve&0xf0U)==0x10U&&p.hollow>0.0001f;
    const float inner_radius=std::clamp(p.hollow,0.f,.95f)*.5f;
    const float shear_x=signed_wire(p.shear_x)*.01f;
    const float shear_y=signed_wire(p.shear_y)*.01f;
    const float twist0=signed_wire(p.twist_begin)*.01f;
    const float twist1=signed_wire(p.twist_end)*.01f;
    // Increase longitudinal detail for strongly twisted linear paths while
    // keeping a strict ceiling on geometry generated from network data.
    const unsigned along=std::clamp(static_cast<unsigned>(std::ceil(
        std::abs(twist1-twist0)*24.f/pi)),base_along,64U);
    const float taper_x=signed_wire(p.taper_x)*.01f;
    const float taper_y=signed_wire(p.taper_y)*.01f;
    const auto point=[&](float u,float angle,float radius)->V{
        const float twist=twist0+(twist1-twist0)*u;
        const float a=angle+twist;
        const float taperX=std::max(.05f,1.f-taper_x*u);
        const float taperY=std::max(.05f,1.f-taper_y*u);
        const float x=std::cos(a)*radius*taperX+shear_x*(u-.5f);
        const float y=std::sin(a)*radius*taperY+shear_y*(u-.5f);
        return {x,y,u-.5f};
    };
    const unsigned profile_steps=std::max(2U,
        static_cast<unsigned>(std::ceil(static_cast<float>(around)*sweep/(2.f*pi))));
    for(unsigned row=0U;row<along;++row){
        const float u0=p.path_begin+(p.path_end-p.path_begin)*float(row)/along;
        const float u1=p.path_begin+(p.path_end-p.path_begin)*float(row+1U)/along;
        for(unsigned col=0U;col<profile_steps;++col){
            const float a0=begin+sweep*float(col)/profile_steps;
            const float a1=begin+sweep*float(col+1U)/profile_steps;
            const float v0=float(col)/profile_steps;
            const float v1=float(col+1U)/profile_steps;
            e.quad(point(u0,a0,.5f),point(u0,a1,.5f),
                   point(u1,a1,.5f),point(u1,a0,.5f),5U,
                   {v0,float(row)/along},{v1,float(row)/along},
                   {v1,float(row+1U)/along},{v0,float(row+1U)/along});
            if(hollow){
                e.quad(point(u0,a1,inner_radius),point(u0,a0,inner_radius),
                       point(u1,a0,inner_radius),point(u1,a1,inner_radius),2U,
                       {v1,float(row)/along},{v0,float(row)/along},
                       {v0,float(row+1U)/along},{v1,float(row+1U)/along});
            }
        }
    }
    // A profile cut is a real open wedge in LLVolume, not a paper-thin
    // shell. Close both radial cut planes along the path so partial-circle
    // profiles have watertight side boundaries (including hollow profiles).
    if(sweep<2.f*pi-0.0001f){
        for(unsigned row=0U;row<along;++row){
            const float u0=p.path_begin+(p.path_end-p.path_begin)*float(row)/along;
            const float u1=p.path_begin+(p.path_end-p.path_begin)*float(row+1U)/along;
            for(unsigned edge=0U;edge<2U;++edge){
                const float angle=edge==0U?begin:end;
                const V outer0=point(u0,angle,.5f);
                const V outer1=point(u1,angle,.5f);
                const V inner0=hollow?point(u0,angle,inner_radius):point(u0,angle,0.f);
                const V inner1=hollow?point(u1,angle,inner_radius):point(u1,angle,0.f);
                if(edge==0U)e.quad(inner0,outer0,outer1,inner1,3U);
                else e.quad(outer0,inner0,inner1,outer1,4U);
            }
        }
    }
    // End caps are annular when hollow; fan triangles otherwise.
    const auto cap=[&](float u,unsigned face,bool reverse){
        const float z=u-.5f;
        const V center{shear_x*(u-.5f),shear_y*(u-.5f),z};
        for(unsigned col=0U;col<profile_steps;++col){
            const float a0=begin+sweep*float(col)/profile_steps;
            const float a1=begin+sweep*float(col+1U)/profile_steps;
            const V o0=point(u,a0,.5f),o1=point(u,a1,.5f);
            const auto cap_uv=[](V v)->UV{
                return {v.x+.5f,.5f-v.y};
            };
            if(hollow){
                const V i0=point(u,a0,inner_radius),i1=point(u,a1,inner_radius);
                if(reverse)e.quad(o1,o0,i0,i1,face,
                    cap_uv(o1),cap_uv(o0),cap_uv(i0),cap_uv(i1));
                else e.quad(o0,o1,i1,i0,face,
                    cap_uv(o0),cap_uv(o1),cap_uv(i1),cap_uv(i0));
            }else if(reverse)e.tri(center,o1,o0,face,
                {.5f,.5f},cap_uv(o1),cap_uv(o0));
            else e.tri(center,o0,o1,face,
                {.5f,.5f},cap_uv(o0),cap_uv(o1));
        }
    };
    cap(p.path_begin,0U,true);
    cap(p.path_end,1U,false);
}
void build_linear_polygon_volume(Emitter& e,
        const protocol::volume::Parameters& p,bool triangular){
    constexpr float pi=3.14159265358979323846f;
    const std::array<V,4U> square{{
        {-.5f,-.5f,0.f},{.5f,-.5f,0.f},
        {.5f,.5f,0.f},{-.5f,.5f,0.f}}};
    constexpr float tri_y=-0.2886751346f;
    const std::array<V,3U> triangle{{
        {-.5f,tri_y,0.f},{.5f,tri_y,0.f},{0.f,-2.f*tri_y,0.f}}};
    const unsigned sides=triangular?3U:4U;
    const float twist0=signed_wire(p.twist_begin)*.01f;
    const float twist1=signed_wire(p.twist_end)*.01f;
    const float taper_x=signed_wire(p.taper_x)*.01f;
    const float taper_y=signed_wire(p.taper_y)*.01f;
    const float shear_x=signed_wire(p.shear_x)*.01f;
    const float shear_y=signed_wire(p.shear_y)*.01f;
    const auto point=[&](float u,unsigned i)->V{
        const V v=triangular?triangle[i]:square[i];
        const float twist=twist0+(twist1-twist0)*u;
        const float cs=std::cos(twist),sn=std::sin(twist);
        const float x=v.x*std::max(.05f,1.f-taper_x*u);
        const float y=v.y*std::max(.05f,1.f-taper_y*u);
        return {x*cs-y*sn+shear_x*(u-.5f),
                x*sn+y*cs+shear_y*(u-.5f),u-.5f};
    };
    const unsigned along=std::clamp(static_cast<unsigned>(std::ceil(
        std::abs(twist1-twist0)*24.f/pi)),12U,64U);
    for(unsigned row=0U;row<along;++row){
        const float u0=p.path_begin+(p.path_end-p.path_begin)*float(row)/along;
        const float u1=p.path_begin+(p.path_end-p.path_begin)*float(row+1U)/along;
        for(unsigned i=0U;i<sides;++i){
            const unsigned next=(i+1U)%sides;
            // LLVolume face IDs: path caps 0/1, inner side 2,
            // profile cuts 3/4, and outer polygon sides begin at 5.
            e.quad(point(u0,i),point(u0,next),point(u1,next),point(u1,i),
                   5U+i,{0.f,float(row)/along},{1.f,float(row)/along},
                   {1.f,float(row+1U)/along},{0.f,float(row+1U)/along});
        }
    }
    const auto cap=[&](float u,unsigned face,bool reverse){
        V center{shear_x*(u-.5f),shear_y*(u-.5f),u-.5f};
        for(unsigned i=0U;i<sides;++i){
            const unsigned next=(i+1U)%sides;
            const V a=point(u,i),b=point(u,next);
            if(reverse)e.tri(center,b,a,face,{.5f,.5f},{1.f,0.f},{0.f,0.f});
            else e.tri(center,a,b,face,{.5f,.5f},{0.f,0.f},{1.f,0.f});
        }
    };
    cap(p.path_begin,0U,true);
    cap(p.path_end,1U,false);
}
void build_prism(Emitter& e){
    constexpr float y=-0.2886751346f;
    const V a{-.5f,y,-.5f}, b{.5f,y,-.5f},c{0.f,-2.f*y,-.5f};
    const V d{a.x,a.y,.5f},f{b.x,b.y,.5f},g{c.x,c.y,.5f};
    // Official SL prism order: top, +Y, -X, -Y, bottom.
    e.tri(d,f,g,0U,{0.f,1.f},{1.f,1.f},{.5f,0.f});
    e.quad(b,c,g,f,1U);
    e.quad(c,a,d,g,2U);
    e.quad(a,b,f,d,3U);
    e.tri(a,c,b,4U,{0.f,1.f},{.5f,0.f},{1.f,1.f});
}
void build_sphere(Emitter& e){
    constexpr unsigned lat=10U,lon=20U;
    constexpr float pi=3.14159265358979323846f;
    const auto vertex=[](float t,float p)->V{
        return {std::sin(t)*std::cos(p)*.5f,
                std::sin(t)*std::sin(p)*.5f,std::cos(t)*.5f};
    };
    for(unsigned row=0;row<lat;++row){
        const float a=pi*float(row)/lat,b=pi*float(row+1)/lat;
        for(unsigned col=0;col<lon;++col){
            const float x=2.f*pi*float(col)/lon;
            const float y=2.f*pi*float(col+1U)/lon;
            const float u0=float(col)/lon,u1=float(col+1U)/lon;
            const float v0=float(row)/lat,v1=float(row+1U)/lat;
            e.quad(vertex(a,x),vertex(a,y),vertex(b,y),vertex(b,x),
                   0U,{u0,v0},{u1,v0},{u1,v1},{u0,v1});
        }
    }
}
bool emit(std::vector<Vertex>& out,const Transform& t,Pose p,V origin,
          const std::map<std::string,std::uint16_t>& layers,
          std::size_t& textured_triangles,const PBRFaceIndices& pbr_faces){
    if(t.primitive==Kind::Unknown||!valid_quat(p.rot)||!finite(p.pos))
        return false;
    const std::size_t start=out.size();
    V profile_scale{1.f,1.f,1.f};
    if(t.volume_params_seen){
        protocol::volume::Parameters decoded{};
        if(!protocol::volume::decode(t.volume_params,
                                     t.volume_params_full_layout,decoded))
            return false;
        profile_scale.x=decoded.scale_x;
        profile_scale.y=decoded.scale_y;
    }
    Emitter e{out,t,layers,p,
              {t.sx*.5f,t.sy*.5f,t.sz*.5f},origin,profile_scale,
              textured_triangles,pbr_faces};
    if(t.base_tint_present){
        e.red=std::clamp(t.base_r,0.f,1.f);
        e.green=std::clamp(t.base_g,0.f,1.f);
        e.blue=std::clamp(t.base_b,0.f,1.f);
    }
    bool swept=false;
    if(t.volume_params_seen&&
       (t.primitive==Kind::Cylinder||t.primitive==Kind::Box||
        t.primitive==Kind::Prism)){
        protocol::volume::Parameters decoded{};
        if(protocol::volume::decode(t.volume_params,
                                    t.volume_params_full_layout,decoded)&&
           decoded.path_curve==0x10U&&
           decoded.radius_offset==0U&&decoded.revolutions==0U&&
           decoded.skew==0U){
            const auto profile=decoded.profile_curve&0x0fU;
            const auto hole=decoded.profile_curve&0xf0U;
            if(t.primitive==Kind::Cylinder&&profile==0U&&
               (hole==0U||hole==0x10U)){
                build_linear_circle_volume(e,decoded);swept=true;
            }else if(hole==0U&&decoded.hollow<=0.0001f&&
                     decoded.profile_begin<=0.0001f&&
                     decoded.profile_end>=0.9999f){
                if(t.primitive==Kind::Box&&profile==1U){
                    build_linear_polygon_volume(e,decoded,false);swept=true;
                }else if(t.primitive==Kind::Prism&&profile==3U){
                    build_linear_polygon_volume(e,decoded,true);swept=true;
                }
            }
        }
    }
    // A linear LLVolume that failed the sweep support checks must
    // never fall through to a generic box/cylinder stand-in. That would
    // make hollow, cut, or otherwise unsupported server shapes look valid
    // while silently discarding their real topology.
    if(!swept&&t.volume_params_seen&&
       (t.primitive==Kind::Box||t.primitive==Kind::Cylinder||
        t.primitive==Kind::Prism)){
        protocol::volume::Parameters decoded{};
        if(!protocol::volume::decode(t.volume_params,
                                     t.volume_params_full_layout,decoded))
            return false;
        if(decoded.path_curve==0x10U)return false;
    }
    if(!swept)switch(t.primitive){
        case Kind::Box:build_box(e);break;
        case Kind::Cylinder:build_cylinder(e);break;
        case Kind::Sphere:build_sphere(e);break;
        case Kind::Prism:build_prism(e);break;
        default:break;
    }
    return out.size()>start;
}
} // namespace
PrimitiveBatch build_live_primitive_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    float agent_x,float agent_y,float agent_z,std::size_t limit,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,std::uint16_t>& texture_layers,const PBRFaceIndices& pbr_faces,
    std::size_t max_vertices){
    PrimitiveBatch result{};
    const V origin{agent_x,agent_y,agent_z};
    if(!finite(origin))return result;
    // R65: cap Quality at 2,048 real primitive shapes while keeping the
    // shared DX12 vertex upload bounded even in sphere-heavy regions.
    limit=std::min<std::size_t>(limit,kMaxQualityPrimitiveShapes);
    max_vertices=std::min<std::size_t>(max_vertices,4000000U);
    struct Draw {const Transform* item;Pose pose;bool child;float distance;};
    std::vector<Draw> chosen;
    chosen.reserve(roots.size()+children.size());
    const auto collect=[&](const Transform& t,Pose p,bool child){
        if(t.reflection_probe||t.primitive==Kind::Unknown)return;
        if(!finite(p.pos)||!valid_quat(p.rot)){
            ++result.invalid_pose_shapes;
            return;
        }
        const V d=sub(p.pos,origin);
        // Allow structures beyond the old 96m horizontal / 60m vertical
        // gate. The 128-shape closest-first budget caps GPU uploads.
        if(std::abs(d.x)>144.f||std::abs(d.y)>144.f||
           std::abs(d.z)>128.f){
            ++result.out_of_range_shapes;
            return;
        }
        ++result.eligible_shapes;
        chosen.push_back({&t,p,child,d.x*d.x+d.y*d.y+d.z*d.z});
    };
    for(const auto& [id,t]:roots){
        (void)id;
        if(t.parent_id==0U)collect(t,{pos(t),quat(t)},false);
    }
    std::set<std::uint32_t> missing_ids;
    for(const auto& [id,t]:children){
        (void)id;
        Pose p{};
        if(t.primitive==Kind::Unknown)continue;
        std::uint32_t missing_id=0U;
        switch(resolve(t,roots,children,avatar_roots,p,missing_id)){
        case RootResult::Found:collect(t,p,true);break;
        case RootResult::Missing:
            ++result.missing_anchor_shapes;
            missing_ids.insert(missing_id);
            ++result.unresolved_parent_shapes;break;
        case RootResult::Avatar:
            ++result.avatar_attachment_shapes;
            ++result.unresolved_parent_shapes;break;
        case RootResult::Cycle:
            ++result.cycle_shapes;
            ++result.unresolved_parent_shapes;break;
        case RootResult::Invalid:
            ++result.invalid_parent_shapes;
            ++result.unresolved_parent_shapes;break;
        case RootResult::TooDeep:
            ++result.deep_parent_shapes;
            ++result.unresolved_parent_shapes;break;
        }
    }
    result.unique_missing_ids=missing_ids.size();
    std::sort(chosen.begin(),chosen.end(),
        [](const Draw& a,const Draw& b){return a.distance<b.distance;});
    if(chosen.size()>limit){
        result.budget_deferred_shapes=chosen.size()-limit;
        chosen.resize(limit);
    }
    auto& vertices=result.scene.vertices;
    // Reserve for common low-poly shapes, not the 1,200-vertex sphere
    // worst case multiplied by 4,080. Grow only when actually needed.
    vertices.reserve(std::min(max_vertices,chosen.size()*120U));
    for(const auto& item:chosen){
        // Swept LLVolume shapes can be substantially denser than their
        // primitive family (a hollow linear cylinder can exceed 4,000
        // vertices). Generate one bounded candidate, then commit it only
        // when the complete shape fits the remaining scene budget.
        // Rolling back the candidate also rolls back its texture accounting.
        const std::size_t shape_start=vertices.size();
        const std::size_t textured_start=result.textured_triangles;
        if(!emit(vertices,*item.item,item.pose,origin,
                texture_layers,result.textured_triangles,pbr_faces))
            continue;
        const std::size_t emitted=vertices.size()-shape_start;
        if(emitted>max_vertices-shape_start){
            vertices.resize(shape_start);
            result.textured_triangles=textured_start;
            ++result.budget_deferred_shapes;
            continue;
        }
        if(item.child)++result.linked_shapes;
        else ++result.root_shapes;
    }
    result.scene.terrain_count=vertices.size();
    return result;
}

std::map<std::string,std::uint16_t> select_visible_mesh_texture_layers(
    const std::map<std::uint32_t,Transform>& roots,
    const std::map<std::uint32_t,Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,protocol::mesh::LOD>& loaded,
    const std::map<std::string,protocol::texture::DecodedTexture>& decoded,
    float agent_x,float agent_y,float agent_z,
    std::size_t max_layers,
    const std::map<std::string,std::uint16_t>& previous,
    const protocol::pbr::Materials& materials,const protocol::pbr::Overrides& overrides,
    const std::vector<std::string>& extra_images) {
    // Keep the original single-draw DX12 array budget (slice 0 is the
    // procedural fallback). Never select textures based on UUID sorting.
    max_layers=std::min<std::size_t>(max_layers,protocol::texture::kGPUImageLayers);
    const V origin{agent_x,agent_y,agent_z};
    if(!max_layers||!finite(origin))return {};
    std::map<std::string,float> nearest;
    // Hundreds of SL objects often instance the same mesh LOD. Decoding
    // its UV-bearing material faces for every instance repeatedly scanned
    // thousands of triangles. Cache a compact 64-bit face mask per asset
    // for this world rebuild, then look up only those material slots.
    std::map<std::string,std::uint64_t> drawable_faces;
    const auto gather=[&](const Transform& t,Pose pose) {
        if(!finite(pose.pos)||!valid_quat(pose.rot))return;
        const bool is_mesh=!t.mesh_asset_id.empty();
        const auto mesh=loaded.find(t.mesh_asset_id);
        if(is_mesh&&mesh==loaded.end())return;
        if(!is_mesh&&t.primitive==Kind::Unknown)return;
        const V delta=sub(pose.pos,origin);
        if(std::abs(delta.x)>144.f||std::abs(delta.y)>144.f||
           std::abs(delta.z)>128.f)return;
        const float distance=dot(delta,delta);
        std::uint64_t mask=0U;
        if(is_mesh){
            const auto [faces_it,inserted_faces]=
                drawable_faces.try_emplace(t.mesh_asset_id,std::uint64_t{0});
            if(inserted_faces){
                for(const auto& face:mesh->second.triangles){
                    if(face.has_uv&&face.face_index<45U)
                        faces_it->second|=(std::uint64_t{1}<<face.face_index);
                    if(faces_it->second==((std::uint64_t{1}<<45U)-1U))break;
                }
            }
            mask=faces_it->second;
        }else{
            // Supported simple-prim preview exposes only genuine
            // material-capable faces: cube six, cylinder three,
            // prism five, sphere one. Mesh/sculpt Unknown is excluded.
            const unsigned faces=t.primitive==Kind::Box?6U:
                t.primitive==Kind::Cylinder?3U:
                t.primitive==Kind::Prism?5U:1U;
            mask=(std::uint32_t{1}<<faces)-1U;
        }
        if(mask==0U)return;
        for(unsigned f=0U;f<45U;++f){
            if((mask&(std::uint64_t{1}<<f))==0U)continue;
            protocol::pbr::Material material;
            const bool gltf=protocol::pbr::effective(t.face_material_ids[f],t.local_id,f,materials,overrides,material);
            std::vector<std::string> demand;
            if(gltf)demand.assign(material.images.begin(),material.images.end());
            else demand.push_back(t.face_texture_override[f]?t.face_texture_asset_ids[f]:t.default_texture_asset_id);
            for(const auto& uuid:demand){
            if(uuid.empty())continue; // explicit null UUID is not default
            const auto image=decoded.find(uuid);
            if(image==decoded.end())continue;
            const auto& bitmap=image->second;
            const auto pixels=std::uint64_t(bitmap.width)*bitmap.height;
            if(!bitmap.width||!bitmap.height||bitmap.width>2048U||
               bitmap.height>2048U||pixels>4U*1024U*1024U||
               bitmap.rgba.size()!=pixels*4U)continue;
            const auto [it,inserted]=nearest.try_emplace(uuid,distance);
            if(!inserted)it->second=std::min(it->second,distance);
            }
        }
    };
    for(const auto& uuid:extra_images){
        const auto image=decoded.find(uuid);
        if(image!=decoded.end()&&image->second.width&&image->second.height&&
           image->second.width<=2048U&&image->second.height<=2048U&&
           image->second.rgba.size()==std::uint64_t(image->second.width)*image->second.height*4U)
            nearest.insert_or_assign(uuid,-1.f);
    }
    for(const auto& [id,root]:roots){
        (void)id;
        if(root.parent_id==0U&&
           (!root.mesh_asset_id.empty()||root.primitive!=Kind::Unknown))
            gather(root,{pos(root),quat(root)});
    }
    for(const auto& [id,child]:children){
        (void)id;
        if(child.mesh_asset_id.empty()&&child.primitive==Kind::Unknown)
            continue;
        Pose pose{};
        std::uint32_t missing=0U;
        if(resolve(child,roots,children,avatar_roots,pose,missing)==
           RootResult::Found)gather(child,pose);
    }
    std::vector<std::pair<std::string,float>> candidates(
        nearest.begin(),nearest.end());
    std::sort(candidates.begin(),candidates.end(),
        [](const auto& a,const auto& b){
            if(a.second!=b.second)return a.second<b.second;
            return a.first<b.first;
        });
    if(candidates.size()>max_layers)candidates.resize(max_layers);
    std::map<std::string,std::uint16_t> selected;
    std::array<bool,protocol::texture::kGPUImageLayers+1U> occupied{};
    occupied[0]=true;
    // Keep unchanged material IDs on the same GPU slice if still visible.
    for(const auto& [uuid,distance]:candidates){
        (void)distance;
        const auto it=previous.find(uuid);
        if(it==previous.end()||it->second==0U||
           it->second>max_layers||occupied[it->second])continue;
        selected.emplace(uuid,it->second);
        occupied[it->second]=true;
    }
    std::uint16_t slot=1U;
    for(const auto& [uuid,distance]:candidates){
        (void)distance;
        if(selected.contains(uuid))continue;
        while(slot<=max_layers&&occupied[slot])++slot;
        if(slot>max_layers)break;
        selected.emplace(uuid,slot);
        occupied[slot]=true;
    }
    return selected;
}

MeshBatch build_live_mesh_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,protocol::mesh::LOD>& loaded,
    float agent_x,float agent_y,float agent_z,std::size_t max_triangles,
    const std::map<std::string,std::uint16_t>& texture_layers,const PBRFaceIndices& pbr_faces){
    MeshBatch batch{};
    const V origin{agent_x,agent_y,agent_z};
    if(!finite(origin))return batch;
    // Keep the complete-object admission invariant while permitting larger
    // dense building sections when the caller's vertex budget allows them.
    // Do not truncate a decoded mesh mid-object.
    max_triangles=std::min<std::size_t>(max_triangles,2000000U);
    struct Item {
        const Transform* transform;
        const protocol::mesh::LOD* lod;
        Pose pose;
        bool child;
        float distance;
    };
    std::vector<Item> chosen;
    const auto try_add=[&](const Transform& t,Pose pose,bool child){
        if(t.reflection_probe||t.mesh_asset_id.empty())return;
        if(!finite(pose.pos)||!valid_quat(pose.rot)){
            ++batch.invalid_pose_instances;return;
        }
        const V delta=sub(pose.pos,origin);
        if(std::abs(delta.x)>144.f||std::abs(delta.y)>144.f||
           std::abs(delta.z)>128.f){
            ++batch.out_of_range_instances;return;
        }
        const auto found=loaded.find(t.mesh_asset_id);
        if(found==loaded.end()||found->second.triangles.empty()){
            ++batch.missing_lod_instances;return;
        }
        chosen.push_back({&t,&found->second,pose,child,dot(delta,delta)});
    };
    for(const auto& [id,t]:roots){
        (void)id;
        if(t.parent_id==0U)try_add(t,{pos(t),quat(t)},false);
    }
    for(const auto& [id,t]:children){
        (void)id;
        if(t.mesh_asset_id.empty()||t.reflection_probe)continue;
        Pose pose{};std::uint32_t missing=0U;
        switch(resolve(t,roots,children,avatar_roots,pose,missing)){
        case RootResult::Found:try_add(t,pose,true);break;
        case RootResult::Missing:++batch.parent_missing_instances;
            ++batch.waiting_for_parent;break;
        case RootResult::Avatar:++batch.parent_avatar_instances;
            ++batch.waiting_for_parent;break;
        case RootResult::Cycle:++batch.parent_cycle_instances;
            ++batch.waiting_for_parent;break;
        case RootResult::Invalid:++batch.parent_invalid_instances;
            ++batch.waiting_for_parent;break;
        case RootResult::TooDeep:++batch.parent_too_deep_instances;
            ++batch.waiting_for_parent;break;
        }
    }
    // R78: preserve near-first ordering across 16-metre distance bands,
    // but favor lower-cost meshes inside each band. This helps small
    // furnishings draw instead of losing the entire budget to a handful
    // of dense assets at nearly the same distance. No geometry is faked.
    std::sort(chosen.begin(),chosen.end(),
       [](const Item& a,const Item& b){
           const auto band_a=static_cast<unsigned>(std::sqrt(a.distance)/16.f);
           const auto band_b=static_cast<unsigned>(std::sqrt(b.distance)/16.f);
           if(band_a!=band_b)return band_a<band_b;
           const auto triangles_a=a.lod->triangles.size();
           const auto triangles_b=b.lod->triangles.size();
           if(triangles_a!=triangles_b)return triangles_a<triangles_b;
           if(a.distance!=b.distance)return a.distance<b.distance;
           // Stable identity breaks exact ties, preventing objects from
           // changing draw priority when decoded-asset maps are refreshed.
           return a.transform->local_id<b.transform->local_id;
       });
    batch.eligible_instances=chosen.size();
    auto& verts=batch.scene.vertices;
    verts.reserve(std::min<std::size_t>(max_triangles,12000U)*3U);
    // R76: allow medium-complexity furniture (up to 4096 triangles) while\n    // retaining R74's stable distance order and overall triangle budget.\n    // R74: never spend the entire scene budget on a few dense meshes.
    // Give each nearby instance a bounded share, so small furnishings and
    // buildings can continue to draw even after a large mesh is skipped.
    // Do not truncate a mesh mid-object: partial triangles create holes.
    const std::size_t fair_share=std::max<std::size_t>(24U,
        max_triangles/std::max<std::size_t>(1U,chosen.size()));
    // R79: reserve a modest share for complex nearby building meshes.
    // Keep the R78 small-mesh priority, but allow authentic 4K-8K triangle
    // assets in the closest 16m band when there is enough headroom.
    // The global triangle ceiling remains unchanged.
    const std::size_t per_object_cap=std::min<std::size_t>(
        max_triangles,std::max<std::size_t>(fair_share*4U,4096U));
    // R87: balanced near-to-far admission. The previous three-pass
    // scheduler always emitted all cheap foliage/decor before ANY complex
    // building mesh. On dense regions it exhausted the global budget before
    // architecture got a chance. Interleave lightweight, medium and complex
    // objects within distance bands instead, so nearby buildings compete
    // fairly for capacity without fabricating or truncating geometry.
    std::vector<std::size_t> draw_order;
    draw_order.reserve(chosen.size());
    const auto class_of=[&](std::size_t i)->unsigned {
        const auto triangles=chosen[i].lod->triangles.size();
        return triangles<=1024U?0U:triangles<=per_object_cap?1U:2U;
    };
    // Preserve 16m distance bands across ALL complexity classes. The
    // previous R87 round robin let far-away complex meshes outrank nearby
    // walls because its three queues were global to the entire region.
    // A single object must never enter draw_order more than once.
    std::map<unsigned,std::array<std::vector<std::size_t>,3>> bands;
    for(std::size_t i=0U;i<chosen.size();++i){
        const auto band=static_cast<unsigned>(
            std::sqrt(chosen[i].distance)/16.f);
        bands[band][class_of(i)].push_back(i);
    }
    // Within each 16m band, favor the genuinely closest structural meshes.
    // Map iteration order is object-ID order, not distance order; in dense
    // scenes that could spend the last available triangles on farther walls
    // while nearer walls remained invisible. Stable sorting keeps tie order.
    for(auto& [band,queues]:bands){
        (void)band;
        for(auto& queue:queues){
            std::stable_sort(queue.begin(),queue.end(),
                [&](std::size_t a,std::size_t b){
                    return chosen[a].distance<chosen[b].distance;
                });
        }
        std::array<std::size_t,3> cursor{0U,0U,0U};
        const std::size_t total=queues[0].size()+queues[1].size()+
                                queues[2].size();
        std::size_t admitted=0U;
        while(admitted<total){
            for(const unsigned category:{2U,0U,1U}){
                if(cursor[category]<queues[category].size()){
                    draw_order.push_back(
                        queues[category][cursor[category]++]);
                    ++admitted;
                }
            }
        }
    }
    // Per-object caps apply to ordinary meshes; complex meshes are
    // independently admitted against the actual remaining global budget.
    for(std::size_t index=0U;index<draw_order.size();++index){
        const auto& item=chosen[draw_order[index]];
        const auto triangles=item.lod->triangles.size();
        const auto remaining=max_triangles-batch.emitted_triangles;
        if(remaining==0U){
            batch.undrawn_budget_instances+=draw_order.size()-index;
            batch.undrawn_global_limit+=draw_order.size()-index;
            break;
        }
        // The first pass is guaranteed the first opportunity to fill the
        // scene. The second pass cannot displace its already emitted meshes.
        // R85: keep the inexpensive-first R81 pass, but permit a complex
        // mesh that fits the remaining GLOBAL budget even when it exceeds
        // the old per-object cap. Otherwise complete building sections
        // vanish despite having decoded LOD geometry.
        // Nearby structural meshes must not disappear solely because the
        // fair-share cap is smaller than the available global budget. Keep
        // the global triangle ceiling and full-object admission invariant.
        // More distant objects retain fair-share scheduling to protect FPS.
        // The global budget is the authoritative capacity guard. Once a
        // complete mesh fits it, rejecting that mesh solely on fair-share
        // size leaves real walls/floors invisible despite available space.
        // Distance-band ordering already prioritizes nearby structures.
        // Never partially draw a decoded mesh, even when it is the only
        // eligible object. A partial object can look like missing walls.
        if(triangles>remaining){
            ++batch.undrawn_budget_instances;
            if(triangles>remaining)++batch.undrawn_global_limit;
            else ++batch.undrawn_object_limit;
            continue;
        }
        const std::size_t first_object_vertex=verts.size();
        std::size_t object_triangles=0U;
        std::size_t transparent=0U,invalid_geometry=0U;
        for(const auto& triangle:item.lod->triangles){
            if(batch.emitted_triangles>=max_triangles)break;
            // Every mesh face is an independent SL material slot. An
            // explicit null override must never fall back to its parent.
            const unsigned face=triangle.face_index;
            const auto& obj=*item.transform;
            const auto pbr=pbr_faces.find(pbr_face_key(obj.local_id,face));
            const auto material_index=pbr==pbr_faces.end()?0U:pbr->second;
            if(material_index==0U&&obj.face_rgba_present&&face<obj.face_rgba.size()&&
               obj.face_rgba[face][3]<=0.f){
                ++transparent;continue;
            }
            const std::string& texture_id=
                face<obj.face_texture_override.size()&&
                obj.face_texture_override[face]?
                  obj.face_texture_asset_ids[face]:
                  obj.default_texture_asset_id;
            const auto layer_it=texture_layers.find(texture_id);
            const std::uint16_t selected_layer=
                layer_it==texture_layers.end()?0U:layer_it->second;
            std::array<V,3> points{};
            bool valid=true;
            for(unsigned v=0U;v<3U;++v){
                const auto i=3U*v;
                const V local{triangle.pos[i]*item.transform->sx,
                              triangle.pos[i+1U]*item.transform->sy,
                              triangle.pos[i+2U]*item.transform->sz};
                const V world=add(item.pose.pos,rotate(item.pose.rot,local));
                points[v]={world.x-origin.x,world.z-origin.z,world.y-origin.y};
                valid=valid&&finite(points[v]);
            }
            if(!valid){++invalid_geometry;continue;}
            // SL coordinate conversion swaps Y/Z handedness. Reverse the
            // winding exactly once; no convex-geometry guesses for meshes.
            std::swap(points[1],points[2]);
            // Keep mesh UV winding in step with the handedness swap.
            const std::array<unsigned,3> original_index{0U,2U,1U};
            const bool textured=selected_layer>0U&&triangle.has_uv;
            const auto a=sub(points[1],points[0]);
            const auto b=sub(points[2],points[0]);
            V normal=cross(a,b);
            const float length=std::sqrt(dot(normal,normal));
            if(!std::isfinite(length)||length<1.e-8f){
                ++invalid_geometry;continue;
            }
            normal=scale(normal,1.f/length);
            for(unsigned v=0U;v<3U;++v){
                const auto& p=points[v];
                const auto& t=*item.transform;
                float red=t.base_tint_present?
                    std::clamp(t.base_r,0.f,1.f):.80f;
                float green=t.base_tint_present?
                    std::clamp(t.base_g,0.f,1.f):.87f;
                float blue=t.base_tint_present?
                    std::clamp(t.base_b,0.f,1.f):.96f;
                if(face<t.face_rgba.size()&&t.face_rgba_present){
                    red=t.face_rgba[face][0];
                    green=t.face_rgba[face][1];
                    blue=t.face_rgba[face][2];
                }
                const auto oi=original_index[v];
                const auto tex_uv=material_index&&triangle.has_uv?
                    UV{triangle.uv[2U*oi],triangle.uv[2U*oi+1U]}:triangle.has_uv?
                    transform_diffuse_uv(triangle.uv[2U*oi],
                        triangle.uv[2U*oi+1U],
                        face<t.face_uv.size()?
                        t.face_uv[face]:protocol::texture::UVTransform{}):
                    UV{0.f,0.f};
                verts.push_back({p.x,p.y,p.z,normal.x,normal.y,normal.z,
                    red,green,blue,.04f,.82f,tex_uv[0],tex_uv[1],
                    0.f,0U,0U,
                    textured?static_cast<float>(selected_layer):0.f,material_index});
            }
            ++object_triangles;
            ++batch.emitted_triangles;
            if(textured)++batch.textured_triangles;
        }
        if(object_triangles){
            // Preserve exact admitted spans; fully transparent and malformed
            // instances have no drawable range. Local IDs are region-scoped.
            batch.instance_ranges.push_back({item.transform->local_id,
                item.transform->mesh_asset_id,first_object_vertex,
                verts.size()-first_object_vertex});
            if(item.child)++batch.linked_meshes;
            else ++batch.root_meshes;
        }else if(transparent){
            ++batch.undrawn_transparent_instances;
        }else if(invalid_geometry){
            ++batch.undrawn_bad_geometry_instances;
        }
        // Remaining budget-deferred objects are counted at the top of
        // the following loop iteration; don't inflate transparency.
    }
    batch.undrawn_eligible_instances=batch.eligible_instances-
        (batch.root_meshes+batch.linked_meshes);
    batch.scene.terrain_count=verts.size();
    return batch;
}
} // namespace kami::world

