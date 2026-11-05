#include "kami/protocol/official_extra_params.h"
#include "kami/protocol/mesh_reference.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// Offline binary wire fixtures; no simulator, credentials or capability URL.
using Bytes=std::vector<std::uint8_t>;
void extra(Bytes& b,std::uint16_t type,const Bytes& payload){
    b.push_back(static_cast<std::uint8_t>(type));
    b.push_back(static_cast<std::uint8_t>(type>>8));
    const auto len=static_cast<std::uint32_t>(payload.size());
    for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(len>>(8*i)));
    b.insert(b.end(),payload.begin(),payload.end());
}
int main(){
    KamiSlExtraParamsV1 summary{};
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(nullptr,0U,&summary)||
       summary.records!=0U)return 1;
    Bytes fields{6U};
    extra(fields,0x10U,Bytes(16,0U)); // flexible
    extra(fields,0x20U,Bytes(16,0U)); // light
    extra(fields,0x70U,Bytes(4,0U)); // extended mesh *flags*, not asset ID
    extra(fields,0x80U,Bytes(12,0x11U)); // PBR render material
    extra(fields,0x90U,Bytes(9,0U)); // reflection probe
    Bytes sculpt(17,0U);
    for(unsigned i=0;i<16;++i)sculpt[i]=static_cast<std::uint8_t>(i+1U);
    sculpt[16]=0x45U; // mesh type low 6 bits == 5, with upper flags
    extra(fields,0x30U,sculpt);
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(fields.data(),
            static_cast<std::uint32_t>(fields.size()),&summary)||
       summary.records!=6U||summary.extended_mesh!=1U||
       summary.render_material!=1U||summary.reflection_probe!=1U||
       summary.flexible!=1U||summary.light!=1U||
       summary.sculpt!=1U||summary.mesh_sculpt!=1U||
       summary.unknown!=0U)return 2;
    for(unsigned i=0;i<16;++i)
        if(summary.mesh_asset_uuid[i]!=i+1U)return 3;
    const auto mesh=kami::protocol::mesh::mesh_asset_from_extras(fields);
    if(mesh!="01020304-0506-0708-090a-0b0c0d0e0f10")return 4;
    Bytes materials{1U};
    extra(materials,0x80U,Bytes(12,0x22U));
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(materials.data(),
        static_cast<std::uint32_t>(materials.size()),&summary)||
       summary.render_material!=1U||summary.mesh_sculpt!=0U||
       !kami::protocol::mesh::mesh_asset_from_extras(materials).empty())
       return 5;
    Bytes bad{1U};
    extra(bad,0x70U,Bytes(3U,0x00U));
    summary.struct_size=sizeof(summary);
    if(kami_sl_decode_extra_params_v1(bad.data(),
        static_cast<std::uint32_t>(bad.size()),&summary))return 6;
    if(summary.records!=0U||summary.mesh_sculpt!=0U)return 7;
    bad={1U};extra(bad,0x80U,Bytes{0x11U});
    summary.struct_size=sizeof(summary);
    if(kami_sl_decode_extra_params_v1(bad.data(),
        static_cast<std::uint32_t>(bad.size()),&summary))return 8;
    bad={1U};extra(bad,0x30U,Bytes(16U,0));
    summary.struct_size=sizeof(summary);
    if(kami_sl_decode_extra_params_v1(bad.data(),
        static_cast<std::uint32_t>(bad.size()),&summary))return 9;
    bad={1U};extra(bad,0xa0U,Bytes{0x01U}); // future unknown parameter
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(bad.data(),
        static_cast<std::uint32_t>(bad.size()),&summary)||
       summary.unknown!=1U)return 10;
    auto truncated=fields;truncated.pop_back();
    summary.struct_size=sizeof(summary);
    if(kami_sl_decode_extra_params_v1(truncated.data(),
        static_cast<std::uint32_t>(truncated.size()),&summary))return 11;
    summary.struct_size=42U;
    if(kami_sl_decode_extra_params_v1(fields.data(),
        static_cast<std::uint32_t>(fields.size()),&summary))return 12;
    // Official mesh param 0x60 aliases sculpt 0x30 and must yield
    // the same authenticated UUID, but 0x70 remains only flags.
    Bytes alias{1U};
    extra(alias,0x60U,sculpt);
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(alias.data(),
            static_cast<std::uint32_t>(alias.size()),&summary)||
       summary.records!=1U||summary.sculpt!=1U||
       summary.mesh_sculpt!=1U||summary.unknown!=0U||
       kami::protocol::mesh::mesh_asset_from_extras(alias)!=mesh)
       return 13;
    auto bad_alias=alias;
    bad_alias.back()=3U; // ordinary sculpt, not a mesh
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(bad_alias.data(),
            static_cast<std::uint32_t>(bad_alias.size()),&summary)||
       summary.sculpt!=1U||summary.mesh_sculpt!=0U||
       !kami::protocol::mesh::mesh_asset_from_extras(bad_alias).empty())
       return 14;
    bad_alias=Bytes{1U};
    extra(bad_alias,0x60U,Bytes(16U,0U)); // malformed mesh alias
    summary.struct_size=sizeof(summary);
    if(kami_sl_decode_extra_params_v1(bad_alias.data(),
            static_cast<std::uint32_t>(bad_alias.size()),&summary))
       return 15;

    return 0;
}
