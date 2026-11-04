#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/world_wire.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <span>
#include <utility>

namespace kami::protocol::objectspatial {
namespace {
std::uint32_t u32(std::span<const std::uint8_t> b,std::size_t i) noexcept {
    return static_cast<std::uint32_t>(b[i])|
        (static_cast<std::uint32_t>(b[i+1])<<8)|
        (static_cast<std::uint32_t>(b[i+2])<<16)|
        (static_cast<std::uint32_t>(b[i+3])<<24);
}
std::uint16_t u16(std::span<const std::uint8_t> b,std::size_t i) noexcept {
    return static_cast<std::uint16_t>(b[i])|
        (static_cast<std::uint16_t>(b[i+1])<<8);
}
float f32(std::span<const std::uint8_t> b,std::size_t i) noexcept {
    const std::uint32_t raw=u32(b,i);
    float v=0.f;
    std::memcpy(&v,&raw,sizeof(v));
    return v;
}
bool sane(const Transform& p) noexcept {
    // Reject non-finite, untrusted and absurd dimensions/coordinates.
    return p.local_id!=0U&&
        std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::isfinite(p.sx)&&std::isfinite(p.sy)&&std::isfinite(p.sz)&&
        p.x>=0.f&&p.x<256.f&&p.y>=0.f&&p.y<256.f&&
        p.z>=-4096.f&&p.z<8192.f&&
        p.sx>=.01f&&p.sy>=.01f&&p.sz>=.01f&&
        p.sx<=64.f&&p.sy<=64.f&&p.sz<=64.f;
}
} // namespace

bool decode_compressed_transforms(const lludp::Packet& packet,
                                  std::vector<Transform>& out) noexcept {
    // High 13: ObjectUpdateCompressed. All blocks must have bounded
    // framing before any untrusted fields are read.
    if(worldwire::inspect(packet).kind!=worldwire::Kind::ObjectCompressed ||
       !worldwire::inspect(packet).complete_frame)return false;
    const std::span<const std::uint8_t> b(packet.body);
    const unsigned count=b[11];
    std::size_t at=12;
    std::vector<Transform> decoded;
    decoded.reserve(count);
    for(unsigned i=0;i<count;++i){
        if(at>b.size()||b.size()-at<6U)return false;
        const std::size_t len=u16(b,at+4);
        at+=6;
        if(len<84U||len>b.size()-at)return false;
        const auto data=b.subspan(at,len);
        at+=len;
        // Compressed Data order per LLViewerObject::processUpdateMessage:
        // UUID[16], localID U32, PCode U8, State U8, CRC U32,
        // Material U8, ClickAction U8, Scale XYZ, Pos XYZ, Rot XYZ,
        // SpecialCode U32, Owner UUID[16], optional Omega / ParentID.
        // PCode=9: volume object. Other PCodes may be avatars/grass/trees.
        if(data[20]!=9U)continue;
        const auto special=u32(data,64U);
        std::size_t cursor=84U;
        if(special&0x80U){
            if(data.size()-cursor<12U)return false;
            cursor+=12U;
        }
        std::uint32_t parent=0;
        if(special&0x20U){
            if(data.size()-cursor<4U)return false;
            parent=u32(data,cursor);
            cursor+=4U;
        }
        // Parent-relative link children cannot be placed in region space
        // until their root transform has been decoded and composed.
        if(parent!=0U)continue;
        const Transform t{
            u32(data,16U),f32(data,40U),f32(data,44U),
            f32(data,48U),f32(data,28U),f32(data,32U),f32(data,36U)
        };
        if(sane(t))decoded.push_back(t);
    }
    if(at!=b.size())return false;
    out=std::move(decoded);
    return true;
}
bool decode_killed_ids(const lludp::Packet& packet,
                       std::vector<std::uint32_t>& out) noexcept {
    const auto info=worldwire::inspect(packet);
    if(info.kind!=worldwire::Kind::ObjectKilled||!info.complete_frame)
        return false;
    std::vector<std::uint32_t> tmp;
    const auto& b=packet.body;
    tmp.reserve(info.blocks);
    for(unsigned i=0;i<info.blocks;++i){
        std::uint32_t id=u32(b,2U+i*4U);
        if(id!=0U)tmp.push_back(id);
    }
    out=std::move(tmp);
    return true;
}
} // namespace kami::protocol::objectspatial
