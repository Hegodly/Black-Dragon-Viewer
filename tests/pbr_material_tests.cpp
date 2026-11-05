#include "kami/protocol/pbr_material.hpp"
#include "kami/world/pbr_material_table.hpp"
#include "kami/world/live_primitive_geometry.hpp"
#include <cstdio>
using namespace kami;
namespace {
void be(std::vector<std::uint8_t>& b,std::uint32_t n){for(int s=24;s>=0;s-=8)b.push_back((n>>s)&255U);}
void text(std::vector<std::uint8_t>& b,const std::string& s){be(b,static_cast<std::uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end());}
std::vector<std::uint8_t> asset(const std::string& json){
    std::vector<std::uint8_t> b{'{',0,0,0,3};
    for(const auto& [key,value]:std::map<std::string,std::string>{{"version","1.1"},{"type","GLTF 2.0"},{"data",json}}){
        b.push_back('k');text(b,key);b.push_back('s');text(b,value);
    }b.push_back('}');return b;
}
std::vector<std::uint8_t> packet(const std::string& notation){
    std::vector<std::uint8_t> b{31U,0x75U,0x41U,static_cast<std::uint8_t>(notation.size()),
        static_cast<std::uint8_t>(notation.size()>>8U)};b.insert(b.end(),notation.begin(),notation.end());return b;
}
int run(){
    using namespace protocol::pbr;
    const std::string image="12345678-90ab-4cde-8fab-000000000001",id="12345678-90ab-4cde-8fab-000000000002";
    const std::string json=R"({"asset":{"version":"2.0"},"images":[{"uri":")"+image+R"("}],"textures":[{"source":0}],"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.8,0.3],"metallicFactor":0.7,"roughnessFactor":0.2,"baseColorTexture":{"index":0,"extensions":{"KHR_texture_transform":{"scale":[2,3],"offset":[0.1,0.2],"rotation":0.3}}},"metallicRoughnessTexture":{"index":0}},"normalTexture":{"index":0,"scale":0.6},"occlusionTexture":{"index":0,"strength":0.4},"emissiveTexture":{"index":0},"emissiveFactor":[0.1,0.2,0.3],"alphaMode":"MASK","alphaCutoff":0.25,"doubleSided":true}]})";
    Material m;
    if(!decode_asset(asset(json),m)||m.images[0]!=image||m.images[1]!=image||
       m.images[2]!=image||m.images[3]!=image||m.images[4]!=image||m.base[2]!=.8f||
       m.metallic!=.7f||m.roughness!=.2f||m.alpha!=1U||m.cutoff!=.25f||
       !m.double_sided||m.normal_scale!=.6f||m.occlusion_strength!=.4f||m.uv[0].scale[1]!=3.f)return 1;
    const auto prior=m;auto truncated=asset(json);truncated.pop_back();
    if(decode_asset(truncated,m)||m!=prior)return 2;
    auto remote=json;remote.replace(remote.find(image),image.size(),"https://example.com/image.png");
    if(decode_json(remote,m)||m!=prior)return 3;
    if(decode_json(R"({"asset":{"version":"2.0"},"materials":[{"pbrMetallicRoughness":{"roughnessFactor":-1}}]})",m))return 4;
    if(!decode_json(R"({"asset":{"version":"2.0"},"materials":[{}]})",m)||m.metallic!=1.f||m.roughness!=1.f||m.base[3]!=1.f)return 5;
    std::array<std::string,32> faces;
    std::vector<std::uint8_t> extras{1U,0x80U,0U,18U,0U,0U,0U,1U,2U};
    extras.insert(extras.end(),{0x12,0x34,0x56,0x78,0x90,0xab,0x4c,0xde,0x8f,0xab,0,0,0,0,0,2});
    if(!decode_face_ids(extras,faces)||faces[2]!=id||!faces[0].empty())return 6;
    auto malformed=extras;malformed[8]=32U;const auto old_faces=faces;
    if(decode_face_ids(malformed,faces)||faces!=old_faces)return 7;
    if(!decode_face_ids(std::vector<std::uint8_t>{1,0x80,0,1,0,0,0,0},faces)||faces!=std::array<std::string,32>{})return 8;
    std::uint32_t object=0U;std::map<unsigned,Override> overrides;
    auto wire=packet("{'id':i77,'te':[i2],'od':[{'bc':[r1,r1,r1,r1],'mf':r0,'rf':r1,'am':i2,'ds':false,'tex':[uffffffff-ffff-ffff-ffff-ffffffffffff],'ti':[{'o':[r0.5,r0.25],'s':[r1,r2],'r':r0.5}]}]}");
    if(!decode_override(wire,object,overrides)||object!=77U||overrides.size()!=1U)return 9;
    Materials materials{{id,prior}};Overrides live{{77U,overrides}};Material applied;
    if(!effective(id,77U,2U,materials,live,applied)||applied.metallic!=0.f||
       applied.roughness!=1.f||applied.alpha!=2U||applied.double_sided||
       !applied.images[0].empty()||applied.base[3]!=1.f||applied.uv[0].scale[1]!=2.f)return 10;
    if(!effective(id,78U,2U,materials,live,applied)||applied.base!=prior.base)return 11;
    const auto retained=overrides;wire.pop_back();
    if(decode_override(wire,object,overrides)||overrides.size()!=retained.size())return 12;
    if(!decode_override(packet("{'id':i77,'te':[],'od':[]}"),object,overrides)||!overrides.empty())return 13;
    protocol::objectspatial::Transform prim{77U,128.f,128.f,35.f,1.f,1.f,1.f};
    prim.primitive=protocol::objectspatial::PrimitiveKind::Box;prim.face_material_ids[2]=id;
    const std::map<std::uint32_t,protocol::objectspatial::Transform> roots{{77U,prim}};
    const auto table=world::build_pbr_table(roots,{},materials,{},{{image,192U}});
    const auto key=world::pbr_face_key(77U,2U);
    if(table.faces.size()!=1U||table.rows.size()!=kRows*2U||table.rows[kRows+3U][0]!=192.f)return 14;
    const auto geometry=world::build_live_primitive_geometry(roots,{},128.f,128.f,35.f,128U,{}, {},table.faces);
    if(geometry.scene.vertices.size()!=36U||geometry.scene.vertices[12].pbr_material!=1U||
       geometry.scene.vertices[0].pbr_material!=0U)return 15;
    protocol::texture::DecodedTexture bitmap;bitmap.width=bitmap.height=1U;bitmap.rgba={255,255,255,255};
    const auto layers=world::select_visible_mesh_texture_layers(roots,{}, {},{},{{image,bitmap}},128.f,128.f,35.f,192U,{},materials,{});
    if(!layers.contains(image))return 16;
    // SL emits undefined placeholders for unchanged earlier texture slots.
    // LLSD Integer is signed; local object IDs retain their U32 bit pattern.
    const auto sparse=packet("{'id':i-1,'te':[i2],'od':[{'tex':[!,u12345678-90ab-4cde-8fab-000000000001],'ti':[!,{'s':[r2,r3]}]}]}");
    if(!decode_override(sparse,object,overrides)||object!=0xffffffffU)return 17;
    Overrides sparse_live{{object,overrides}};
    if(!effective(id,object,2U,materials,sparse_live,applied)||
       applied.images[0]!=prior.images[0]||applied.images[1]!=image||
       applied.uv[1].scale[1]!=3.f)return 18;
    return 0;
}
}
int main(){const auto result=run();if(result)std::fprintf(stderr,"PBR regression %d failed\n",result);return result;}
