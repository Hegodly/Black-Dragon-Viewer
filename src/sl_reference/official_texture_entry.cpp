/*
 * Linden Lab TextureEntry wire format compatibility adapter.
 * Derived from LLPrimitive::unpack_TEField/parseTEMessage in the
 * official Second Life viewer, indra/llprimitive/llprimitive.cpp.
 * Original Copyright (C) 2010 Linden Research, Inc.
 * Copyright (C) 2026 KAMI contributors for C ABI and bounds checking.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 * Upstream reference: secondlife/viewer develop, pinned commit
 * bb143726c780303e8302bfd7699d3d2d95935c39
 * https://github.com/secondlife/viewer/blob/bb143726c780303e8302bfd7699d3d2d95935c39/indra/llprimitive/llprimitive.cpp
 *
 * Distributed as a REPLACEABLE DLL. See third_party/secondlife-lgpl/NOTICE.md
 * and third_party/secondlife-lgpl/LICENSE.LGPL-2.1.txt.
 */
#include "kami/protocol/official_texture_entry.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {
struct Parse {
    const std::uint8_t* input;
    std::size_t length;
    std::size_t pos=0U;
    KamiSlTextureEntryV1& result;
    unsigned face_count;

    // LLPrimitive::unpack_TEField uses a variable-length, big-endian
    // sequence of 7-bit groups. The low bit refers to face index 0.
    bool read_mask(std::uint64_t& mask) noexcept {
        mask=0U;
        for(unsigned i=0;i<10U;++i){
            if(pos>=length)return false;
            const auto byte=input[pos++];
            if(mask>(std::numeric_limits<std::uint64_t>::max()>>7U))
                return false;
            mask=(mask<<7U)|(byte&0x7fU);
            if(!(byte&0x80U))
                return (mask>>KAMI_SL_TE_MAX_FACES)==0U;
        }
        return false;
    }
    template<class Store>
    bool field(std::size_t width,Store store,
               bool allow_implicit_final_terminator=false) noexcept {
        if(width>length-pos)return false;
        const auto* default_value=input+pos;
        pos+=width;
        for(unsigned f=0U;f<face_count;++f)
            store(f,default_value,false);
        for(unsigned fields=0U;fields<48U;++fields){
            if(pos==length)return allow_implicit_final_terminator;
            std::uint64_t mask=0U;
            if(!read_mask(mask))return false;
            if(mask==0U)return true;
            // Never treat a missing value as a new field's default.
            if(width>length-pos)return false;
            const auto* override_value=input+pos;
            pos+=width;
            for(unsigned f=0U;f<face_count;++f)
                if((mask&(std::uint64_t{1}<<f))!=0U)
                    store(f,override_value,true);
        }
        return false;
    }
};
std::int16_t read_s16(const std::uint8_t* b) noexcept {
    const auto raw=std::uint16_t(b[0])|
        (std::uint16_t(b[1])<<8U);
    return raw<0x8000U?static_cast<std::int16_t>(raw):
        static_cast<std::int16_t>(static_cast<std::int32_t>(raw)-65536);
}
float read_f32(const std::uint8_t* b) noexcept {
    const std::uint32_t bits=std::uint32_t(b[0])|
        (std::uint32_t(b[1])<<8U)|
        (std::uint32_t(b[2])<<16U)|
        (std::uint32_t(b[3])<<24U);
    float f=0.f;std::memcpy(&f,&bits,sizeof(f));return f;
}
}
extern "C" KAMI_SL_TE_API std::int32_t KAMI_SL_TE_CALL
kami_sl_decode_texture_entry_v1(const std::uint8_t* data,
    std::uint32_t length,std::uint32_t face_count,
    KamiSlTextureEntryV1* output){
    if(!output||output->struct_size!=sizeof(*output)||
       !data||length==0U||length>KAMI_SL_TE_MAX_BYTES||
       face_count==0U||face_count>KAMI_SL_TE_MAX_FACES)return 0;
    // All parsing is transactional; malformed server packets never
    // replace the last authenticated material table.
    KamiSlTextureEntryV1 parsed{};
    parsed.struct_size=sizeof(parsed);
    parsed.face_count=face_count;
    Parse p{data,length,0U,parsed,face_count};
    const auto image=[&](unsigned f,const std::uint8_t* value,bool exception){
        auto& face=parsed.faces[f];
        std::memcpy(face.image_id,value,16U);
        if(!exception&&f==0U)
            std::memcpy(parsed.default_image_id,value,16U);
        if(exception)face.image_override=1U;
    };
    if(!p.field(16U,image))return 0;
    const auto color=[&](unsigned f,const std::uint8_t* value,bool exception){
        (void)exception;
        std::memcpy(parsed.faces[f].inverted_rgba,value,4U);
        if(f==0U&&!exception)
            std::memcpy(parsed.default_inverted_rgba,value,4U);
    };
    if(!p.field(4U,color))return 0;
    if(!p.field(4U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].scale_s=read_f32(value);
    }))return 0;
    if(!p.field(4U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].scale_t=read_f32(value);
    }))return 0;
    if(!p.field(2U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].offset_s=read_s16(value);
    }))return 0;
    if(!p.field(2U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].offset_t=read_s16(value);
    }))return 0;
    if(!p.field(2U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].packed_rotation=read_s16(value);
    }))return 0;
    if(!p.field(1U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].bump_shiny_fullbright=value[0];
    }))return 0;
    if(!p.field(1U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].media_texgen=value[0];
    }))return 0;
    // Official LLPrimitive::parseTEMessage appends a 0 terminator to
    // accommodate a final TE field with no terminating mask.
    if(!p.field(1U,[&](unsigned f,const std::uint8_t* value,bool){
        parsed.faces[f].glow=value[0];
    },true))return 0;
    // Modern viewer may transmit legacy material ID as an optional
    // final field; it is NOT a glTF PBR material asset identifier.
    if(p.pos<length){
        if(!p.field(16U,[&](unsigned f,const std::uint8_t* value,bool){
            std::memcpy(parsed.faces[f].material_id,value,16U);
        },true))return 0;
    }
    if(p.pos!=length)return 0;
    *output=parsed;
    return 1;
}
