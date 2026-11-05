#include "kami/protocol/texture_entry.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace kami::protocol::texture {
namespace {
std::string uuid_string(const std::array<std::uint8_t,16U>& id) {
    if(std::all_of(id.begin(),id.end(),
                   [](std::uint8_t byte){return byte==0U;}))return {};
    constexpr char hex[]="0123456789abcdef";
    std::string result;
    result.reserve(36U);
    for(std::size_t i=0;i<id.size();++i){
        if(i==4U||i==6U||i==8U||i==10U)result.push_back('-');
        result.push_back(hex[id[i]>>4U]);
        result.push_back(hex[id[i]&0x0fU]);
    }
    return result;
}
// Official packTEField exception masks use a big-endian stream of
// 7-bit groups. The mask bit number is the SL face index (0..44).
bool face_mask(std::span<const std::uint8_t> bytes,
               std::size_t& at,std::uint64_t& mask) noexcept {
    mask=0;
    for(unsigned octet=0U;octet<10U&&at<bytes.size();++octet){
        const auto b=bytes[at++];
        if(mask>(std::numeric_limits<std::uint64_t>::max()>>7U))
            return false;
        mask=(mask<<7U)|(b&0x7fU);
        if((b&0x80U)==0U)
            return (mask>>45U)==0U;
    }
    return false;
}
template<class Set>
bool exceptions(std::span<const std::uint8_t> bytes,std::size_t& at,
                std::size_t width,Set set) noexcept {
    for(unsigned n=0U;n<48U;++n){
        std::uint64_t mask=0U;
        if(!face_mask(bytes,at,mask))return false;
        if(mask==0U)return true; // next field starts immediately after 0
        if(width>bytes.size()-at)return false;
        const auto value=bytes.subspan(at,width);
        at+=width;
        for(unsigned f=0U;f<45U;++f)
            if(mask&(std::uint64_t{1}<<f))set(f,value);
    }
    return false;
}
std::uint16_t le16(std::span<const std::uint8_t> b) noexcept {
    return std::uint16_t(b[0])|(std::uint16_t(b[1])<<8U);
}
float f32(std::span<const std::uint8_t> b) noexcept {
    const std::uint32_t bits=std::uint32_t(b[0])|
        (std::uint32_t(b[1])<<8U)|(std::uint32_t(b[2])<<16U)|
        (std::uint32_t(b[3])<<24U);
    float value=0.f;
    std::memcpy(&value,&bits,sizeof(value));
    return value;
}
float signed_fraction(std::span<const std::uint8_t> b,float divisor) noexcept {
    const int n=static_cast<int>(le16(b));
    return static_cast<float>(n>=32768?n-65536:n)/divisor;
}
template<class Set>
bool typed_field(std::span<const std::uint8_t> bytes,std::size_t& at,
                 std::size_t width,Set set) noexcept {
    if(width>bytes.size()-at)return false;
    const auto initial=bytes.subspan(at,width);
    at+=width;
    for(unsigned f=0U;f<45U;++f)set(f,initial);
    return exceptions(bytes,at,width,set);
}
bool valid_uv(const UVTransform& uv) noexcept {
    return std::isfinite(uv.repeat_u)&&std::isfinite(uv.repeat_v)&&
        std::isfinite(uv.offset_u)&&std::isfinite(uv.offset_v)&&
        std::isfinite(uv.rotation)&&
        std::abs(uv.repeat_u)<=100.f&&std::abs(uv.repeat_v)<=100.f&&
        std::abs(uv.offset_u)<=1.001f&&std::abs(uv.offset_v)<=1.001f&&
        std::abs(uv.rotation)<=6.284f;
}
} // namespace
std::string default_image_uuid(const Base& entry){
    return uuid_string(entry.image_id);
}
std::string face_image_uuid(const Base& entry,unsigned face){
    if(face>=entry.face_image_override.size())return {};
    return entry.face_image_override[face]?
        uuid_string(entry.face_image_ids[face]):
        uuid_string(entry.image_id);
}
bool decode_base(std::span<const std::uint8_t> bytes,Base& out) noexcept {
    if(bytes.size()<21U||bytes.size()>4096U)return false;
    Base parsed{};
    std::copy_n(bytes.begin(),16U,parsed.image_id.begin());
    std::size_t at=16U;
    if(!exceptions(bytes,at,16U,
        [&](unsigned f,std::span<const std::uint8_t> id){
            parsed.face_image_override[f]=true;
            std::copy_n(id.begin(),16U,parsed.face_image_ids[f].begin());
        }))return false;
    if(bytes.size()-at<4U)return false;
    // Packed TE colors are inverted: 0,0,0,0 means opaque white.
    const auto color=[&](unsigned face,std::span<const std::uint8_t> rgba){
        for(unsigned c=0U;c<4U;++c)
            parsed.rgba[face][c]=float(255U-rgba[c])/255.f;
    };
    const auto common=bytes.subspan(at,4U);
    at+=4U;
    for(unsigned f=0U;f<45U;++f)color(f,common);
    parsed.red=parsed.rgba[0][0];
    parsed.green=parsed.rgba[0][1];
    parsed.blue=parsed.rgba[0][2];
    parsed.alpha=parsed.rgba[0][3];
    // Existing short TE fixtures encode only image and color defaults.
    // A full simulator TE contains the sentinel, per-face colors, then
    // S/T repeats F32, S/T offsets S16 and rotation S16.
    if(at==bytes.size()){out=parsed;return true;}
    if(!exceptions(bytes,at,4U,color))return false;
    if(at==bytes.size()){out=parsed;return true;}
    if(!typed_field(bytes,at,4U,
         [&](unsigned f,std::span<const std::uint8_t> value){
             parsed.uv[f].repeat_u=f32(value);
         }))return false;
    if(!typed_field(bytes,at,4U,
         [&](unsigned f,std::span<const std::uint8_t> value){
             parsed.uv[f].repeat_v=f32(value);
         }))return false;
    if(!typed_field(bytes,at,2U,
         [&](unsigned f,std::span<const std::uint8_t> value){
             parsed.uv[f].offset_u=signed_fraction(value,32767.f);
         }))return false;
    if(!typed_field(bytes,at,2U,
         [&](unsigned f,std::span<const std::uint8_t> value){
             parsed.uv[f].offset_v=signed_fraction(value,32767.f);
         }))return false;
    constexpr float full_turn=6.28318530717958647692f;
    if(!typed_field(bytes,at,2U,
         [&](unsigned f,std::span<const std::uint8_t> value){
             parsed.uv[f].rotation=
                signed_fraction(value,32768.f)*full_turn;
         }))return false;
    for(const auto& uv:parsed.uv)
        if(!valid_uv(uv))return false;
    // Remaining bump, texgen, glow, PBR IDs are deliberately not
    // interpreted as diffuse UV transforms.
    out=std::move(parsed);
    return true;
}
} // namespace kami::protocol::texture
