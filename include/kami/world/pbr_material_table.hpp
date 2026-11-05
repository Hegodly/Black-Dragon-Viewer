#pragma once
#include "kami/protocol/pbr_material.hpp"
#include "kami/protocol/object_spatial.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace kami::world {
using PBRFaceIndices=std::map<std::uint64_t,std::uint32_t>;
using PBRRow=std::array<float,4>;
inline std::uint64_t pbr_face_key(std::uint32_t local,unsigned face){
    return (std::uint64_t(local)<<5U)|face;
}
struct PBRTable {
    PBRFaceIndices faces;
    std::vector<PBRRow> rows=std::vector<PBRRow>(protocol::pbr::kRows);
};

// First reserve PBR records for terrain, then favor objects near the
// viewer instead of allowing ascending simulator local IDs to exhaust
// the fixed GPU material table. If an agent is not supplied, preserve
// deterministic legacy order (for standalone tools and tests).
inline PBRTable build_pbr_table(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const protocol::pbr::Materials& materials,const protocol::pbr::Overrides& overrides,
    const std::map<std::string,std::uint16_t>& images,
    const std::array<std::string,4>& terrain_assets={},
    std::optional<std::array<float,3>> agent=std::nullopt){
    PBRTable table;
    using Transform=protocol::objectspatial::Transform;
    using Record=std::array<PBRRow,protocol::pbr::kRows>;
    std::map<Record,std::uint32_t> records;
    const auto layer=[&](const std::string& id){
        const auto i=images.find(id);
        return i==images.end()?0.f:float(i->second);
    };
    const auto scan_object=[&](std::uint32_t id,const Transform& t){
        if(t.reflection_probe)return;
        for(unsigned f=0U;f<32U;++f){
            protocol::pbr::Material m;
            if(!protocol::pbr::effective(t.face_material_ids[f],id,f,materials,overrides,m))
                continue;
            Record record{};
            record[0]=m.base;
            record[1]={m.emissive[0],m.emissive[1],m.emissive[2],m.normal_scale};
            record[2]={m.metallic,m.roughness,m.occlusion_strength,m.cutoff};
            record[3]={layer(m.images[0]),layer(m.images[1]),layer(m.images[2]),layer(m.images[3])};
            record[4]={float(m.alpha),m.double_sided?1.f:0.f,m.unlit?1.f:0.f,layer(m.images[4])};
            for(unsigned s=0;s<protocol::pbr::Slots;++s){
                record[5U+s*2U]={m.uv[s].scale[0],m.uv[s].scale[1],m.uv[s].rotation,0.f};
                record[6U+s*2U]={m.uv[s].offset[0],m.uv[s].offset[1],0.f,0.f};
            }
            auto it=records.find(record);
            if(it==records.end()){
                if(records.size()>=protocol::pbr::kMaxGPURecords-1U)continue;
                const auto index=static_cast<std::uint32_t>(records.size()+1U);
                it=records.emplace(record,index).first;
                table.rows.insert(table.rows.end(),record.begin(),record.end());
            }
            table.faces.emplace(pbr_face_key(id,f),it->second);
        }
    };
    Transform land;
    for(unsigned i=0U;i<4U;++i)land.face_material_ids[i]=terrain_assets[i];
    scan_object(0U,land);

    if(!agent){
        for(const auto& [id,t]:roots)scan_object(id,t);
        for(const auto& [id,t]:children)scan_object(id,t);
        return table;
    }

    // Resolve a verified root anchor for each child. Missing roots,
    // invalid transforms and cyclic parent chains must not reserve
    // scarce GPU PBR slots for shapes that cannot render at all.
    const auto root_position=[&](const Transform& t)
            ->std::optional<std::array<float,3>> {
        const Transform* current=&t;
        std::array<std::uint32_t,16U> seen{};
        unsigned visited=0U;
        while(current->parent_id!=0U){
            if(visited>=seen.size())return std::nullopt;
            seen[visited++]=current->local_id;
            const auto parent_id=current->parent_id;
            if(std::find(seen.begin(),seen.begin()+visited,parent_id)!=
               seen.begin()+visited)return std::nullopt;
            const auto root=roots.find(parent_id);
            if(root!=roots.end())current=&root->second;
            else {
                const auto child=children.find(parent_id);
                if(child==children.end())return std::nullopt;
                current=&child->second;
            }
        }
        if(!std::isfinite(current->x)||!std::isfinite(current->y)||
           !std::isfinite(current->z))return std::nullopt;
        return std::array<float,3>{current->x,current->y,current->z};
    };
    struct Candidate {
        const Transform* object;
        float distance;
    };
    std::vector<Candidate> sorted;
    sorted.reserve(roots.size()+children.size());
    const auto add=[&](const Transform& t){
        if(t.reflection_probe||
            (t.primitive==protocol::objectspatial::PrimitiveKind::Unknown&&
             t.mesh_asset_id.empty()))return;
        const auto pos=root_position(t);
        if(!pos)return;
        const float dx=(*pos)[0]-(*agent)[0];
        const float dy=(*pos)[1]-(*agent)[1];
        const float dz=(*pos)[2]-(*agent)[2];
        if(!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(dz))return;
        sorted.push_back({&t,dx*dx+dy*dy+dz*dz});
    };
    for(const auto& [id,t]:roots){(void)id;if(t.parent_id==0U)add(t);}
    for(const auto& [id,t]:children){(void)id;add(t);}
    std::sort(sorted.begin(),sorted.end(),
        [](const Candidate& a,const Candidate& b){
            if(a.distance!=b.distance)return a.distance<b.distance;
            return a.object->local_id<b.object->local_id;
        });
    for(const auto& item:sorted)scan_object(item.object->local_id,*item.object);
    return table;
}
} // namespace kami::world
