#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/pbr_material.hpp"
#include "kami/protocol/world_wire.hpp"
#include "kami/protocol/mesh_reference.hpp"
#include "kami/protocol/texture_entry.hpp"
#include "kami/protocol/official_extra_params.h"
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
struct CompressedVolume {
    std::array<std::string,32U> face_material_ids{};
    bool material_params_seen=false;
    bool reflection_probe=false;float probe_ambiance=1.f,probe_clip=.1f;std::uint8_t probe_flags=0U;
    std::string mesh;
    PrimitiveKind primitive=PrimitiveKind::Unknown;
    bool extras=false,shape=false,unsupported=false;
    bool base_tint_present=false,default_texture_present=false;
    std::string default_texture_asset_id;
    std::array<std::string,32U> face_texture_asset_ids{};
    std::array<bool,32U> face_texture_override{};
    std::array<texture::UVTransform,32U> face_uv{};
    std::array<std::array<float,4U>,32U> face_rgba{};
    bool face_rgba_present=false;
    float base_r=1.f,base_g=1.f,base_b=1.f,base_a=1.f;
    std::uint32_t extra_records=0,sculpt_records=0;
    bool legacy_particles=false;
};
CompressedVolume decode_volume_tail(std::span<const std::uint8_t> data,
                                    std::size_t at,std::uint32_t special){
    CompressedVolume out{};
    const auto has=[&](std::size_t count){
        return at<=data.size()&&count<=data.size()-at;
    };
    if(special&0x02U){
        if(!has(1U))return out;
        ++at;
    }else if(special&0x01U){
        // Official LLViewerObject reads ScratchPadSize (U32 LE), then
        // unpackBinaryData reads a second length (S32 LE) and payload.
        // Declared and embedded sizes must agree before parsing extras.
        if(!has(8U))return out;
        const auto declared=u32(data,at);
        const auto embedded=u32(data,at+4U);
        at+=8U;
        if(declared>4096U||embedded!=declared||!has(embedded))
            return out;
        at+=embedded;
    }
    const auto str=[&]() -> bool {
        for(std::size_t i=0;i<2048U&&has(1U);++i){
            if(data[at++]==0U)return true;
        }
        return false;
    };
    if(special&0x04U){
        if(!str()||!has(4U))return out;
        at+=4U;
    }
    if((special&0x200U)&&!str())return out;
    if(special&0x08U){
        // Official LLPartSysData::unpackLegacy consumes exactly
        // PS_SYS_DATA_BLOCK_SIZE (68) + PS_LEGACY_PART_DATA_BLOCK_SIZE
        // (18) bytes, then LLViewerObject reads the ExtraParams count.
        // Skip only this known fixed-length legacy format. Particles
        // themselves are NOT rendered by this path.
        constexpr std::size_t legacy_particles=68U+18U;
        if(!has(legacy_particles)){out.unsupported=true;return out;}
        at+=legacy_particles;
        out.legacy_particles=true;
    }
    if(!has(1U))return out;
    const auto first=at;
    const auto count=data[at++];
    if(count>32U)return out;
    for(unsigned i=0;i<count;++i){
        if(!has(6U))return out;
        const std::size_t length=u32(data,at+2U);
        at+=6U;
        if(length>4096U||!has(length))return out;
        at+=length;
    }
    // Validate the complete envelope using Linden Lab's parameter type
    // sizes (0x30 sculpt, 0x70 extended flags, 0x80 render material).
    const auto extras=data.subspan(first,at-first);
    KamiSlExtraParamsV1 summary{};
    summary.struct_size=sizeof(summary);
    if(!kami_sl_decode_extra_params_v1(extras.data(),
          static_cast<std::uint32_t>(extras.size()),&summary)){
        out.unsupported=true;
        return out;
    }
    out.extra_records=summary.records;
    out.sculpt_records=summary.sculpt;
    out.extras=true;
    out.material_params_seen=pbr::decode_face_ids(extras,out.face_material_ids);
    (void)pbr::decode_probe(extras,out.reflection_probe,out.probe_ambiance,out.probe_clip,out.probe_flags);
    out.mesh=mesh::mesh_asset_from_extras(extras);
    // LLViewerObject processes sound and NV fields BEFORE
    // LLVOVolume::unpackVolumeParams() consumes 23 shape bytes.
    if(special&0x10U){
        if(!has(25U))return out;
        at+=25U;
    }
    if((special&0x100U)&&!str())return out;
    if(!has(23U))return out;
    out.primitive=classify_simple_shape(data.subspan(at,23U));
    out.shape=true;
    at+=23U;
    // LLVOVolume unpacks TextureEntry immediately after the volume
    // parameters. LLDataPackerBinaryBuffer uses LE S32 length then TE.
    // Malformed optional TE data never produces a fabricated color.
    if(has(4U)){
        const std::uint32_t length=u32(data,at);
        at+=4U;
        if(length<=4096U&&has(length)&&length>=16U){
            const auto te=data.subspan(at,length);
            texture::Base tint{};
            if(texture::decode_base(te,tint)){
                out.base_tint_present=true;
                out.face_rgba_present=true;
                out.default_texture_asset_id=texture::default_image_uuid(tint);
                for(unsigned face=0U;face<out.face_texture_override.size();++face){
                    if(!tint.face_image_override[face])continue;
                    out.face_texture_override[face]=true;
                    out.face_texture_asset_ids[face]=texture::face_image_uuid(tint,face);
                }
                out.base_r=tint.red;out.base_g=tint.green;
                out.base_b=tint.blue;out.base_a=tint.alpha;
                for(unsigned f=0U;f<32U;++f){
                    out.face_uv[f]=tint.uv[f];
                    out.face_rgba[f]=tint.rgba[f];
                }
                out.default_texture_present=
                    std::any_of(tint.image_id.begin(),tint.image_id.end(),
                                [](std::uint8_t value){return value!=0U;});
            }
        }
    }
    if(!out.mesh.empty())out.primitive=PrimitiveKind::Unknown;
    return out;
}
} // namespace

