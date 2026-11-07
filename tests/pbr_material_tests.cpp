#include "kami/protocol/pbr_material.hpp"
#include "kami/world/pbr_material_table.hpp"
#include "kami/world/live_primitive_geometry.hpp"
#include <cstdio>
#include <algorithm>
#include "kami/graphics/settings.hpp"
#include <cstring>
#include "kami/world/environment.hpp"
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
    // SL mesh/primitive TextureEntry supports 45 faces (0..44), not 32.
    // The PBR face-ID bridge must retain the final official face slot.
    std::vector<std::uint8_t> high_face{1U,0x80U,0U,18U,0U,0U,0U,1U,44U};
    for(unsigned i=0U;i<16U;++i)high_face.push_back(
        static_cast<std::uint8_t>(i+1U));
    std::array<std::string,45U> high_face_ids{};
    if(!decode_face_ids(high_face,high_face_ids)||
       high_face_ids[44U]!="01020304-0506-0708-090a-0b0c0d0e0f10")
        return 47;
    const auto bare=asset(json);
    const std::string official_header="<? LLSD/Binary ?>\n";
    auto canonical=bare;canonical.insert(canonical.begin(),official_header.begin(),official_header.end());
    Material from_official;
    if(!decode_asset(canonical,from_official)||from_official!=m)return 25;
    DecodeError decode_error=DecodeError::GLTF;
    for(const std::string header:{"<? LLSD/Binary ?>\r\n","<?llsd/binary?>\n","  <? lLsD/bInArY ?>\n","<? LLSD/Binary ?>"}){
        auto bytes=bare;bytes.insert(bytes.begin(),header.begin(),header.end());bytes.push_back('\n');
        if(!decode_asset(bytes,from_official,&decode_error)||decode_error!=DecodeError::None||from_official!=m)return 26;
    }
    const auto check_bad=[&](std::vector<std::uint8_t> bytes,DecodeError expected){
        auto unchanged=m;auto error=DecodeError::None;
        return !decode_asset(bytes,unchanged,&error)&&error==expected&&unchanged==m;
    };
    for(const auto& [header,error]:std::map<std::string,DecodeError>{
        {"<? LLSD/XML ?>\n",DecodeError::XML},{"<? LLSD/Notation ?>\n",DecodeError::Notation},
        {"<? arbitrary ?>\n",DecodeError::Header},{"<? LLSD/Binary ",DecodeError::Header}}){
        auto bytes=bare;bytes.insert(bytes.begin(),header.begin(),header.end());
        if(!check_bad(bytes,error))return 27;
    }
    auto trailing=canonical;trailing.push_back('x');
    if(!check_bad(trailing,DecodeError::Trailing)||!check_bad({},DecodeError::Size))return 28;
    // R65: a complete binary LLSD map may have a bounded NUL
    // terminator, but arbitrary junk, excessive padding or an absent
    // closing map marker must remain rejected.
    for(unsigned nul_count=1U;nul_count<=4U;++nul_count){
        auto nul_terminated=canonical;
        nul_terminated.insert(nul_terminated.end(),nul_count,0U);
        Material decoded;auto result_error=DecodeError::Size;
        if(!decode_asset(nul_terminated,decoded,&result_error)||
           decoded!=m||result_error!=DecodeError::None)return 47;
    }
    auto excess_padding=canonical;
    excess_padding.insert(excess_padding.end(),5U,0U);
    if(!check_bad(excess_padding,DecodeError::Trailing))return 48;
    auto mixed_padding=canonical;
    mixed_padding.push_back(0U);mixed_padding.push_back('x');
    if(!check_bad(mixed_padding,DecodeError::Trailing))return 49;
    auto no_close=canonical;
    no_close[no_close.size()-1U]=0U;
    if(!check_bad(no_close,DecodeError::Trailing))return 50;
    auto version_bad=asset(json);const std::string version="1.1";
    const auto where=std::search(version_bad.begin(),version_bad.end(),version.begin(),version.end());
    if(where==version_bad.end())return 29;*where='2';
    if(!check_bad(version_bad,DecodeError::Version))return 30;
    auto type_bad=asset(json);const std::string type="GLTF 2.0";
    const auto type_at=std::search(type_bad.begin(),type_bad.end(),type.begin(),type.end());
    if(type_at==type_bad.end())return 31;*type_at='x';
    if(!check_bad(type_bad,DecodeError::Type)||!check_bad(asset("not json"),DecodeError::GLTF))return 32;
    const auto escape=[](const std::string& value){
        std::string out;for(const char c:value){
            if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='>')out+="&gt;";
            else if(c=='"')out+="&quot;";else out+=c;
        }return out;
    };
    const auto bytes=[](const std::string& text){return std::vector<std::uint8_t>(text.begin(),text.end());};
    const std::string xml="<llsd><map><key>version</key><string>1.1</string><key>type</key><string>GLTF 2.0</string><key>data</key><string>"+escape(json)+"</string></map></llsd>";
    const std::string cdata="<llsd><map><key>version</key><string>1.1</string><key>type</key><string>GLTF 2.0</string><key>data</key><string><![CDATA["+json+"]]></string></map></llsd>";
    const std::string notation="{'version':'1.1','type':'GLTF 2.0','data':s("+std::to_string(json.size())+")'"+json+"'}";
    for(const auto& input:std::vector<std::string>{xml,"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"+xml,
        "<? LLSD/XML ?>\n"+xml,cdata,"\xef\xbb\xbf"+xml,notation,"<? lLsD/NoTaTiOn ?>\n"+notation}){
        Material decoded;decode_error=DecodeError::Envelope;
        if(!decode_asset(bytes(input),decoded,&decode_error)||decoded!=m||decode_error!=DecodeError::None)return 33;
    }
    const std::string json_envelope="{\"version\":\"1.1\",\"type\":\"GLTF 2.0\",\"data\":\"";
    // JSON quoted strings are a valid subset of notation; escaping the
    // nested glTF text must preserve its own JSON escape sequences.
    std::string escaped;for(char c:json){if(c=='\\'||c=='"')escaped+='\\';escaped+=c;}
    if(!decode_asset(bytes(json_envelope+escaped+"\"}"),from_official)||from_official!=m)return 34;
    if(asset_format(canonical)!=AssetFormat::Binary||asset_format(bytes(xml))!=AssetFormat::XML||
       asset_format(bytes(notation))!=AssetFormat::Notation||asset_format(std::vector<std::uint8_t>{0x1f,0x8b})!=AssetFormat::Gzip)return 35;
    for(const auto& input:std::vector<std::string>{xml+"junk",xml+xml,"<llsd><array/></llsd>",
        "<!DOCTYPE llsd SYSTEM 'https://example.invalid/never-fetch'>"+xml,
        "<llsd><map><key>data</key><string><map/></string></map></llsd>",
        "<llsd><map><key>version</key><string>1.1</string><key>version</key><string>1.0</string></map></llsd>"})
        if(!check_bad(bytes(input),DecodeError::XML))return 36;
    if(!check_bad(bytes("{'version':s(99)'1.1'}"),DecodeError::Notation)||
       !check_bad(bytes("{'version':'1.1','version':'1.1'}"),DecodeError::Notation)||
       !check_bad(std::vector<std::uint8_t>{0x1f,0x8b,0,0},DecodeError::Compressed))return 37;
    auto wrong_value=bare;const auto value_tag=std::find(wrong_value.begin()+10U,wrong_value.end(),std::uint8_t('s'));
    if(value_tag==wrong_value.end())return 38;*value_tag='b';
    if(!check_bad(wrong_value,DecodeError::Value))return 39;
    const auto prior=m;auto truncated=asset(json);truncated.pop_back();
    if(decode_asset(truncated,m)||m!=prior)return 2;
    auto remote=json;remote.replace(remote.find(image),image.size(),"https://example.com/image.png");
    if(decode_json(remote,m)||m!=prior)return 3;
    if(decode_json(R"({"asset":{"version":"2.0"},"materials":[{"pbrMetallicRoughness":{"roughnessFactor":-1}}]})",m))return 4;
    if(!decode_json(R"({"asset":{"version":"2.0"},"materials":[{}]})",m)||m.metallic!=1.f||m.roughness!=1.f||m.base[3]!=1.f)return 5;
    std::array<std::string,45> faces;
    std::vector<std::uint8_t> extras{1U,0x80U,0U,18U,0U,0U,0U,1U,2U};
    extras.insert(extras.end(),{0x12,0x34,0x56,0x78,0x90,0xab,0x4c,0xde,0x8f,0xab,0,0,0,0,0,2});
    if(!decode_face_ids(extras,faces)||faces[2]!=id||!faces[0].empty())return 6;
    auto malformed=extras;malformed[8]=45U;const auto old_faces=faces;
    if(decode_face_ids(malformed,faces)||faces!=old_faces)return 7;
    if(!decode_face_ids(std::vector<std::uint8_t>{1,0x80,0,1,0,0,0,0},faces)||faces!=std::array<std::string,45>{})return 8;
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
    // Terrain material IDs occupy four independent virtual faces.
    const auto land=world::build_pbr_table({}, {}, materials, {}, {{image,1U}}, {id,id,id,id});
    if(land.faces.size()!=4U||!land.faces.contains(world::pbr_face_key(0U,3U)))return 19;
    const auto land_images=world::select_visible_mesh_texture_layers({}, {}, {}, {}, {{image,bitmap}},0,0,0,1U,{}, {}, {},{image});
    if(!land_images.contains(image))return 20;
    // R63: >1,023 distinct PBR records must not let low local-ID,
    // distant roots crowd out a nearby face or the terrain material.
    {
        Materials crowded_materials;
        std::map<std::uint32_t,protocol::objectspatial::Transform> crowded_roots;
        const std::string terrain_id="r63-terrain";
        auto terrain_material=prior;
        terrain_material.base[0]=0.991f;
        crowded_materials.emplace(terrain_id,terrain_material);
        for(std::uint32_t i=0U;i<1040U;++i){
            const std::string material_id="r63-material-"+std::to_string(i);
            auto material=prior;
            material.base[0]=float(i+1U)/2048.f;
            crowded_materials.emplace(material_id,material);
            auto root=prim;
            root.local_id=10000U+i;
            root.x=i==1039U?0.f:80.f;
            root.y=0.f;
            root.z=0.f;
            root.face_material_ids={};
            root.face_material_ids[0]=material_id;
            crowded_roots.emplace(root.local_id,root);
        }
        const auto first=world::build_pbr_table(
            crowded_roots,{},crowded_materials,{}, {},{terrain_id,"","",""});
        if(first.faces.contains(world::pbr_face_key(11039U,0U)))return 44;
        const auto prioritized=world::build_pbr_table(
            crowded_roots,{},crowded_materials,{}, {},
            {terrain_id,"","",""},std::array<float,3>{0.f,0.f,0.f});
        if(!prioritized.faces.contains(world::pbr_face_key(11039U,0U))||
           !prioritized.faces.contains(world::pbr_face_key(0U,0U))||
           prioritized.rows.size()>std::size_t(kMaxGPURecords)*kRows)return 45;
        // Unresolved linked children must not consume PBR capacity.
        auto orphan=prim;
        orphan.parent_id=999999U;
        orphan.local_id=20001U;
        orphan.face_material_ids={};
        orphan.face_material_ids[0]="r63-material-0";
        const auto unresolved=world::build_pbr_table({},{{orphan.local_id,orphan}},
            crowded_materials,{}, {},{},std::array<float,3>{0.f,0.f,0.f});
        if(unresolved.faces.contains(world::pbr_face_key(orphan.local_id,0U)))return 46;
    }
    std::vector<std::uint8_t> probe{1,0x90,0,9,0,0,0,0,0,0,0,0,0,0,0,1};
    bool found=false;float ambiance=-1,clip=-1;std::uint8_t flags=0;
    if(!decode_probe(probe,found,ambiance,clip,flags)||!found||ambiance!=0||clip!=0||flags!=1)return 21;
    probe.pop_back();if(decode_probe(probe,found,ambiance,clip,flags))return 22;
    auto volume=prim;volume.reflection_probe=true;volume.probe_flags=1;volume.sx=volume.sy=volume.sz=20;
    const auto selected=world::select_environment_probe({{77U,volume}},{128,128,35},{0,0,0});
    if(!selected.server||!selected.box||selected.center!=std::array<float,3>{0,0,0})return 23;
    const auto fallback=world::select_environment_probe({{77U,volume}},{0,0,0},{1000,0,0});
    if(fallback.server||fallback.center[0]!=1000)return 24;
    graphics::Settings settings;settings.preset=graphics::Preset::Quality;
    if(settings.mesh_triangles()!=1300000U)return 40;
    settings.preset=graphics::Preset::Balanced;if(settings.mesh_triangles()!=400000U)return 41;
    settings.preset=graphics::Preset::Performance;if(settings.mesh_triangles()!=80000U)return 42;
    settings.preset=graphics::Preset::Potato;if(settings.mesh_triangles()!=24000U)return 43;
    return 0;
}
}
int main(){const auto result=run();if(result)std::fprintf(stderr,"PBR regression %d failed\n",result);return result;}
