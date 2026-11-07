#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/pbr_material.hpp"
#include "kami/protocol/world_wire.hpp"
#include "kami/protocol/mesh_reference.hpp"
#include "kami/protocol/texture_entry.hpp"
#include "kami/protocol/official_extra_params.h"
#include <algorithm>
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
    const bool is_child=p.parent_id!=0U;
    // Children have local-frame positions; roots have region coordinates.
    return p.local_id!=0U&&p.parent_id!=p.local_id&&
        std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::isfinite(p.sx)&&std::isfinite(p.sy)&&std::isfinite(p.sz)&&
        std::isfinite(p.rx)&&std::isfinite(p.ry)&&
        std::isfinite(p.rz)&&std::isfinite(p.rw)&&
        (is_child?(std::abs(p.x)<=128.f&&std::abs(p.y)<=128.f&&
                    std::abs(p.z)<=128.f):
                  (p.x>=0.f&&p.x<256.f&&p.y>=0.f&&p.y<256.f&&
                   p.z>=-4096.f&&p.z<8192.f))&&
        p.sx>=.01f&&p.sy>=.01f&&p.sz>=.01f&&
        p.sx<=64.f&&p.sy<=64.f&&p.sz<=64.f;
}
// Raw fields from official message_template.msg ObjectUpdate ObjectData.
// Only simple, uncut, unhollowed primitive path/profile combinations
// have a matching renderer; complex SL volumes must stay unsupported.
PrimitiveKind simple_shape(std::span<const std::uint8_t> s) noexcept {
    // Official 23-byte SL path/profile layout:
    // PathCurve[0], PathBegin[1..2], PathEnd[3..4],
    // ScaleX[5], ScaleY[6], other path settings[7..15],
    // ProfileCurve[16], ProfileBegin/End/Hollow[17..22].
    if(s.size()!=23U)return PrimitiveKind::Unknown;
    for(unsigned i=1U;i<=4U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    if(s[5]!=100U||s[6]!=100U)return PrimitiveKind::Unknown;
    for(unsigned i=7U;i<=15U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    for(unsigned i=17U;i<23U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    if((s[16]&0xF0U)!=0U)return PrimitiveKind::Unknown;
    const auto profile=s[16]&0x0FU;
    if(s[0]==0x10U&&profile==0x01U)return PrimitiveKind::Box;
    if(s[0]==0x10U&&profile==0x00U)return PrimitiveKind::Cylinder;
    if(s[0]==0x10U&&profile==0x03U)return PrimitiveKind::Prism;
    if(s[0]==0x20U&&profile==0x05U)return PrimitiveKind::Sphere;
    return PrimitiveKind::Unknown;
}
// High 12 uses individual message_template fields: ProfileCurve[1],
// PathScaleX[6], PathScaleY[7]. High 13's packed format differs.
PrimitiveKind full_simple_shape(std::span<const std::uint8_t> s) noexcept {
    if(s.size()!=23U)return PrimitiveKind::Unknown;
    for(unsigned i=2U;i<=5U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    if(s[6]>200U||s[7]>200U)return PrimitiveKind::Unknown;
    for(unsigned i=8U;i<=16U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    for(unsigned i=17U;i<23U;++i)
        if(s[i]!=0U)return PrimitiveKind::Unknown;
    if((s[1]&0xF0U)!=0U)return PrimitiveKind::Unknown;
    const auto profile=s[1]&0x0FU;
    if(s[0]==0x10U&&profile==0x01U)return PrimitiveKind::Box;
    if(s[0]==0x10U&&profile==0x00U)return PrimitiveKind::Cylinder;
    if(s[0]==0x10U&&profile==0x03U)return PrimitiveKind::Prism;
    if(s[0]==0x20U&&profile==0x05U)return PrimitiveKind::Sphere;
    return PrimitiveKind::Unknown;
}
}
// Packed High 13 classifier; full High 12 uses full_simple_shape.
PrimitiveKind classify_simple_shape(
    std::span<const std::uint8_t> s) noexcept {
    return simple_shape(s);
}
// High 12 ObjectUpdate, per scripts/messages/message_template.msg and
// LLViewerObject::processUpdateMessage(OUT_FULL). Unlike High 13,
// every block has dozens of fixed and length-prefixed fields. Consume
// every field so the next block begins at the *correct* boundary.
bool decode_full_transforms(const lludp::Packet& packet,
                            std::vector<Transform>& out,
                            FullDecodeAudit* audit,
                            std::vector<std::uint32_t>* parent_references,
                            std::vector<Transform>* child_transforms,
                            std::vector<std::uint32_t>* avatar_root_ids,
                            std::vector<AvatarFullPose>* avatar_poses) noexcept {
    const auto info=worldwire::inspect(packet);
    if(info.kind!=worldwire::Kind::ObjectFull||info.blocks==0U||
       info.blocks>64U)return false;
    const std::span<const std::uint8_t> b(packet.body);
    if(b.size()<12U)return false;
    Cursor cur(b.subspan(12U));
    std::vector<Transform> decoded;
    std::vector<Transform> children;
    decoded.reserve(info.blocks);
    children.reserve(info.blocks);
    FullDecodeAudit frame{};
    std::vector<std::uint32_t> parents;
    std::vector<std::uint32_t> avatars;
    std::vector<AvatarFullPose> decoded_avatar_poses;
    for(unsigned n=0U;n<info.blocks;++n){
        Transform t{};
        std::uint8_t pcode=0U;
        std::uint32_t parent=0U;
        std::span<const std::uint8_t> movement{};
        std::span<const std::uint8_t> full_id{};
        // ID U32, State U8, FullID UUID, CRC U32, PCode U8,
        // Material U8, ClickAction U8, Scale LLVector3.
        if(!cur.u32(t.local_id)||!cur.skip(1U)||
           !cur.take(16U,full_id)||!cur.skip(4U)||
           !cur.u8(pcode)||!cur.skip(2U)||
           !cur.f32(t.sx)||!cur.f32(t.sy)||!cur.f32(t.sz)||
           !cur.variable(1U,movement)||
           !cur.u32(parent)||!cur.skip(4U))return false;
        // Preserve exactly 23 shape bytes (PathCurve through
        // ProfileHollow), but NEVER manufacture unspecified topology.
        std::span<const std::uint8_t> shape{};
        if(!cur.take(23U,shape))return false;
        t.primitive=full_simple_shape(shape);
        t.volume_params_seen=true;
        t.volume_params_full_layout=true;
        std::copy(shape.begin(),shape.end(),t.volume_params.begin());
        // TextureEntry Var2; TextureAnim Var1; NameValue Var2;
        // Data Var2; Text Var1; TextColor Fixed4;
        // MediaURL Var1; PSBlock Var1; ExtraParams Var1.
        std::span<const std::uint8_t> texture_entry{};
        std::span<const std::uint8_t> extras{};
        if(!cur.variable(2U,texture_entry)||!cur.skip_variable(1U)||
           !cur.skip_variable(2U)||!cur.skip_variable(2U)||
           !cur.skip_variable(1U)||!cur.skip(4U)||
           !cur.skip_variable(1U)||!cur.skip_variable(1U)||
           !cur.variable(1U,extras))return false;
        // Sound UUID, OwnerID UUID, Gain F32, Flags U8,
        // Radius F32, JointType U8, JointPivot LLVector3,
        // JointAxisOrAnchor LLVector3.
        if(!cur.skip(16U+16U+4U+1U+4U+1U+12U+12U))
            return false;
        if(pcode!=9U){
            ++frame.non_volume;
            // PCode 47 is an avatar root, NOT a volume to place in-world.
            if(pcode==47U&&parent==0U&&t.local_id!=0U){
                avatars.push_back(t.local_id);
                const std::size_t offset=
                    movement.size()==76U||movement.size()==140U?16U:0U;
                if(movement.size()==60U||movement.size()==76U||
                   movement.size()==124U||movement.size()==140U){
                    AvatarFullPose pose{};
                    pose.local_id=t.local_id;
                    std::copy_n(full_id.begin(),16U,pose.full_id.begin());
                    const auto read=[&](std::size_t at,float& v) noexcept {
                        if(at+4U>movement.size())return false;
                        std::uint32_t bits=
                            std::uint32_t(movement[at])|
                            (std::uint32_t(movement[at+1U])<<8U)|
                            (std::uint32_t(movement[at+2U])<<16U)|
                            (std::uint32_t(movement[at+3U])<<24U);
                        std::memcpy(&v,&bits,sizeof(v));
                        return true;
                    };
                    if(read(offset,pose.x)&&read(offset+4U,pose.y)&&
                       read(offset+8U,pose.z)&&
                       std::isfinite(pose.x)&&std::isfinite(pose.y)&&
                       std::isfinite(pose.z)&&
                       pose.x>=0.f&&pose.x<256.f&&
                       pose.y>=0.f&&pose.y<256.f&&
                       pose.z>=-4096.f&&pose.z<8192.f)
                        decoded_avatar_poses.push_back(pose);
                }
            }
            continue;
        }
        ++frame.volume;
        // The separately replaceable LGPL parser uses the official
        // Linden Lab LLNetworkData parameter type/length table.
        // Modern 0x70, 0x80 and 0x90 types are NOT mesh asset UUIDs.
        KamiSlExtraParamsV1 typed{};
        typed.struct_size=sizeof(typed);
        const bool extras_valid=kami_sl_decode_extra_params_v1(
            extras.empty()?nullptr:extras.data(),
            static_cast<std::uint32_t>(extras.size()),&typed)!=0;
        if(extras_valid&&!extras.empty()){
            ++frame.extra_envelopes;
            frame.extra_records+=typed.records;
            frame.sculpt_records+=typed.sculpt;
            frame.mesh_sculpt_records+=typed.mesh_sculpt;
            frame.flexible_records+=typed.flexible;
            frame.light_records+=typed.light;
            frame.projector_records+=typed.light_image;
            frame.extended_mesh_records+=typed.extended_mesh;
            frame.render_material_records+=typed.render_material;
            frame.reflection_probe_records+=typed.reflection_probe;
            frame.other_extra_records+=typed.unknown;
        }else if(!extras_valid){
            ++frame.malformed_extra_envelopes;
        }
        // A corrupt sculpt envelope may conceal mesh topology. Never
        // render a guessed primitive for such an object.
        if(extras_valid){
            t.mesh_asset_id=mesh::mesh_asset_from_extras(extras);
            t.material_params_seen=pbr::decode_face_ids(extras,t.face_material_ids);
            (void)pbr::decode_probe(extras,t.reflection_probe,t.probe_ambiance,t.probe_clip,t.probe_flags);
        }
        else t.primitive=PrimitiveKind::Unknown;
        if(!t.mesh_asset_id.empty()){
            t.primitive=PrimitiveKind::Unknown;
            ++frame.mesh_references;
        }
        if(texture_entry.size()>=16U &&
           std::any_of(texture_entry.begin(),texture_entry.begin()+16U,
                       [](std::uint8_t ch){return ch!=0U;})){
            t.default_texture_present=true;
            ++frame.texture_references;
        }
        texture::Base base{};
        if(texture::decode_base(texture_entry,base)){
            t.base_tint_present=true;
            t.face_rgba_present=true;
            t.default_texture_asset_id=texture::default_image_uuid(base);
            for(unsigned face=0U;face<t.face_texture_override.size();++face){
                if(base.face_image_override[face]){
                    t.face_texture_override[face]=true;
                    t.face_texture_asset_ids[face]=texture::face_image_uuid(base,face);
                }
                t.face_uv[face]=base.uv[face];
                t.face_rgba[face]=base.rgba[face];
            }
            t.base_r=base.red;t.base_g=base.green;
            t.base_b=base.blue;t.base_a=base.alpha;
        }
        t.parent_id=parent;
        if(parent!=0U){
            ++frame.linked;
            if(parent!=t.local_id &&
               std::find(parents.begin(),parents.end(),parent)==parents.end())
                parents.push_back(parent);
        }
        if(t.primitive==PrimitiveKind::Unknown)
            ++frame.unsupported_shapes;
        // Per the official viewer, full-precision ObjectData has
        // 60-byte position/velocity/acceleration/rotation/omega,
        // optionally prefixed by 16-byte avatar collision plane.
        // The 124/140 variants include extended data; the same
        // position offset remains valid.
        std::size_t pos_offset=0U;
        if(movement.size()==76U||movement.size()==140U)
            pos_offset=16U;
        else if(movement.size()!=60U&&movement.size()!=124U){
            ++frame.unsupported_motion;
            continue;
        }
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
           !read_float(pos_offset+8U,t.z)||
           !read_float(pos_offset+36U,t.rx)||
           !read_float(pos_offset+40U,t.ry)||
           !read_float(pos_offset+44U,t.rz))return false;
        const float norm=t.rx*t.rx+t.ry*t.ry+t.rz*t.rz;
        if(!std::isfinite(norm)||norm>1.0001f){
            ++frame.invalid_transform;
            continue;
        }
        t.rw=std::sqrt(std::max(0.f,1.f-norm));
        if(valid(t)){
            if(parent==0U){
                decoded.push_back(t);
                ++frame.accepted_roots;
            }else{
                children.push_back(t);
                ++frame.accepted_children;
            }
        }else ++frame.invalid_transform;
    }
    if(!cur.done())return false;
    out=std::move(decoded);
    if(audit)*audit=frame; // publish only after full frame validation
    if(parent_references)*parent_references=std::move(parents);
    if(avatar_root_ids)*avatar_root_ids=std::move(avatars);
    if(avatar_poses)*avatar_poses=std::move(decoded_avatar_poses);
    if(child_transforms)*child_transforms=std::move(children);
    return true;
}
} // namespace kami::protocol::objectspatial

