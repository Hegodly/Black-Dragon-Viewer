#include "kami/protocol/object_spatial.hpp"
#include "kami/world/object_proxies.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

using namespace kami;
namespace {
void u16(std::vector<std::uint8_t>& b,std::uint16_t v){
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v>>8));
}
void u32(std::vector<std::uint8_t>& b,std::uint32_t v){
    for(unsigned i=0;i<4;++i)b.push_back(
        static_cast<std::uint8_t>(v>>(i*8)));
}
void f32(std::vector<std::uint8_t>& b,float v){
    std::uint32_t bits=0;
    std::memcpy(&bits,&v,sizeof(v));
    u32(b,bits);
}
std::vector<std::uint8_t> packed(std::uint32_t id,
                                 std::uint8_t pcode=9,
                                 std::uint32_t parent=0){
    std::vector<std::uint8_t> b(16,0); // object UUID
    u32(b,id);
    b.push_back(pcode);b.push_back(0); // PCode, State
    u32(b,123U);b.push_back(0);b.push_back(0); // CRC/material/action
    f32(b,2.f);f32(b,3.f);f32(b,4.f); // SL scale
    if(parent!=0U){
        f32(b,1.f);f32(b,2.f);f32(b,3.f); // linked local position
    }else{
        f32(b,128.f);f32(b,140.f);f32(b,35.f); // region position
    }
    f32(b,0.f);f32(b,0.f);f32(b,0.f); // rotation vector
    u32(b,parent!=0U?0x20U:0U); // special code
    b.insert(b.end(),16,0); // owner UUID
    if(parent!=0U)u32(b,parent);
    return b;
}
kami::protocol::lludp::Packet compressed(const std::vector<std::uint8_t>& one){
    kami::protocol::lludp::Packet p{};
    p.body.push_back(13);
    p.body.insert(p.body.end(),10,0); // region handle, time dilation
    p.body.push_back(1);
    u32(p.body,0U); // UpdateFlags
    u16(p.body,static_cast<std::uint16_t>(one.size()));
    p.body.insert(p.body.end(),one.begin(),one.end());
    return p;
}
}
int main(){
    using namespace protocol;
    const auto valid=compressed(packed(77U));
    std::vector<objectspatial::Transform> out;
    if(!objectspatial::decode_compressed_transforms(valid,out)||
       out.size()!=1U||out[0].local_id!=77U||
       out[0].x!=128.f||out[0].y!=140.f||out[0].z!=35.f||
       out[0].sx!=2.f||out[0].sy!=3.f||out[0].sz!=4.f)
        return 1;
    std::map<std::uint32_t,objectspatial::Transform> objects;
    objects.emplace(77U,out[0]);
    const auto mesh=world::build_live_object_spatial_proxies(
        objects,128.f,140.f,35.f);
    if(mesh.terrain_count!=432U||mesh.avatar_count!=0U||
       mesh.vertices.size()!=432U)return 2;
    for(const auto& v:mesh.vertices){
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||
           !std::isfinite(v.pz)||v.bone_weights!=0U)return 3;
    }
    // Compressed children may anchor nested shapes, but must never
    // masquerade as region-space roots or manufactured prim meshes.
    std::vector<objectspatial::Transform> children;
    if(!objectspatial::decode_compressed_transforms(
        compressed(packed(78U,9U,77U)),out,&children)||
       !out.empty()||children.size()!=1U||
       children[0].parent_id!=77U||children[0].local_id!=78U||
       children[0].x!=1.f||children[0].y!=2.f||children[0].z!=3.f||
       children[0].primitive!=objectspatial::PrimitiveKind::Unknown)
        return 4;
    // Avatar PCode must never masquerade as object volume.
    if(!objectspatial::decode_compressed_transforms(
        compressed(packed(78U,47U)),out)||!out.empty())
        return 5;
    auto broken=valid;
    broken.body.pop_back();
    if(objectspatial::decode_compressed_transforms(
           broken,out,&children)||children.size()!=1U)return 6;
    auto poisoned=packed(80U);
    const float nan=std::nanf("");
    std::uint32_t bits=0;
    std::memcpy(&bits,&nan,sizeof(bits));
    for(unsigned n=0;n<4;++n)
        poisoned[40+n]=static_cast<std::uint8_t>(bits>>(8*n));
    if(!objectspatial::decode_compressed_transforms(
        compressed(poisoned),out)||!out.empty())return 7;
    lludp::Packet killed{};
    killed.body={16,2,77,0,0,0,78,0,0,0};
    std::vector<std::uint32_t> ids;
    if(!objectspatial::decode_killed_ids(killed,ids)||
       ids.size()!=2U||ids[0]!=77U||ids[1]!=78U)return 8;
    killed.body.pop_back();
    if(objectspatial::decode_killed_ids(killed,ids))return 9;
    if(world::build_live_object_spatial_proxies(
        objects,0.f,0.f,0.f).terrain_count!=0U)return 10;
    // One ObjectUpdateCached High 14 frame with two object IDs.
    lludp::Packet cached{};
    cached.body={14U};
    cached.body.insert(cached.body.end(),10U,0U);
    cached.body.push_back(2U);
    u32(cached.body,77U);u32(cached.body,123U);u32(cached.body,0U);
    u32(cached.body,78U);u32(cached.body,124U);u32(cached.body,0U);
    if(!objectspatial::decode_cached_ids(cached,ids)||
       ids.size()!=2U||ids[0]!=77U||ids[1]!=78U)return 11;
    cached.body.pop_back();
    if(objectspatial::decode_cached_ids(cached,ids))return 12;

    // Compressed mesh ExtraParams (0x30, SculptType=5) follow parent data.
    // Never mistake a random UUID elsewhere in the compressed packet
    // for a mesh asset reference.
    auto withmesh=packed(93U,9U,77U);
    withmesh.push_back(1U); // one ExtraParam
    withmesh.insert(withmesh.end(),{0x30U,0U,17U,0U,0U,0U});
    for(unsigned i=0;i<16U;++i)
        withmesh.push_back(static_cast<std::uint8_t>(i+1U));
    withmesh.push_back(5U);
    if(!objectspatial::decode_compressed_transforms(
         compressed(withmesh),out,&children)||children.size()!=1U||
       children[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10")
       return 13;
    withmesh.back()=3U; // sculpt, not mesh
    if(!objectspatial::decode_compressed_transforms(
         compressed(withmesh),out,&children)||children.size()!=1U||
       !children[0].mesh_asset_id.empty())return 14;
    withmesh.back()=5U;
    withmesh[withmesh.size()-21U]=255U; // size field malformed
    if(!objectspatial::decode_compressed_transforms(
         compressed(withmesh),out,&children)||children.size()!=1U||
       !children[0].mesh_asset_id.empty())return 15;

    // A genuine High 13 volume carries ExtraParams followed by the
    // 23-byte profile/path block. R34 discarded all of these shapes.
    auto cbox=packed(110U);
    cbox.push_back(0U); // ExtraParam count = 0
    std::vector<std::uint8_t> shape(23U,0U);
    shape[0]=0x10U;shape[16]=0x01U;
    shape[5]=100U;shape[6]=100U;
    cbox.insert(cbox.end(),shape.begin(),shape.end());
    if(!objectspatial::decode_compressed_transforms(
          compressed(cbox),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Box)return 16;
    // A supported High 13 circular sweep must survive the object classifier:
    // cuts + circular hollow + neutral non-emitted path modifiers.
    auto swept=cbox;
    auto* shape_data=swept.data()+swept.size()-23U;
    shape_data[0]=0x10U;
    shape_data[3]=0x50U;shape_data[4]=0xc3U;
    shape_data[5]=100U;shape_data[6]=100U;
    for(unsigned i=7U;i<=15U;++i)shape_data[i]=0U;
    shape_data[16]=0x10U;
    shape_data[19]=0x50U;shape_data[20]=0xc3U;
    shape_data[21]=0x20U;shape_data[22]=0x4eU;
    if(objectspatial::classify_simple_shape(
           std::span<const std::uint8_t>(shape_data,23U))!=
           objectspatial::PrimitiveKind::Cylinder||
       !objectspatial::decode_compressed_transforms(
           compressed(swept),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Cylinder)
        return 17;
    // Circle profile cuts are emitted as closed radial wedges; both
    // hollow and solid circular variants must reach that geometry path.
    auto circle_shape=shape;circle_shape[16U]=0U;
    auto circle_cut=circle_shape;
    circle_cut[17U]=0x50U;circle_cut[18U]=0xc3U; // begin .2
    circle_cut[19U]=0x50U;circle_cut[20U]=0x9cU; // end .8
    if(objectspatial::classify_simple_shape(circle_cut)!=
       objectspatial::PrimitiveKind::Cylinder)return 49;
    auto hollow_circle=circle_shape;
    hollow_circle[16U]=0x10U;
    hollow_circle[21U]=0x20U;hollow_circle[22U]=0x4eU;
    if(objectspatial::classify_simple_shape(hollow_circle)!=
       objectspatial::PrimitiveKind::Cylinder)return 50;
    auto hollow_without_hole=circle_shape;
    hollow_without_hole[21U]=0x20U;hollow_without_hole[22U]=0x4eU;
    if(objectspatial::classify_simple_shape(hollow_without_hole)!=
       objectspatial::PrimitiveKind::Unknown)return 51;
    auto hole_without_hollow=circle_shape;
    hole_without_hollow[16U]=0x10U;
    if(objectspatial::classify_simple_shape(hole_without_hollow)!=
       objectspatial::PrimitiveKind::Unknown)return 52;
    auto square_profile_cut=shape;
    square_profile_cut[17U]=0x50U;square_profile_cut[18U]=0xc3U;
    if(objectspatial::classify_simple_shape(square_profile_cut)!=
       objectspatial::PrimitiveKind::Unknown)return 53;
    auto scaled_shape=cbox;
    scaled_shape[scaled_shape.size()-23U+5U]=50U;
    scaled_shape[scaled_shape.size()-23U+6U]=150U;
    if(!objectspatial::decode_compressed_transforms(
          compressed(scaled_shape),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Box||
       !out[0].volume_params_seen||out[0].volume_params_full_layout||
       out[0].volume_params[5U]!=50U||out[0].volume_params[6U]!=150U)return 17;
    // Square and triangular linear profiles are admitted only when
    // the full profile interval is valid and no unsupported hollow is set.
    auto polygon_shape=shape;
    polygon_shape[0U]=0x10U;
    polygon_shape[3U]=0x50U;polygon_shape[4U]=0xc3U;
    polygon_shape[19U]=0x50U;polygon_shape[20U]=0xc3U;
    auto square_profile=polygon_shape;
    square_profile[16U]=0x01U;
    if(objectspatial::classify_simple_shape(square_profile)!=
       objectspatial::PrimitiveKind::Box)return 30;
    auto triangle_profile=polygon_shape;
    triangle_profile[16U]=0x03U;
    if(objectspatial::classify_simple_shape(triangle_profile)!=
       objectspatial::PrimitiveKind::Prism)return 31;
    auto hollow_polygon=polygon_shape;
    hollow_polygon[16U]=0x11U;hollow_polygon[21U]=0x20U;
    hollow_polygon[22U]=0x4eU;
    if(objectspatial::classify_simple_shape(hollow_polygon)!=
       objectspatial::PrimitiveKind::Unknown)return 32;
    auto cut=cbox;
    cut[cut.size()-23U+7U]=1U; // unsupported shear/path modifier
    if(!objectspatial::decode_compressed_transforms(
          compressed(cut),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Unknown)return 29;
    auto linkedbox=packed(111U,9U,110U);
    linkedbox.push_back(0U);
    linkedbox.insert(linkedbox.end(),shape.begin(),shape.end());
    if(!objectspatial::decode_compressed_transforms(
          compressed(linkedbox),out,&children)||children.size()!=1U||
       children[0].primitive!=objectspatial::PrimitiveKind::Box)return 18;

    // One fully specified compressed mesh with a true SL volume tail.
    auto meshblock=packed(112U,9U,110U);
    meshblock.push_back(1U);
    meshblock.insert(meshblock.end(),{0x30U,0U,17U,0U,0U,0U});
    for(unsigned i=0U;i<16U;++i)
        meshblock.push_back(static_cast<std::uint8_t>(i+1U));
    meshblock.push_back(5U);
    meshblock.insert(meshblock.end(),shape.begin(),shape.end());
    objectspatial::CompressedDecodeAudit audit{};
    if(!objectspatial::decode_compressed_transforms(
         compressed(meshblock),out,&children,&audit)||
       children.size()!=1U||
       children[0].primitive!=objectspatial::PrimitiveKind::Unknown||
       !children[0].volume_params_seen||
       children[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10"||
       audit.mesh_references!=1U||audit.shape_fields!=1U)return 19;
    // Without a sculpt/mesh marker a genuine cylinder can use the
    // same path bytes; no speculative mesh identity is introduced.
    auto cylinder=packed(113U);
    auto round_shape=shape;
    round_shape[16]=0U;
    cylinder.push_back(0U);
    cylinder.insert(cylinder.end(),round_shape.begin(),round_shape.end());
    if(!objectspatial::decode_compressed_transforms(
           compressed(cylinder),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Cylinder)return 20;
    // Identical updates must not force redundant CPU/DX12 geometry rebuilds.
    objectspatial::Transform stable{501U,128.f,140.f,35.f,2.f,2.f,2.f};
    auto again=stable;
    if(!objectspatial::same_spatial_transform(stable,again))return 23;
    again.x+=.1f;
    if(objectspatial::same_spatial_transform(stable,again))return 24;
    again=stable;again.mesh_asset_id="01020304-0506-0708-090a-0b0c0d0e0f10";
    if(objectspatial::same_spatial_transform(stable,again))return 25;
    again=stable;again.primitive=objectspatial::PrimitiveKind::Box;
    if(objectspatial::same_spatial_transform(stable,again))return 26;
    // Official compressed scratch-pad envelope includes two independent
    // little-endian lengths, followed by the payload. Previously KAMI
    // abandoned this entire path and could miss valid following meshes.
    auto scratch=packed(801U);
    scratch[64U]=0x01U; // SpecialCode ScratchPad flag, not PCode
    scratch.insert(scratch.end(),{3,0,0,0,3,0,0,0,1,2,3});
    scratch.push_back(1U);
    scratch.insert(scratch.end(),{0x30U,0U,17U,0U,0U,0U});
    for(unsigned i=0U;i<16U;++i)
        scratch.push_back(static_cast<std::uint8_t>(i+1U));
    scratch.push_back(5U);
    scratch.insert(scratch.end(),shape.begin(),shape.end());
    objectspatial::CompressedDecodeAudit scratch_audit{};
    if(!objectspatial::decode_compressed_transforms(
         compressed(scratch),out,&children,&scratch_audit)||
       out.size()!=1U||
       out[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10"||
       scratch_audit.extra_records!=1U||
       scratch_audit.sculpt_records!=1U)return 27;
    scratch[84U+4U]=99U; // malformed ScratchPad embedded size
    if(!objectspatial::decode_compressed_transforms(
         compressed(scratch),out,&children,&scratch_audit)||
       out.size()!=1U||!out[0].mesh_asset_id.empty()||
       scratch_audit.extra_records!=0U)return 28;
    // Legacy particle payload is precisely 86 bytes and precedes
    // ExtraParams in genuine compressed SL object updates.
    auto legacy=packed(901U);
    legacy[64U]=0x08U;
    legacy.insert(legacy.end(),86U,0U);
    legacy.insert(legacy.end(),meshblock.begin()+88U,meshblock.end());
    objectspatial::CompressedDecodeAudit legacy_audit{};
    if(!objectspatial::decode_compressed_transforms(
           compressed(legacy),out,&children,&legacy_audit)||
       out.size()!=1U||out[0].mesh_asset_id!=
           "01020304-0506-0708-090a-0b0c0d0e0f10"||
       legacy_audit.legacy_particle_blocks!=1U||
       legacy_audit.mesh_references!=1U)return 29;
    // Truncated particle stream must never yield a fabricated UUID.
    legacy.resize(84U+60U);
    if(!objectspatial::decode_compressed_transforms(
           compressed(legacy),out,&children,&legacy_audit)||
       out.size()!=1U||!out[0].mesh_asset_id.empty()||
       legacy_audit.mesh_references!=0U)return 30;
    // A genuine compressed full volume stores a U32 texture-entry byte
    // length immediately after the 23 shape bytes, then the TE payload.
    auto tinted=cbox;
    std::vector<std::uint8_t> te(16U,0x31U);
    te.insert(te.end(),{0U,255U,0U,128U,0U});
    u32(tinted,static_cast<std::uint32_t>(te.size()));
    tinted.insert(tinted.end(),te.begin(),te.end());
    if(!objectspatial::decode_compressed_transforms(
           compressed(tinted),out)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Box||
       !out[0].base_tint_present||!out[0].default_texture_present||
       out[0].base_r!=0.f||out[0].base_g!=1.f||
       std::abs(out[0].base_b-127.f/255.f)>.001f)return 31;
    auto previous=out[0];
    auto sparse=objectspatial::Transform{previous.local_id,129.f,140.f,35.f,
                                         2.f,3.f,4.f};
    const auto merged=objectspatial::merge_compressed_transform(sparse,previous);
    if(!merged.base_tint_present||merged.base_r!=0.f||
       merged.base_g!=1.f||merged.primitive!=objectspatial::PrimitiveKind::Box||
       !merged.default_texture_present)return 32;
    auto malformed_tint=tinted;
    malformed_tint.pop_back();
    if(!objectspatial::decode_compressed_transforms(
           compressed(malformed_tint),out)||out.size()!=1U||
       out[0].base_tint_present)return 33;
    // Compressed High 13 mesh alias is decoded via the official
    // extra-param bridge; extended 0x70 is not a mesh UUID.
    auto mesh_alias=meshblock;
    mesh_alias[mesh_alias.size()-23U-17U-6U]=0x60U;
    objectspatial::CompressedDecodeAudit alias_audit{};
    if(!objectspatial::decode_compressed_transforms(
          compressed(mesh_alias),out,&children,&alias_audit)||
       children.size()!=1U||
       children[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10"||
       alias_audit.mesh_references!=1U||alias_audit.sculpt_records!=1U)
        return 40;
    mesh_alias[mesh_alias.size()-23U-17U-6U]=0x70U;
    if(!objectspatial::decode_compressed_transforms(
          compressed(mesh_alias),out,&children,&alias_audit)||
       children.size()!=1U||!children[0].mesh_asset_id.empty()||
       alias_audit.mesh_references!=0U)
        return 41;

    // R67: stream by the actual composed child position, not merely
    // the parent root. A building whose root lies outside the 144m
    // stream radius may contain perfectly visible linked mesh pieces.
    {
        using Transform=objectspatial::Transform;
        Transform remote_root{1001U,252.f,140.f,35.f,2.f,2.f,2.f};
        Transform near_child{1002U,-127.f,0.f,0.f,1.f,1.f,1.f};
        near_child.parent_id=1001U;
        const std::map<std::uint32_t,Transform> roots{
            {1001U,remote_root}};
        const std::map<std::uint32_t,Transform> links{
            {1002U,near_child}};
        std::array<float,3> world_position{};
        if(!objectspatial::resolve_world_position(
                near_child,roots,links,{},world_position)||
           std::abs(world_position[0]-125.f)>.001f||
           std::abs(world_position[1]-140.f)>.001f||
           std::abs(world_position[2]-35.f)>.001f||
           std::abs(remote_root.x-50.f)<=144.f||
           std::abs(world_position[0]-50.f)>144.f)
            return 42;
        constexpr float half=0.7071067811865476f;
        auto rotated_root=remote_root;
        rotated_root.x=128.f;rotated_root.rz=half;rotated_root.rw=half;
        auto rotated_child=near_child;
        rotated_child.x=1.f;
        auto grandchild=near_child;grandchild.local_id=1003U;
        grandchild.parent_id=1002U;
        grandchild.x=0.f;grandchild.y=2.f;
        const std::map<std::uint32_t,Transform> rotated_roots{
            {1001U,rotated_root}};
        const std::map<std::uint32_t,Transform> rotated_links{
            {1002U,rotated_child},{1003U,grandchild}};
        if(!objectspatial::resolve_world_position(
                rotated_child,rotated_roots,rotated_links,{},
                world_position)||
           std::abs(world_position[0]-128.f)>.001f||
           std::abs(world_position[1]-141.f)>.001f)return 43;
        if(!objectspatial::resolve_world_position(
                grandchild,rotated_roots,rotated_links,{},
                world_position)||
           std::abs(world_position[0]-126.f)>.001f||
           std::abs(world_position[1]-141.f)>.001f)return 44;
        const auto retained=world_position;
        if(objectspatial::resolve_world_position(
                near_child,{},links,{},world_position)||
           world_position!=retained)return 45;
        auto cycle=near_child;cycle.parent_id=1002U;
        if(objectspatial::resolve_world_position(
                cycle,roots,{{1002U,cycle}},{},world_position)||
           world_position!=retained)return 46;
        auto nonunit=rotated_root;nonunit.rw=3.f;
        if(objectspatial::resolve_world_position(
                near_child,{{1001U,nonunit}},links,{},world_position))
            return 47;
        if(objectspatial::resolve_world_position(
                near_child,roots,links,{1001U},world_position))
            return 48;
    }
    return 0;
}
