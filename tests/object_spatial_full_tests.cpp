#include "kami/protocol/object_spatial.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
using Byte=std::uint8_t;
using Bytes=std::vector<Byte>;
void u16(Bytes& b,std::uint16_t v) {
    b.push_back(static_cast<Byte>(v));
    b.push_back(static_cast<Byte>(v>>8U));
}
void u32(Bytes& b,std::uint32_t v) {
    for(unsigned i=0;i<4U;++i)b.push_back(
        static_cast<Byte>(v>>(i*8U)));
}
void f32(Bytes& b,float v) {
    std::uint32_t bits=0;
    std::memcpy(&bits,&v,sizeof(bits));
    u32(b,bits);
}
void var1(Bytes& b,const Bytes& data={}) {
    b.push_back(static_cast<Byte>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
void var2(Bytes& b,const Bytes& data={}) {
    u16(b,static_cast<std::uint16_t>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
Bytes block(std::uint32_t id,std::uint32_t parent=0,
            Byte pcode=9,float x=128.f,const Bytes& extras={}) {
    Bytes b;
    u32(b,id); // LocalID
    b.push_back(0U); // State
    b.insert(b.end(),16U,0U); // UUID
    u32(b,1U); // CRC
    b.push_back(pcode); // Volume=9
    b.push_back(0U);b.push_back(0U); // material/action
    f32(b,2.f);f32(b,3.f);f32(b,4.f); // scale
    Bytes pos;
    f32(pos,x);f32(pos,140.f);f32(pos,35.f);
    pos.insert(pos.end(),48U,0U); // 60-byte full precision
    var1(b,pos);
    u32(b,parent);u32(b,0U); // ParentID and UpdateFlags
    b.insert(b.end(),23U,0U); // Prim path/profile shape
    var2(b);var1(b);var2(b);var2(b);var1(b);
    b.insert(b.end(),4U,0U); // TextColor
    var1(b);var1(b);var1(b,extras); // MediaURL, particles, extra
    b.insert(b.end(),66U,0U); // sound + joint data
    return b;
}
kami::protocol::lludp::Packet packet(const std::vector<Bytes>& blocks) {
    kami::protocol::lludp::Packet p{};
    p.body.push_back(12U); // ObjectUpdate High 12
    p.body.insert(p.body.end(),10U,0U); // RegionData
    p.body.push_back(static_cast<Byte>(blocks.size()));
    for(const auto& b:blocks)
        p.body.insert(p.body.end(),b.begin(),b.end());
    return p;
}
}
int main(){
    using namespace kami::protocol;
    std::vector<objectspatial::Transform> out;
    auto live=packet({block(77U),block(78U,77U)});
    objectspatial::FullDecodeAudit audit{};
    std::vector<std::uint32_t> parents;
    if(!objectspatial::decode_full_transforms(live,out,&audit,&parents)||
       out.size()!=1U||out[0].local_id!=77U||
       out[0].x!=128.f||out[0].y!=140.f||out[0].z!=35.f||
       out[0].sx!=2.f||out[0].sy!=3.f||out[0].sz!=4.f||
       audit.volume!=2U||audit.accepted_roots!=1U||
       audit.linked!=1U||audit.non_volume!=0U||
       parents.size()!=1U||parents[0]!=77U)return 1;
    // Full updates can have more than one variable-length field. Guard
    // against accidentally using an offset into the preceding block.
    auto second=packet({block(77U),block(80U,0,9U,222.f)});
    if(!objectspatial::decode_full_transforms(second,out)||
       out.size()!=2U||out[1].local_id!=80U||
       out[1].x!=222.f)return 2;
    // Official ObjectUpdate 23-byte profile/path layout starts after
    // fixed 40-byte record prefix, 61-byte movement and 8-byte flags.
    auto box=block(90U);
    box[109]=0x10U;box[110]=0x01U;
    box[115]=100U;box[116]=100U;
    auto cyl=block(91U,90U,9U,1.f);
    cyl[109]=0x10U;cyl[110]=0x00U;
    cyl[115]=100U;cyl[116]=100U;
    auto triangle=block(93U);
    triangle[109]=0x10U;triangle[110]=0x03U;
    triangle[115]=100U;triangle[116]=100U;
    if(!objectspatial::decode_full_transforms(
           packet({triangle}),out,&audit)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Prism)return 11;
    auto round=block(92U,90U,9U,2.f);
    round[109]=0x20U;round[110]=0x05U;
    round[115]=100U;round[116]=100U;
    // Linked-child ObjectData positions are local to the parent,
    // not region-scale. Use small finite relative XY coordinates.
    const float local_y=3.f;
    std::memcpy(cyl.data()+45U,&local_y,sizeof(local_y));
    std::memcpy(round.data()+45U,&local_y,sizeof(local_y));
    std::vector<objectspatial::Transform> linked;
    if(!objectspatial::decode_full_transforms(
           packet({box,cyl,round}),out,&audit,&parents,&linked)||
       out.size()!=1U||linked.size()!=2U||
       out[0].primitive!=objectspatial::PrimitiveKind::Box||
       linked[0].primitive!=objectspatial::PrimitiveKind::Cylinder||
       linked[1].primitive!=objectspatial::PrimitiveKind::Sphere||
       linked[0].parent_id!=90U||
       audit.accepted_roots!=1U||audit.accepted_children!=2U||
       audit.linked!=2U||audit.unsupported_shapes!=0U||
       parents.size()!=1U||parents[0]!=90U)return 9;

    // Full High 12 uses ProfileCurve[1], PathScaleX[6] and PathScaleY[7].
    // Do not accidentally classify with compressed High 13 byte offsets.
    auto changed_profile=box;
    changed_profile[110]=2U;
    if(!objectspatial::decode_full_transforms(
        packet({changed_profile}),out,&audit)||out.size()!=1U||
        out[0].primitive!=objectspatial::PrimitiveKind::Unknown)return 13;
    auto changed_scale=box;
    changed_scale[115]=90U;
    if(!objectspatial::decode_full_transforms(
        packet({changed_scale}),out,&audit)||out.size()!=1U||
        out[0].primitive!=objectspatial::PrimitiveKind::Unknown)return 14;
    box[121]=20U; // PathRadiusOffset: complex modified shape
    if(!objectspatial::decode_full_transforms(
           packet({box}),out,&audit,&parents,&linked)||
       out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Unknown||
       audit.unsupported_shapes!=1U)return 10;
    // Mesh id comes from a real 0x30 sculpt ExtraParam, not a fake prim.
    Bytes sculpt{1U,0x30U,0U,17U,0U,0U,0U};
    for(unsigned i=0U;i<16U;++i)sculpt.push_back(
        static_cast<Byte>(i+1U));
    sculpt.push_back(5U);
    auto mesh_block=block(94U,0U,9U,128.f,sculpt);
    mesh_block[109]=0x10U;mesh_block[110]=0x01U;
    mesh_block[115]=100U;mesh_block[116]=100U;
    if(!objectspatial::decode_full_transforms(
         packet({mesh_block}),out,&audit)||out.size()!=1U||
       out[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10"||
       out[0].primitive!=objectspatial::PrimitiveKind::Unknown||
       audit.mesh_references!=1U||audit.extra_records!=1U||
       audit.sculpt_records!=1U||audit.mesh_sculpt_records!=1U)return 12;
    auto bot=packet({block(81U,0,47U)});
    std::vector<std::uint32_t> avatar_roots;
    if(!objectspatial::decode_full_transforms(
         bot,out,&audit,&parents,nullptr,&avatar_roots)||
       !out.empty()||!parents.empty()||audit.non_volume!=1U||
       audit.accepted_roots!=0U||avatar_roots.size()!=1U||
       avatar_roots[0]!=81U)return 3;
    auto badpos=packet({block(82U,0,9U,800.f)});
    if(!objectspatial::decode_full_transforms(badpos,out,&audit)||
       !out.empty()||audit.invalid_transform!=1U||
       audit.volume!=1U)return 4;
    auto quantized=block(84U);
    // 32-byte motion variant: currently unsupported, but must be
    // reported honestly rather than silently counted as a real object.
    quantized[40]=32U;
    quantized.erase(quantized.begin()+73,quantized.begin()+101);
    if(!objectspatial::decode_full_transforms(
           packet({quantized}),out,&audit)||
       !out.empty()||audit.unsupported_motion!=1U||
       audit.accepted_roots!=0U)return 8;
    auto invalid=live;
    invalid.body.pop_back();
    if(objectspatial::decode_full_transforms(invalid,out))return 5;
    invalid=live;
    invalid.body[52]=255U; // 1-byte motion payload length no longer fits
    if(objectspatial::decode_full_transforms(invalid,out))return 6;
    invalid=packet({block(83U)});
    invalid.body[0]=13U; // Different wire family, reject entirely
    if(objectspatial::decode_full_transforms(invalid,out))return 7;
    // A real High 13 packed box layout cannot masquerade as a High 12 box.
    auto wrong_layout=block(100U);
    wrong_layout[109U]=0x10U;
    wrong_layout[125U]=0x01U; // High 12 PathSkew, NOT ProfileCurve
    wrong_layout[114U]=100U;wrong_layout[115U]=100U;
    if(!objectspatial::decode_full_transforms(
           packet({wrong_layout}),out,&audit)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Unknown)return 15;
    // Distinguish genuine light/flexible/projector records from sculpt meshes.
    Bytes light{1U,0x20U,0U,16U,0U,0U,0U};
    light.insert(light.end(),16U,0U);
    if(!objectspatial::decode_full_transforms(
           packet({block(101U,0U,9U,128.f,light)}),out,&audit)||
       audit.extra_records!=1U||audit.light_records!=1U||
       audit.sculpt_records!=0U||audit.mesh_references!=0U||
       audit.extra_envelopes!=1U||audit.malformed_extra_envelopes!=0U)
        return 16;
    Bytes flex{1U,0x10U,0U,16U,0U,0U,0U};
    flex.insert(flex.end(),16U,0U);
    if(!objectspatial::decode_full_transforms(
           packet({block(102U,0U,9U,128.f,flex)}),out,&audit)||
       audit.flexible_records!=1U||audit.mesh_sculpt_records!=0U)return 17;
    Bytes project{1U,0x40U,0U,16U,0U,0U,0U};
    project.insert(project.end(),16U,0U);
    if(!objectspatial::decode_full_transforms(
           packet({block(103U,0U,9U,128.f,project)}),out,&audit)||
       audit.projector_records!=1U||audit.mesh_references!=0U)return 18;
    Bytes unknown{1U,0x99U,0U,1U,0U,0U,0U,0U};
    if(!objectspatial::decode_full_transforms(
           packet({block(104U,0U,9U,128.f,unknown)}),out,&audit)||
       audit.other_extra_records!=1U||audit.mesh_references!=0U)return 19;
    light[3U]=200U;
    if(!objectspatial::decode_full_transforms(
           packet({block(105U,0U,9U,128.f,light)}),out,&audit)||
       audit.malformed_extra_envelopes!=1U||audit.extra_records!=0U||
       audit.mesh_references!=0U)return 20;
    // A malformed sculpt/ExtraParams envelope must never render as
    // a fabricated simple box even if its path/profile looks like one.
    auto malformed_box=block(110U,0U,9U,128.f,light);
    malformed_box[109]=0x10U;malformed_box[110]=0x01U;
    malformed_box[115]=100U;malformed_box[116]=100U;
    if(!objectspatial::decode_full_transforms(
           packet({malformed_box}),out,&audit)||out.size()!=1U||
       out[0].primitive!=objectspatial::PrimitiveKind::Unknown||
       audit.malformed_extra_envelopes!=1U)return 21;
    // Official legacy PARAMS_MESH (0x60) is a sculpt param alias.
    // Regression covers the exact live full-update mesh discovery path.
    auto alias=sculpt;
    alias[1]=0x60U;
    if(!objectspatial::decode_full_transforms(
           packet({block(111U,0U,9U,128.f,alias)}),out,&audit)||
       out.size()!=1U||
       out[0].mesh_asset_id!="01020304-0506-0708-090a-0b0c0d0e0f10"||
       audit.legacy_mesh_alias_records!=1U||
       audit.mesh_sculpt_records!=1U||
       audit.other_extra_records!=0U)return 22;
    alias.back()=3U;
    if(!objectspatial::decode_full_transforms(
           packet({block(112U,0U,9U,128.f,alias)}),out,&audit)||
       out.size()!=1U||!out[0].mesh_asset_id.empty()||
       audit.legacy_mesh_alias_records!=1U||
       audit.mesh_sculpt_records!=0U)return 23;
    return 0;
}
