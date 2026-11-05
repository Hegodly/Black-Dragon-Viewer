#pragma once
#include "kami/protocol/pbr_material.hpp"
#include "kami/protocol/object_spatial.hpp"
#include <array>
#include <map>
#include <vector>
namespace kami::world {
using PBRFaceIndices=std::map<std::uint64_t,std::uint32_t>;
using PBRRow=std::array<float,4>;
inline std::uint64_t pbr_face_key(std::uint32_t local,unsigned face){return(std::uint64_t(local)<<5U)|face;}
struct PBRTable {
    PBRFaceIndices faces;
    std::vector<PBRRow> rows=std::vector<PBRRow>(protocol::pbr::kRows);
};
inline PBRTable build_pbr_table(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const protocol::pbr::Materials& materials,const protocol::pbr::Overrides& overrides,
    const std::map<std::string,std::uint16_t>& images,
    const std::array<std::string,4>& terrain_assets={}){
    PBRTable table;
    using Record=std::array<PBRRow,protocol::pbr::kRows>;
    std::map<Record,std::uint32_t> records;
    const auto layer=[&](const std::string& id){const auto i=images.find(id);return i==images.end()?0.f:float(i->second);};
    const auto scan=[&](const auto& objects){
        for(const auto& [id,t]:objects)for(unsigned f=0U;f<32U;++f){
            protocol::pbr::Material m;
            if(!protocol::pbr::effective(t.face_material_ids[f],id,f,materials,overrides,m))continue;
            Record record{};
            record[0]=m.base;record[1]={m.emissive[0],m.emissive[1],m.emissive[2],m.normal_scale};
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
    scan(roots);scan(children);
    protocol::objectspatial::Transform land;
    for(unsigned i=0U;i<4U;++i)land.face_material_ids[i]=terrain_assets[i];
    scan(std::map<std::uint32_t,protocol::objectspatial::Transform>{{0U,land}});
    return table;
}
}
