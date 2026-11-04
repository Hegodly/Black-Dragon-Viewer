#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/world_wire.hpp"
#include "kami/protocol/mesh_reference.hpp"
#include <array>
#include <algorithm>
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
bool sane_child(const Transform& p) noexcept {
    return p.local_id!=0U&&p.parent_id!=0U&&
        p.parent_id!=p.local_id&&
        std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::abs(p.x)<=128.f&&std::abs(p.y)<=128.f&&
        std::abs(p.z)<=128.f&&
        std::isfinite(p.sx)&&std::isfinite(p.sy)&&std::isfinite(p.sz)&&
        p.sx>=.01f&&p.sy>=.01f&&p.sz>=.01f&&
        p.sx<=64.f&&p.sy<=64.f&&p.sz<=64.f;
}

// All offset operations remain within the size-delimited compressed block.
// The official compressed object template puts ExtraParams after optional
// Omega, ParentID, Tree/Scratch, HUD text, MediaURL and particle fields.
// This conservative reader declines unknown variable-sized particle fields
// rather than scanning for a UUID pattern (which could invent a mesh).
std::string compressed_mesh_id(std::span<const std::uint8_t> data,
                               std::size_t at,std::uint32_t special) noexcept {
    const auto available=[&](std::size_t n){
        return at<=data.size()&&n<=data.size()-at;
    };
    if(special&0x02U){
        if(!available(1U))return {};
        ++at; // TreeData U8
    }else if(special&0x01U){
        if(!available(4U))return {};
        const std::size_t length=u32(data,at);
        at+=4U;
        if(length>4096U||!available(length))return {};
        at+=length;
    }
    const auto cstring=[&]() -> bool {
        std::size_t n=0;
        while(available(1U)&&n++<2048U){
            if(data[at++]==0U)return true;
        }
        return false;
    };
    if(special&0x04U){
        if(!cstring()||!available(4U))return {};
        at+=4U; // text color
    }
    if((special&0x200U)&&!cstring())return {};
    if(special&0x08U)return {}; // particle payload not decoded yet
    if(!available(1U))return {};
    const std::size_t first=at;
    const unsigned count=data[at++];
    if(count>32U)return {};
    for(unsigned n=0;n<count;++n){
        if(!available(6U))return {};
        const std::size_t length=u32(data,at+2U);
        at+=6U;
        if(length>4096U||!available(length))return {};
        at+=length;
    }
    return mesh::mesh_asset_from_extras(data.subspan(first,at-first));
}
} // namespace

bool decode_compressed_transforms(const lludp::Packet& packet,
                                  std::vector<Transform>& out,
                                  std::vector<Transform>* child_transforms) noexcept {
    // High 13: ObjectUpdateCompressed. All blocks must have bounded
    // framing before any untrusted fields are read.
    if(worldwire::inspect(packet).kind!=worldwire::Kind::ObjectCompressed ||
       !worldwire::inspect(packet).complete_frame)return false;
    const std::span<const std::uint8_t> b(packet.body);
    const unsigned count=b[11];
    std::size_t at=12;
    std::vector<Transform> decoded;
    std::vector<Transform> linked;
    decoded.reserve(count);
    linked.reserve(count);
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
        // Retain parent-relative children separately; never pass their
        // local coordinates to the region-space root renderer.
        Transform t{
            u32(data,16U),f32(data,40U),f32(data,44U),
            f32(data,48U),f32(data,28U),f32(data,32U),f32(data,36U)
        };
        t.parent_id=parent;
        t.mesh_asset_id=compressed_mesh_id(data,cursor,special);
        // The compressed path does not classify unknown path/profile
        // topology; a mesh ID alone is not a renderable triangle list.
        // Compressed ObjectUpdate stores 3 floats for LLQuaternion.
        t.rx=f32(data,52U);
        t.ry=f32(data,56U);
        t.rz=f32(data,60U);
        const float q=t.rx*t.rx+t.ry*t.ry+t.rz*t.rz;
        if(!std::isfinite(q)||q>1.0001f)continue;
        t.rw=std::sqrt(std::max(0.f,1.f-q));
        if(parent==0U){
            if(sane(t))decoded.push_back(t);
        }else if(sane_child(t)){
            linked.push_back(t);
        }
    }
    if(at!=b.size())return false;
    out=std::move(decoded);
    if(child_transforms)*child_transforms=std::move(linked);
    return true;
}
bool decode_cached_ids(const lludp::Packet& packet,
                       std::vector<std::uint32_t>& out) noexcept {
    const auto info=worldwire::inspect(packet);
    if(info.kind!=worldwire::Kind::ObjectCached||!info.complete_frame)
        return false;
    const std::span<const std::uint8_t> b(packet.body);
    std::vector<std::uint32_t> ids;
    ids.reserve(info.blocks);
    for(unsigned i=0;i<info.blocks;++i){
        // ID U32, CRC U32 and UpdateFlags U32 follow the RegionData.
        const auto id=u32(b,12U+i*12U);
        if(id!=0U && std::find(ids.begin(),ids.end(),id)==ids.end())
            ids.push_back(id);
    }
    out=std::move(ids);
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
