#include "kami/protocol/texture_entry.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
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
} // namespace
std::string default_image_uuid(const Base& entry) {
    return uuid_string(entry.image_id);
}
std::string face_image_uuid(const Base& entry,unsigned face) {
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
    // Each exception is encoded as a 7-bit-continuation mask followed by
    // a UUID. The mask=0 terminator ends the image-id field.
    bool ended=false;
    for(unsigned exception=0U;exception<48U&&at<bytes.size();++exception){
        std::uint64_t mask=0U;
        bool complete=false;
        for(unsigned octet=0U;octet<10U&&at<bytes.size();++octet){
            const auto b=bytes[at++];
            if(mask>(std::numeric_limits<std::uint64_t>::max()>>7U))
                return false;
            mask=(mask<<7U)|(b&0x7fU);
            if((b&0x80U)==0U){complete=true;break;}
        }
        if(!complete)return false;
        if(mask==0U){ended=true;break;}
        // Linden Lab LLPrimitive supports at most 45 per-face entries.
        if((mask>>45U)!=0U||bytes.size()-at<16U)return false;
        for(unsigned face=0U;face<45U;++face){
            if((mask&(std::uint64_t{1}<<face))==0U)continue;
            parsed.face_image_override[face]=true;
            std::copy_n(bytes.data()+at,16U,
                        parsed.face_image_ids[face].begin());
        }
        at+=16U;
    }
    if(!ended||bytes.size()-at<4U)return false;
    // The source viewer packs (255 - RGBA), making white opaque all zero.
    parsed.red=static_cast<float>(255U-bytes[at])/255.f;
    parsed.green=static_cast<float>(255U-bytes[at+1U])/255.f;
    parsed.blue=static_cast<float>(255U-bytes[at+2U])/255.f;
    parsed.alpha=static_cast<float>(255U-bytes[at+3U])/255.f;
    out=parsed;
    return true;
}
} // namespace kami::protocol::texture