bool decode_compressed_transforms(const lludp::Packet& packet,
                                  std::vector<Transform>& out,
                                  std::vector<Transform>* child_transforms,
                                  CompressedDecodeAudit* audit) noexcept {
    // High 13: ObjectUpdateCompressed. All blocks must have bounded
    // framing before any untrusted fields are read.
    if(worldwire::inspect(packet).kind!=worldwire::Kind::ObjectCompressed ||
       !worldwire::inspect(packet).complete_frame)return false;
    const std::span<const std::uint8_t> b(packet.body);
    const unsigned count=b[11];
    std::size_t at=12;
    std::vector<Transform> decoded;
    std::vector<Transform> linked;
    CompressedDecodeAudit frame{};
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
        const auto shape=decode_volume_tail(data,cursor,special);
        t.mesh_asset_id=shape.mesh;
        t.face_material_ids=shape.face_material_ids;
        t.material_params_seen=shape.material_params_seen;
        t.reflection_probe=shape.reflection_probe;t.probe_ambiance=shape.probe_ambiance;
        t.probe_clip=shape.probe_clip;t.probe_flags=shape.probe_flags;
        t.primitive=shape.primitive;
        t.volume_params_seen=shape.shape;
        t.default_texture_present=shape.default_texture_present;
        t.default_texture_asset_id=shape.default_texture_asset_id;
        t.face_texture_asset_ids=shape.face_texture_asset_ids;
        t.face_texture_override=shape.face_texture_override;
        t.face_uv=shape.face_uv;
        t.face_rgba=shape.face_rgba;
        t.face_rgba_present=shape.face_rgba_present;
        t.base_tint_present=shape.base_tint_present;
        t.base_r=shape.base_r;t.base_g=shape.base_g;
        t.base_b=shape.base_b;t.base_a=shape.base_a;
        if(shape.extras)++frame.extras_seen;
        frame.extra_records+=shape.extra_records;
        frame.sculpt_records+=shape.sculpt_records;
        if(shape.shape)++frame.shape_fields;
        if(shape.primitive!=PrimitiveKind::Unknown)++frame.supported_shapes;
        if(!shape.mesh.empty())++frame.mesh_references;
        if(shape.unsupported)++frame.unsupported_optional;
        if(shape.legacy_particles)++frame.legacy_particle_blocks;
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
    if(audit)*audit=frame;
    return true;
}
bool resolve_world_position(
    const Transform& object,
    const std::map<std::uint32_t,Transform>& roots,
    const std::map<std::uint32_t,Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    std::array<float,3>& world) noexcept {
    const auto finite_pose=[](const Transform& t) noexcept {
        const float sq=t.rx*t.rx+t.ry*t.ry+t.rz*t.rz+t.rw*t.rw;
        return t.local_id!=0U&&std::isfinite(t.x)&&
            std::isfinite(t.y)&&std::isfinite(t.z)&&
            std::isfinite(sq)&&sq>.99f&&sq<1.01f;
    };
    if(!finite_pose(object))return false;
    // Compose root-relative and nested-child offsets the same way the
    // actual DX12 mesh hierarchy does. Root-only distance calculations
    // omit valid nearby children of a distant structural linkset.
    std::array<float,3> position{object.x,object.y,object.z};
    std::array<std::uint32_t,16U> visited{};
    std::size_t depth=0U;
    visited[depth++]=object.local_id;
    const Transform* current=&object;
    while(current->parent_id!=0U){
        if(depth>=visited.size())return false;
        const auto id=current->parent_id;
        if(avatar_roots.contains(id)||
           std::find(visited.begin(),visited.begin()+depth,id)!=
               visited.begin()+depth)return false;
        visited[depth++]=id;
        const Transform* parent=nullptr;
        const auto root=roots.find(id);
        if(root!=roots.end()){
            if(root->second.parent_id!=0U)return false;
            parent=&root->second;
        }else{
            const auto child=children.find(id);
            if(child==children.end()||child->second.parent_id==0U)
                return false;
            parent=&child->second;
        }
        if(!finite_pose(*parent))return false;
        const float qx=parent->rx,qy=parent->ry,qz=parent->rz,qw=parent->rw;
        const float tx=2.f*(qy*position[2]-qz*position[1]);
        const float ty=2.f*(qz*position[0]-qx*position[2]);
        const float tz=2.f*(qx*position[1]-qy*position[0]);
        position={
            parent->x+position[0]+qw*tx+(qy*tz-qz*ty),
            parent->y+position[1]+qw*ty+(qz*tx-qx*tz),
            parent->z+position[2]+qw*tz+(qx*ty-qy*tx)
        };
        if(!std::isfinite(position[0])||!std::isfinite(position[1])||
           !std::isfinite(position[2]))return false;
        current=parent;
    }
    world=position;
    return true;
}
bool same_spatial_transform(const Transform& a,const Transform& b) noexcept {
    return a.local_id==b.local_id&&a.parent_id==b.parent_id&&
        a.x==b.x&&a.y==b.y&&a.z==b.z&&
        a.sx==b.sx&&a.sy==b.sy&&a.sz==b.sz&&
        a.rx==b.rx&&a.ry==b.ry&&a.rz==b.rz&&a.rw==b.rw&&
        a.primitive==b.primitive&&a.mesh_asset_id==b.mesh_asset_id&&
        a.volume_params_seen==b.volume_params_seen&&
        a.default_texture_present==b.default_texture_present&&
        a.default_texture_asset_id==b.default_texture_asset_id&&
        a.face_texture_asset_ids==b.face_texture_asset_ids&&
        a.face_texture_override==b.face_texture_override&&
        a.face_material_ids==b.face_material_ids&&a.material_params_seen==b.material_params_seen&&
        a.reflection_probe==b.reflection_probe&&a.probe_ambiance==b.probe_ambiance&&
        a.probe_clip==b.probe_clip&&a.probe_flags==b.probe_flags&&
        a.face_uv==b.face_uv&&a.face_rgba==b.face_rgba&&
        a.face_rgba_present==b.face_rgba_present&&
        a.base_tint_present==b.base_tint_present&&
        a.base_r==b.base_r&&a.base_g==b.base_g&&
        a.base_b==b.base_b&&a.base_a==b.base_a;
}
Transform merge_compressed_transform(
    const Transform& incoming,const Transform& previous){
    Transform merged=incoming;
    if(incoming.local_id==0U||incoming.local_id!=previous.local_id)
        return merged;
    if(!incoming.volume_params_seen){
        merged.primitive=previous.primitive;
        if(merged.mesh_asset_id.empty())
            merged.mesh_asset_id=previous.mesh_asset_id;
        merged.volume_params_seen=previous.volume_params_seen;
    }
    if(!incoming.material_params_seen){
        merged.face_material_ids=previous.face_material_ids;
        merged.material_params_seen=previous.material_params_seen;
        merged.reflection_probe=previous.reflection_probe;merged.probe_ambiance=previous.probe_ambiance;
        merged.probe_clip=previous.probe_clip;merged.probe_flags=previous.probe_flags;
    }
    merged.default_texture_present=
        merged.default_texture_present||previous.default_texture_present;
    if(merged.default_texture_asset_id.empty()&&
       !incoming.base_tint_present)
        merged.default_texture_asset_id=previous.default_texture_asset_id;
    if(!incoming.base_tint_present){
        merged.face_texture_asset_ids=previous.face_texture_asset_ids;
        merged.face_texture_override=previous.face_texture_override;
        merged.face_uv=previous.face_uv;
        merged.face_rgba=previous.face_rgba;
        merged.face_rgba_present=previous.face_rgba_present;
    }
    if(!incoming.base_tint_present&&previous.base_tint_present){
        merged.base_tint_present=true;
        merged.base_r=previous.base_r;merged.base_g=previous.base_g;
        merged.base_b=previous.base_b;merged.base_a=previous.base_a;
    }
    // A validated sculpt/mesh marker always takes priority over a
    // previous simple-prim classification.
    if(!merged.mesh_asset_id.empty())
        merged.primitive=PrimitiveKind::Unknown;
    return merged;
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
std::vector<std::uint32_t> expand_killed_linkset_ids(
    std::span<const std::uint32_t> killed_ids,
    const std::map<std::uint32_t,Transform>& linked_objects) {
    std::vector<std::uint32_t> expanded;
    std::set<std::uint32_t> seen;
    expanded.reserve(killed_ids.size()+linked_objects.size());
    for(const auto id:killed_ids)
        if(id!=0U&&seen.insert(id).second)expanded.push_back(id);
    // KillObject can name a linkset root while the retained child updates
    // remain in a separate map. Expand transitively so no descendant keeps
    // a dangling parent transform (including nested linked children).
    bool changed=true;
    while(changed){
        changed=false;
        for(const auto& [id,child]:linked_objects){
            if(id==0U||child.local_id!=id||child.parent_id==0U||
               id==child.parent_id||!seen.contains(child.parent_id))continue;
            if(seen.insert(id).second){
                expanded.push_back(id);
                changed=true;
            }
        }
    }
    return expanded;
}
} // namespace kami::protocol::objectspatial

