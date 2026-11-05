#include "kami/protocol/official_texture_entry.h"
#include "kami/protocol/texture_entry.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <cstring>

namespace {
using Bytes=std::vector<std::uint8_t>;
void add(Bytes& b,std::initializer_list<std::uint8_t> bytes){
    b.insert(b.end(),bytes.begin(),bytes.end());
}
Bytes official_wire_fixture(){
    Bytes b(16U,0x31U);            // default diffuse image UUID
    add(b,{0x81U,0x00U});         // face #7 default-image override
    b.insert(b.end(),16U,0xa0U);
    add(b,{0U});                  // image field end
    add(b,{0U,0U,0U,0U});         // default white opaque
    add(b,{0x04U,0U,128U,255U,0U}); // face 2 packed color override
    add(b,{0U});                  // color field end
    add(b,{0U,0U,0U,0x40U});     // repeat S default 2
    add(b,{0x04U,0U,0U,0x80U,0x3fU}); // face 2 repeat S 1
    add(b,{0U});
    add(b,{0U,0U,0U,0x3fU});    // repeat T default .5
    add(b,{0U});
    add(b,{0U,0x20U,0U});        // offset S 8192 / 32767
    add(b,{0U,0xc0U,0U});        // offset T -16384 / 32767
    add(b,{0U,0x20U,0U});        // rotation 8192 /32768 turns
    add(b,{0x00U,0U});           // bump/shiny/fullbright 0
    add(b,{0x00U,0U});           // media/texgen 0
    add(b,{128U,0U});            // glow default .5, sentinel
    b.insert(b.end(),16U,0U);    // default legacy material ID
    add(b,{0x04U});              // face #2 material UUID
    b.insert(b.end(),16U,0x66U);
    add(b,{0U});
    return b;
}
int run(){
    const auto bytes=official_wire_fixture();
    KamiSlTextureEntryV1 decoded{};
    decoded.struct_size=sizeof(decoded);
    if(!kami_sl_decode_texture_entry_v1(
          bytes.data(),static_cast<std::uint32_t>(bytes.size()),
          45U,&decoded))return 1;
    if(decoded.face_count!=45U||
       decoded.default_image_id[0]!=0x31U||
       decoded.faces[7U].image_override!=1U||
       decoded.faces[7U].image_id[0]!=0xa0U||
       decoded.faces[6U].image_override!=0U||
       decoded.faces[6U].image_id[0]!=0x31U)return 2;
    if(decoded.faces[2U].inverted_rgba[1]!=128U||
       decoded.faces[2U].inverted_rgba[2]!=255U||
       decoded.faces[0U].inverted_rgba[1]!=0U||
       std::abs(decoded.faces[2U].scale_s-1.f)>1.e-6f||
       std::abs(decoded.faces[3U].scale_s-2.f)>1.e-6f||
       std::abs(decoded.faces[3U].scale_t-.5f)>1.e-6f||
       decoded.faces[0U].offset_s!=8192||
       decoded.faces[0U].offset_t!=-16384||
       decoded.faces[0U].packed_rotation!=8192||
       decoded.faces[0U].glow!=128U||
       decoded.faces[2U].material_id[0]!=0x66U)return 3;
    kami::protocol::texture::Base material{};
    if(!kami::protocol::texture::decode_base(bytes,material))return 4;
    if(!material.face_image_override[7U]||
       material.face_image_override[8U]||
       material.face_image_ids[7U][0]!=0xa0U||
       material.rgba[2U][2]!=0.f||
       std::abs(material.uv[2U].repeat_u-1.f)>1.e-6f||
       std::abs(material.uv[0U].repeat_v-.5f)>1.e-6f||
       std::abs(material.uv[0U].rotation-1.57079632679f)>2.e-4f)
        return 5;
    // Official LLPrimitive appends a zero byte when a final TE field
    // omitted its sentinel. This form must still decode correctly.
    auto last_unterminated=bytes;
    last_unterminated.pop_back();
    KamiSlTextureEntryV1 no_sentinel{};
    no_sentinel.struct_size=sizeof(no_sentinel);
    if(!kami_sl_decode_texture_entry_v1(
        last_unterminated.data(),
        static_cast<std::uint32_t>(last_unterminated.size()),
        45U,&no_sentinel))return 6;
    // Truncating an intermediate mandatory field, unlike the last mask,
    // must never retain half-parsed material state.
    auto truncated=bytes;
    truncated.resize(36U);
    const auto previous=decoded;
    if(kami_sl_decode_texture_entry_v1(
        truncated.data(),static_cast<std::uint32_t>(truncated.size()),
        45U,&decoded)||std::memcmp(&decoded,&previous,sizeof(decoded))!=0)
        return 7;
    KamiSlTextureEntryV1 wrong{};
    wrong.struct_size=1U;
    if(kami_sl_decode_texture_entry_v1(
        bytes.data(),static_cast<std::uint32_t>(bytes.size()),
        45U,&wrong))return 8;
    // Fuzz a deterministic bounded set of hostile snapshots.
    std::uint32_t seed=0xd3c01205U;
    for(unsigned n=0U;n<1000U;++n){
        seed=1664525U*seed+1013904223U;
        auto hostile=bytes;
        const auto pos=static_cast<std::size_t>(seed)%hostile.size();
        hostile[pos]^=static_cast<std::uint8_t>((seed>>24U)|1U);
        KamiSlTextureEntryV1 local{};
        local.struct_size=sizeof(local);
        (void)kami_sl_decode_texture_entry_v1(
            hostile.data(),static_cast<std::uint32_t>(hostile.size()),
            45U,&local);
    }
    return 0;
}
}
int main(){
    const int result=run();
    if(result!=0)
        std::fprintf(stderr,"Official LLPrimitive TextureEntry compatibility test %d failed\n",result);
    return result;
}
