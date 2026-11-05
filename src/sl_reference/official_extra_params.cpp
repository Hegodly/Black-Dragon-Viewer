/*
 * Linden Lab ExtraParams wire compatibility adapter.
 * Adapted from Second Life Viewer indra/llprimitive/llprimitive.h and
 * llprimitive.cpp (LLNetworkData::isValid, param codes).
 * Copyright (C) 2010 Linden Research, Inc.
 * Copyright (C) 2026 KAMI contributors for the C ABI and bounds checks.
 * License: GNU Lesser General Public License v2.1 ONLY.
 * Upstream: https://github.com/secondlife/viewer
 * Keep this source and the LGPL notices with the replaceable DLL.
 */
#include "kami/protocol/official_extra_params.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace {
enum : std::uint16_t {
    PARAMS_FLEXIBLE=0x10,PARAMS_LIGHT=0x20,
    PARAMS_SCULPT=0x30,PARAMS_LIGHT_IMAGE=0x40,
    PARAMS_MESH=0x60, PARAMS_EXTENDED_MESH=0x70,
    PARAMS_RENDER_MATERIAL=0x80,PARAMS_REFLECTION_PROBE=0x90
};
bool known_size_valid(std::uint16_t type,std::uint32_t size) noexcept {
    // Official LLViewerObject::unpackParameterEntry remaps 0x60
    // (PARAMS_MESH) to 0x30 (PARAMS_SCULPT). Both carry the same
    // 17-byte payload: sculpt UUID[16] + type[1].
    switch(type){
    case PARAMS_FLEXIBLE: return size==16U;
    case PARAMS_LIGHT: return size==16U;
    case PARAMS_SCULPT:
    case PARAMS_MESH: return size==17U;
    case PARAMS_LIGHT_IMAGE: return size==28U;
    case PARAMS_EXTENDED_MESH: return size==4U;
    case PARAMS_RENDER_MATERIAL: return size>=1U;
    case PARAMS_REFLECTION_PROBE: return size==9U;
    default:return true; // forward compatibility for opaque records
    }
}
std::uint16_t get_u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0])|
           (static_cast<std::uint16_t>(p[1])<<8);
}
std::uint32_t get_u32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])|
        (static_cast<std::uint32_t>(p[1])<<8)|
        (static_cast<std::uint32_t>(p[2])<<16)|
        (static_cast<std::uint32_t>(p[3])<<24);
}
}
extern "C" KAMI_SL_EXTRA_EXPORT int32_t KAMI_SL_EXTRA_CALL
kami_sl_decode_extra_params_v1(const std::uint8_t* bytes,
    std::uint32_t bytes_length,KamiSlExtraParamsV1* output){
    if(!output)return 0;
    const auto supplied=output->struct_size;
    // A client with an incompatible ABI version may provide a smaller
    // output buffer: never overwrite beyond the declared struct size.
    if(supplied!=sizeof(*output))return 0;
    std::memset(output,0,sizeof(*output));
    output->struct_size=sizeof(*output);
    if(bytes_length>8192U||
       (bytes_length!=0U&&!bytes))return 0;
    if(bytes_length==0U)return 1; // Empty optional extra field.
    KamiSlExtraParamsV1 tmp{};
    tmp.struct_size=sizeof(tmp);
    auto at=std::uint32_t{1};
    const std::uint32_t count=bytes[0];
    if(count>32U)return 0;
    for(std::uint32_t i=0;i<count;++i){
        if(at>bytes_length||bytes_length-at<6U)return 0;
        const auto type=get_u16(bytes+at);
        const auto length=get_u32(bytes+at+2U);
        at+=6U;
        if(length>4096U||length>bytes_length-at||
           !known_size_valid(type,length))return 0;
        const auto* data=bytes+at;
        ++tmp.records;
        switch(type) {
        case PARAMS_FLEXIBLE:++tmp.flexible;break;
        case PARAMS_LIGHT:++tmp.light;break;
        case PARAMS_LIGHT_IMAGE:++tmp.light_image;break;
        case PARAMS_EXTENDED_MESH:++tmp.extended_mesh;break;
        case PARAMS_RENDER_MATERIAL:
            // A one-byte payload is valid only for count==0 (clear).
            // Nonempty envelopes remain opaque here; KAMI validates their
            // complete face/UUID entries before using material references.
            if(length==1U&&data[0]!=0U)return 0;
            ++tmp.render_material;break;
        case PARAMS_REFLECTION_PROBE:++tmp.reflection_probe;break;
        case PARAMS_SCULPT:
        case PARAMS_MESH:{
            ++tmp.sculpt;
            // Linden Lab: LLSculptParams packs UUID[16] then sculpt
            // type[1]; lower 6 bits == 5 means a mesh, not 0x70.
            if((data[16]&0x3fU)==5U &&
               std::any_of(data,data+16,[](std::uint8_t x){return x!=0U;})){
                ++tmp.mesh_sculpt;
                std::memcpy(tmp.mesh_asset_uuid,data,16U);
            }
            break;
        }
        default: ++tmp.unknown;break;
        }
        at+=length;
    }
    if(at!=bytes_length)return 0;
    *output=tmp;
    return 1;
}

