#include "kami/world/live_primitive_geometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <set>

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
struct Emitter {
    std::vector<Vertex>& out;
    Pose pose;
    V half;
    V origin;
    float red=.78f,green=.83f,blue=.92f;
    V transform(V v) const noexcept {
        return add(pose.pos,rotate(pose.rot,
            {v.x*half.x*2.f,v.y*half.y*2.f,v.z*half.z*2.f}));
    }
    V render(V v) const noexcept {
        return {v.x-origin.x,v.z-origin.z,v.y-origin.y};
    }
    void tri(V x,V y,V z){
        // Construct normals from the final DX12 Y-up transform.
        // The SL->DX12 axis permutation reverses handedness.
        const V local_center=scale(add(add(x,y),z),1.f/3.f);
        V a=render(transform(x)),b=render(transform(y)),c=render(transform(z));
        const V normal=cross(sub(b,a),sub(c,a));
        // Point outward on convex elementary primitives, including
        // caps and the sphere poles, despite the handedness conversion.
        const V expected=render(add(pose.pos,rotate(pose.rot,
            {local_center.x*half.x*2.f,
             local_center.y*half.y*2.f,
             local_center.z*half.z*2.f})));
        const V center=render(pose.pos);
        V n=normal;
        if(dot(n,sub(expected,center))<0.f){std::swap(b,c);n=scale(n,-1.f);}
        const float len=std::sqrt(dot(n,n));
        if(len<1.e-6f||!std::isfinite(len))return;
        n=scale(n,1.f/len);
        if(!finite(a)||!finite(b)||!finite(c))return;
        for(const V p:{a,b,c})
            out.push_back({p.x,p.y,p.z,n.x,n.y,n.z,
                red,green,blue,.06f,.79f,0.f,0.f});
    }
    void quad(V a,V b,V c,V d){tri(a,b,c);tri(a,c,d);}
};
void build_box(Emitter& e) {
    const std::array<V,8> p={{
      {-.5f,-.5f,-.5f},{.5f,-.5f,-.5f},
      {.5f,.5f,-.5f},{-.5f,.5f,-.5f},
      {-.5f,-.5f,.5f},{.5f,-.5f,.5f},
      {.5f,.5f,.5f},{-.5f,.5f,.5f}
    }};
    e.quad(p[4],p[5],p[6],p[7]);
    e.quad(p[1],p[0],p[3],p[2]);
    e.quad(p[0],p[4],p[7],p[3]);
    e.quad(p[5],p[1],p[2],p[6]);
    e.quad(p[3],p[7],p[6],p[2]);
    e.quad(p[0],p[1],p[5],p[4]);
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
        e.quad(lo,lon,hin,hi);
        e.tri({0,0,.5f},hi,hin);
        e.tri({0,0,-.5f},lon,lo);
    }
}
void build_prism(Emitter& e){
    // Strictly classified straight, uncut, unhollowed triangular prism.
    constexpr float y=-0.2886751346f;
    const V a{-.5f,y,-.5f}, b{.5f,y,-.5f},c{0.f,-2.f*y,-.5f};
    const V d{a.x,a.y,.5f},f{b.x,b.y,.5f},g{c.x,c.y,.5f};
    e.tri(a,c,b);e.tri(d,f,g);
    e.quad(a,b,f,d);e.quad(b,c,g,f);e.quad(c,a,d,g);
}
void build_sphere(Emitter& e){
    constexpr unsigned lat=10U,lon=20U;
    constexpr float pi=3.14159265358979323846f;
    const auto vertex=[](float t,float p)->V{
        return {std::sin(t)*std::cos(p)*.5f,
                std::sin(t)*std::sin(p)*.5f,std::cos(t)*.5f};
    };
    for(unsigned row=0;row<lat;++row){
        const float a=pi*static_cast<float>(row)/lat;
        const float b=pi*static_cast<float>(row+1)/lat;
        for(unsigned col=0;col<lon;++col){
            const float x=2*pi*static_cast<float>(col)/lon;
            const float y=2*pi*static_cast<float>(col+1)/lon;
            e.quad(vertex(a,x),vertex(a,y),vertex(b,y),vertex(b,x));
        }
    }
}
bool emit(std::vector<Vertex>& out,const Transform& t,Pose p,V origin){
    if(t.primitive==Kind::Unknown||!valid_quat(p.rot)||!finite(p.pos))
        return false;
    const std::size_t start=out.size();
    Emitter e{out,p,{t.sx*.5f,t.sy*.5f,t.sz*.5f},origin};
    if(t.base_tint_present){
        // Use the actual region-provided default face tint. This is a
        // solid-color approximation, NOT a substitute for JPEG2000/PBR.
        e.red=std::clamp(t.base_r,0.f,1.f);
        e.green=std::clamp(t.base_g,0.f,1.f);
        e.blue=std::clamp(t.base_b,0.f,1.f);
    }
    switch(t.primitive){
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
    const std::set<std::uint32_t>& avatar_roots){
    PrimitiveBatch result{};
    const V origin{agent_x,agent_y,agent_z};
    if(!finite(origin))return result;
    // R32: larger closest-first batch with an upper bound on the DX12
    // vertex workload. Mesh asset geometry remains a separate pipeline.
    limit=std::min<std::size_t>(limit,128U);
    struct Draw {const Transform* item;Pose pose;bool child;float distance;};
    std::vector<Draw> chosen;
    chosen.reserve(roots.size()+children.size());
    const auto collect=[&](const Transform& t,Pose p,bool child){
        if(t.primitive==Kind::Unknown)return;
        if(!finite(p.pos)||!valid_quat(p.rot)){
            ++result.invalid_pose_shapes;
            return;
        }
        const V d=sub(p.pos,origin);
        // Allow structures beyond the old 96m horizontal / 60m vertical
        // gate. The 128-shape closest-first budget caps GPU uploads.
        if(std::abs(d.x)>144.f||std::abs(d.y)>144.f||
           std::abs(d.z)>96.f){
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
    vertices.reserve(chosen.size()*1200U);
    for(const auto& item:chosen){
        if(emit(vertices,*item.item,item.pose,origin)){
            if(item.child)++result.linked_shapes;
            else ++result.root_shapes;
        }
    }
    result.scene.terrain_count=vertices.size();
    return result;
}

MeshBatch build_live_mesh_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,protocol::mesh::LOD>& loaded,
    float agent_x,float agent_y,float agent_z,std::size_t max_triangles){
    MeshBatch batch{};
    const V origin{agent_x,agent_y,agent_z};
    if(!finite(origin))return batch;
    max_triangles=std::min<std::size_t>(max_triangles,24000U);
    struct Item {
        const Transform* transform;
        const protocol::mesh::LOD* lod;
        Pose pose;
        bool child;
        float distance;
    };
    std::vector<Item> chosen;
    const auto try_add=[&](const Transform& t,Pose pose,bool child){
        if(t.mesh_asset_id.empty())return;
        const auto found=loaded.find(t.mesh_asset_id);
        if(found==loaded.end()||found->second.triangles.empty())return;
        if(!finite(pose.pos)||!valid_quat(pose.rot))return;
        const V delta=sub(pose.pos,origin);
        if(std::abs(delta.x)>144.f||std::abs(delta.y)>144.f||
           std::abs(delta.z)>96.f)return;
        chosen.push_back({&t,&found->second,pose,child,dot(delta,delta)});
    };
    for(const auto& [id,t]:roots){
        (void)id;
        if(t.parent_id==0U)try_add(t,{pos(t),quat(t)},false);
    }
    for(const auto& [id,t]:children){
        (void)id;
        if(t.mesh_asset_id.empty()||!loaded.contains(t.mesh_asset_id))
            continue;
        Pose pose{};std::uint32_t missing=0U;
        if(resolve(t,roots,children,avatar_roots,pose,missing)==
           RootResult::Found)try_add(t,pose,true);
        else ++batch.waiting_for_parent;
    }
    std::sort(chosen.begin(),chosen.end(),
       [](const Item& a,const Item& b){return a.distance<b.distance;});
    auto& verts=batch.scene.vertices;
    verts.reserve(std::min<std::size_t>(max_triangles,12000U)*3U);
    for(const auto& item:chosen){
        std::size_t object_triangles=0U;
        for(const auto& triangle:item.lod->triangles){
            if(batch.emitted_triangles>=max_triangles)break;
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
            if(!valid)continue;
            // SL coordinate conversion swaps Y/Z handedness. Reverse the
            // winding exactly once; no convex-geometry guesses for meshes.
            std::swap(points[1],points[2]);
            const auto a=sub(points[1],points[0]);
            const auto b=sub(points[2],points[0]);
            V normal=cross(a,b);
            const float length=std::sqrt(dot(normal,normal));
            if(!std::isfinite(length)||length<1.e-8f)continue;
            normal=scale(normal,1.f/length);
            for(unsigned v=0U;v<3U;++v){
                const auto& p=points[v];
                const auto& t=*item.transform;
                const float red=t.base_tint_present?
                    std::clamp(t.base_r,0.f,1.f):.80f;
                const float green=t.base_tint_present?
                    std::clamp(t.base_g,0.f,1.f):.87f;
                const float blue=t.base_tint_present?
                    std::clamp(t.base_b,0.f,1.f):.96f;
                verts.push_back({p.x,p.y,p.z,normal.x,normal.y,normal.z,
                                 red,green,blue,.04f,.82f,0.f,0.f});
            }
            ++object_triangles;
            ++batch.emitted_triangles;
        }
        if(object_triangles){
            if(item.child)++batch.linked_meshes;
            else ++batch.root_meshes;
        }
        if(batch.emitted_triangles>=max_triangles)break;
    }
    batch.scene.terrain_count=verts.size();
    return batch;
}
} // namespace kami::world
