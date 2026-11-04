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
    return 0;
}
