// Bounded ImprovedTerseObjectUpdate High 15 pose decoder.
// See Linden Lab secondlife/viewer:
// scripts/messages/message_template.msg (High 15) and
// indra/newview/llviewerobject.cpp OUT_TERSE_IMPROVED.
// Original KAMI decoder, not a compiled copy of the official viewer.
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/world_wire.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace kami::protocol::objectspatial {
namespace {
using Byte=std::uint8_t;
std::uint16_t le16(std::span<const Byte> bytes,std::size_t n) noexcept {
    return std::uint16_t(bytes[n])|(std::uint16_t(bytes[n+1U])<<8);
}
std::uint32_t le32(std::span<const Byte> bytes,std::size_t n) noexcept {
    return std::uint32_t(bytes[n])|
       (std::uint32_t(bytes[n+1U])<<8)|
       (std::uint32_t(bytes[n+2U])<<16)|
       (std::uint32_t(bytes[n+3U])<<24);
}
float f32(std::span<const Byte> bytes,std::size_t n) noexcept {
    const auto raw=le32(bytes,n);
    float f=0.f;
    std::memcpy(&f,&raw,sizeof(f));
    return f;
}
float unit(std::uint16_t value) noexcept {
    return (float(value)/65535.f)*2.f-1.f;
}
bool sane(const TersePose& p) noexcept {
    const float len=p.rx*p.rx+p.ry*p.ry+p.rz*p.rz+p.rw*p.rw;
    return p.local_id!=0U&&
        std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        p.x>-4096.f&&p.x<8192.f&&p.y>-4096.f&&p.y<8192.f&&
        p.z>-4096.f&&p.z<8192.f&&
        std::isfinite(len)&&len>.90f&&len<1.10f;
}
bool pose_from_data(std::span<const Byte> bytes,TersePose& pose) noexcept {
    // Known server formats: U32 LocalID + U8 State + U8 AvatarFlag
    // + optional 16-byte avatar collision plane + LLVector3 Position
    // + U16[3] Velocity + U16[3] Acceleration + U16[4] Quaternion
    // + U16[3] angular velocity. Ignore unrecognized formats.
    if(bytes.size()!=44U&&bytes.size()!=60U)return false;
    const bool avatar_flag=bytes[5]!=0U;
    if(avatar_flag!=(bytes.size()==60U))return false;
    const std::size_t offset=avatar_flag?22U:6U;
    TersePose candidate{};
    candidate.local_id=le32(bytes,0U);
    candidate.x=f32(bytes,offset);
    candidate.y=f32(bytes,offset+4U);
    candidate.z=f32(bytes,offset+8U);
    const std::size_t q=offset+12U+6U+6U;
    candidate.rx=unit(le16(bytes,q));
    candidate.ry=unit(le16(bytes,q+2U));
    candidate.rz=unit(le16(bytes,q+4U));
    candidate.rw=unit(le16(bytes,q+6U));
    if(!sane(candidate))return false;
    const float norm=std::sqrt(candidate.rx*candidate.rx+
        candidate.ry*candidate.ry+candidate.rz*candidate.rz+
        candidate.rw*candidate.rw);
    candidate.rx/=norm;
    candidate.ry/=norm;
    candidate.rz/=norm;
    candidate.rw/=norm;
    pose=candidate;
    return true;
}
} // namespace
bool decode_terse_poses(const lludp::Packet& packet,
                        std::vector<TersePose>& out) noexcept {
    const auto info=worldwire::inspect(packet);
    if(info.kind!=worldwire::Kind::ObjectTerse||!info.complete_frame||
       info.blocks==0U||info.blocks>64U)return false;
    const std::span<const Byte> bytes(packet.body);
    std::size_t cursor=12U;
    std::vector<TersePose> result;
    try{
        result.reserve(info.blocks);
        for(unsigned i=0U;i<info.blocks;++i) {
            if(cursor>=bytes.size())return false;
            const auto len=std::size_t(bytes[cursor++]);
            if(len>bytes.size()-cursor)return false;
            TersePose pose{};
            if(pose_from_data(bytes.subspan(cursor,len),pose))
                result.push_back(pose);
            cursor+=len;
            if(cursor>bytes.size()||bytes.size()-cursor<2U)
                return false;
            const auto te_length=le16(bytes,cursor);
            cursor+=2U;
            if(te_length>bytes.size()-cursor)return false;
            cursor+=te_length; // A texture entry is not a pose.
        }
        if(cursor!=bytes.size())return false;
        out=std::move(result);
        return true;
    }catch(...){return false;}
}
bool apply_terse_pose(Transform& existing,const TersePose& update)
    noexcept {
    if(existing.local_id==0U||existing.local_id!=update.local_id||
       !sane(update))return false;
    const auto changed=[](float old,float fresh,float eps) noexcept {
        return !std::isfinite(old)||std::abs(old-fresh)>eps;
    };
    const bool dirty=changed(existing.x,update.x,.01f)||
        changed(existing.y,update.y,.01f)||
        changed(existing.z,update.z,.01f)||
        changed(existing.rx,update.rx,.0005f)||
        changed(existing.ry,update.ry,.0005f)||
        changed(existing.rz,update.rz,.0005f)||
        changed(existing.rw,update.rw,.0005f);
    if(!dirty)return false;
    existing.x=update.x;
    existing.y=update.y;
    existing.z=update.z;
    existing.rx=update.rx;
    existing.ry=update.ry;
    existing.rz=update.rz;
    existing.rw=update.rw;
    // Parent identity, mesh id, volume parameters, scale, and material
    // metadata are preserved from authenticated full updates.
    return true;
}
} // namespace kami::protocol::objectspatial
