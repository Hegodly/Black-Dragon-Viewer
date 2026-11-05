#include "kami/world/live_primitive_geometry.hpp"
#include "kami/world/texture_uv.hpp"
#include "kami/protocol/texture_budget.hpp"
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
using UV=std::array<float,2U>;
struct Emitter {
    std::vector<Vertex>& out;
    const Transform& material;
    const std::map<std::string,std::uint16_t>& layers;
    Pose pose;
    V half;
    V origin;
    std::size_t& textured_triangles;
    float red=.78f,green=.83f,blue=.92f;
    V transform(V v) const noexcept {
        return add(pose.pos,rotate(pose.rot,
            {v.x*half.x*2.f,v.y*half.y*2.f,v.z*half.z*2.f}));
    }
    V render(V v) const noexcept {
        return {v.x-origin.x,v.z-origin.z,v.y-origin.y};
    }
    void tri(V x,V y,V z,unsigned face,UV uvx,UV uvy,UV uvz){
        const V local_center=scale(add(add(x,y),z),1.f/3.f);
        V a=render(transform(x)),b=render(transform(y)),c=render(transform(z));
        V normal=cross(sub(b,a),sub(c,a));
        const V expected=render(add(pose.pos,rotate(pose.rot,
            {local_center.x*half.x*2.f,
             local_center.y*half.y*2.f,
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
            const auto uv=transform_diffuse_uv(coords[i][0],coords[i][1],uv_param);
            out.push_back({positions[i].x,positions[i].y,positions[i].z,
                normal.x,normal.y,normal.z,r,g,bl,.06f,.79f,
                uv[0],uv[1],0.f,0U,0U,static_cast<float>(layer)});
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
          std::size_t& textured_triangles){
    if(t.primitive==Kind::Unknown||!valid_quat(p.rot)||!finite(p.pos))
        return false;
    const std::size_t start=out.size();
    Emitter e{out,t,layers,p,
              {t.sx*.5f,t.sy*.5f,t.sz*.5f},origin,textured_triangles};
    if(t.base_tint_present){
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
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,std::uint16_t>& texture_layers){
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
        if(emit(vertices,*item.item,item.pose,origin,
                texture_layers,result.textured_triangles)){
            if(item.child)++result.linked_shapes;
            else ++result.root_shapes;
        }
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
    const std::map<std::string,std::uint16_t>& previous) {
    // Keep the original single-draw DX12 array budget (slice 0 is the
    // procedural fallback). Never select textures based on UUID sorting.
    max_layers=std::min<std::size_t>(max_layers,protocol::texture::kGPUImageLayers);
    const V origin{agent_x,agent_y,agent_z};
    if(!max_layers||!finite(origin))return {};
    std::map<std::string,float> nearest;
    // Hundreds of SL objects often instance the same mesh LOD. Decoding
    // its UV-bearing material faces for every instance repeatedly scanned
    // thousands of triangles. Cache a compact 32-bit face mask per asset
    // for this world rebuild, then look up only those material slots.
    std::map<std::string,std::uint32_t> drawable_faces;
    const auto gather=[&](const Transform& t,Pose pose) {
        if(!finite(pose.pos)||!valid_quat(pose.rot))return;
        const bool is_mesh=!t.mesh_asset_id.empty();
        const auto mesh=loaded.find(t.mesh_asset_id);
        if(is_mesh&&mesh==loaded.end())return;
        if(!is_mesh&&t.primitive==Kind::Unknown)return;
        const V delta=sub(pose.pos,origin);
        if(std::abs(delta.x)>144.f||std::abs(delta.y)>144.f||
           std::abs(delta.z)>96.f)return;
        const float distance=dot(delta,delta);
        std::uint32_t mask=0U;
        if(is_mesh){
            const auto [faces_it,inserted_faces]=
                drawable_faces.try_emplace(t.mesh_asset_id,0U);
            if(inserted_faces){
                for(const auto& face:mesh->second.triangles){
                    if(face.has_uv&&face.face_index<32U)
                        faces_it->second|=(std::uint32_t{1}<<face.face_index);
                    if(faces_it->second==0xffffffffU)break;
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
        for(unsigned f=0U;f<32U;++f){
            if((mask&(std::uint32_t{1}<<f))==0U)continue;
            const auto& uuid=t.face_texture_override[f]?
                t.face_texture_asset_ids[f]:t.default_texture_asset_id;
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
    };
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
    const std::map<std::string,std::uint16_t>& texture_layers){
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
            // Every mesh face is an independent SL material slot. An
            // explicit null override must never fall back to its parent.
            const unsigned face=triangle.face_index;
            const auto& obj=*item.transform;
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
            if(!valid)continue;
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
            if(!std::isfinite(length)||length<1.e-8f)continue;
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
                const auto tex_uv=triangle.has_uv?
                    transform_diffuse_uv(triangle.uv[2U*oi],
                        triangle.uv[2U*oi+1U],
                        face<t.face_uv.size()?
                        t.face_uv[face]:protocol::texture::UVTransform{}):
                    UV{0.f,0.f};
                verts.push_back({p.x,p.y,p.z,normal.x,normal.y,normal.z,
                    red,green,blue,.04f,.82f,tex_uv[0],tex_uv[1],
                    0.f,0U,0U,
                    textured?static_cast<float>(selected_layer):0.f});
            }
            ++object_triangles;
            ++batch.emitted_triangles;
            if(textured)++batch.textured_triangles;
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
