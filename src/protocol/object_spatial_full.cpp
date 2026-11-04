#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/world_wire.hpp"
#include <cmath>
#include <cstring>
#include <span>
#include <utility>

namespace kami::protocol::objectspatial {
namespace {
class Cursor final {
public:
    explicit Cursor(std::span<const std::uint8_t> bytes) noexcept:bytes_(bytes){}
    [[nodiscard]] bool take(std::size_t n,std::span<const std::uint8_t>& out) noexcept {
        if(at_>bytes_.size()||n>bytes_.size()-at_)return false;
        out=bytes_.subspan(at_,n);at_+=n;return true;
    }
    [[nodiscard]] bool skip(std::size_t n) noexcept {
        std::span<const std::uint8_t> unused;
        return take(n,unused);
    }
    [[nodiscard]] bool u8(std::uint8_t& v) noexcept {
        std::span<const std::uint8_t> b;
        if(!take(1U,b))return false;
        v=b[0];return true;
    }
    [[nodiscard]] bool u32(std::uint32_t& v) noexcept {
        std::span<const std::uint8_t> b;
        if(!take(4U,b))return false;
        v=static_cast<std::uint32_t>(b[0])|
          (static_cast<std::uint32_t>(b[1])<<8)|
          (static_cast<std::uint32_t>(b[2])<<16)|
          (static_cast<std::uint32_t>(b[3])<<24);
        return true;
    }
    [[nodiscard]] bool f32(float& v) noexcept {
        std::uint32_t bits=0;
        if(!u32(bits))return false;
        static_assert(sizeof(bits)==sizeof(v));
        std::memcpy(&v,&bits,sizeof(v));return true;
    }
    [[nodiscard]] bool variable(unsigned n,std::span<const std::uint8_t>& data) noexcept {
        if(n!=1U&&n!=2U)return false;
        std::span<const std::uint8_t> len;
        if(!take(n,len))return false;
        const std::size_t size=n==1U?
            static_cast<std::size_t>(len[0]):
            static_cast<std::size_t>(len[0])|
            (static_cast<std::size_t>(len[1])<<8);
        return take(size,data);
    }
    [[nodiscard]] bool skip_variable(unsigned n) noexcept {
        std::span<const std::uint8_t> ignored;
        return variable(n,ignored);
    }
    [[nodiscard]] bool done() const noexcept {return at_==bytes_.size();}
private:
    std::span<const std::uint8_t> bytes_;
    std::size_t at_=0U;
};
bool valid(const Transform& p) noexcept {
    return p.local_id!=0U&&
        std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::isfinite(p.sx)&&std::isfinite(p.sy)&&std::isfinite(p.sz)&&
        p.x>=0.f&&p.x<256.f&&p.y>=0.f&&p.y<256.f&&
        p.z>=-4096.f&&p.z<8192.f&&
        p.sx>=.01f&&p.sy>=.01f&&p.sz>=.01f&&
        p.sx<=64.f&&p.sy<=64.f&&p.sz<=64.f;
}
}
// High 12 ObjectUpdate, per scripts/messages/message_template.msg and
// LLViewerObject::processUpdateMessage(OUT_FULL). Unlike High 13,
// every block has dozens of fixed and length-prefixed fields. Consume
// every field so the next block begins at the *correct* boundary.
bool decode_full_transforms(const lludp::Packet& packet,
                            std::vector<Transform>& out) noexcept {
    const auto info=worldwire::inspect(packet);
    if(info.kind!=worldwire::Kind::ObjectFull||info.blocks==0U||
       info.blocks>64U)return false;
    const std::span<const std::uint8_t> b(packet.body);
    if(b.size()<12U)return false;
    Cursor cur(b.subspan(12U));
    std::vector<Transform> decoded;
    decoded.reserve(info.blocks);
    for(unsigned n=0U;n<info.blocks;++n){
        Transform t{};
        std::uint8_t pcode=0U;
        std::uint32_t parent=0U;
        std::span<const std::uint8_t> movement{};
        // ID U32, State U8, FullID UUID, CRC U32, PCode U8,
        // Material U8, ClickAction U8, Scale LLVector3.
        if(!cur.u32(t.local_id)||!cur.skip(1U+16U+4U)||
           !cur.u8(pcode)||!cur.skip(2U)||
           !cur.f32(t.sx)||!cur.f32(t.sy)||!cur.f32(t.sz)||
           !cur.variable(1U,movement)||
           !cur.u32(parent)||!cur.skip(4U))return false;
        // Parametric primitive shape fields: 23 fixed bytes,
        // i.e. PathCurve through ProfileHollow inclusive.
        if(!cur.skip(23U))return false;
        // TextureEntry Var2; TextureAnim Var1; NameValue Var2;
        // Data Var2; Text Var1; TextColor Fixed4;
        // MediaURL Var1; PSBlock Var1; ExtraParams Var1.
        if(!cur.skip_variable(2U)||!cur.skip_variable(1U)||
           !cur.skip_variable(2U)||!cur.skip_variable(2U)||
           !cur.skip_variable(1U)||!cur.skip(4U)||
           !cur.skip_variable(1U)||!cur.skip_variable(1U)||
           !cur.skip_variable(1U))return false;
        // Sound UUID, OwnerID UUID, Gain F32, Flags U8,
        // Radius F32, JointType U8, JointPivot LLVector3,
        // JointAxisOrAnchor LLVector3.
        if(!cur.skip(16U+16U+4U+1U+4U+1U+12U+12U))
            return false;
        if(pcode!=9U||parent!=0U)continue;
        // Per the official viewer, full-precision ObjectData has
        // 60-byte position/velocity/acceleration/rotation/omega,
        // optionally prefixed by 16-byte avatar collision plane.
        // The 124/140 variants include extended data; the same
        // position offset remains valid.
        std::size_t pos_offset=0U;
        if(movement.size()==76U||movement.size()==140U)
            pos_offset=16U;
        else if(movement.size()!=60U&&movement.size()!=124U)
            continue;
        const auto read_float=[&](std::size_t offset,float& value) {
            if(offset+4U>movement.size())return false;
            std::uint32_t bits=static_cast<std::uint32_t>(movement[offset])|
                (static_cast<std::uint32_t>(movement[offset+1U])<<8)|
                (static_cast<std::uint32_t>(movement[offset+2U])<<16)|
                (static_cast<std::uint32_t>(movement[offset+3U])<<24);
            std::memcpy(&value,&bits,sizeof(value));
            return true;
        };
        if(!read_float(pos_offset,t.x)||
           !read_float(pos_offset+4U,t.y)||
           !read_float(pos_offset+8U,t.z))return false;
        if(valid(t))decoded.push_back(t);
    }
    if(!cur.done())return false;
    out=std::move(decoded);
    return true;
}
} // namespace kami::protocol::objectspatial
