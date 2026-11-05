#include "kami/protocol/object_spatial.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
using Bytes=std::vector<std::uint8_t>;
void u16(Bytes& b,std::uint16_t v){
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v>>8));
}
void u32(Bytes& b,std::uint32_t v){
    for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));
}
void f32(Bytes& b,float v){
    std::uint32_t u=0;std::memcpy(&u,&v,sizeof(u));u32(b,u);
}
Bytes data(std::uint32_t id,bool avatar,float x,float y,float z) {
    Bytes result;
    u32(result,id);result.push_back(0U);result.push_back(avatar?1U:0U);
    if(avatar)result.insert(result.end(),16U,0U);
    f32(result,x);f32(result,y);f32(result,z);
    for(int i=0;i<6;++i)u16(result,32768U); // velocity, acceleration
    for(int i=0;i<3;++i)u16(result,32768U);
    u16(result,65535U); // identity-like quantized quaternion w=1
    for(int i=0;i<3;++i)u16(result,32768U); // angular velocity
    return result;
}
kami::protocol::lludp::Packet message(const std::vector<Bytes>& blocks){
    kami::protocol::lludp::Packet p;
    p.body.push_back(15U); // ImprovedTerseObjectUpdate High 15
    p.body.insert(p.body.end(),10U,0U);
    p.body.push_back(static_cast<std::uint8_t>(blocks.size()));
    for(const auto& block:blocks){
        p.body.push_back(static_cast<std::uint8_t>(block.size()));
        p.body.insert(p.body.end(),block.begin(),block.end());
        u16(p.body,0U); // TextureEntry Variable 2 absent
    }
    return p;
}
int main(){
    using namespace kami::protocol::objectspatial;
    std::vector<TersePose> poses;
    auto a=data(41U,false,10.f,40.f,4.f);
    if(a.size()!=44U)return 1;
    auto b=data(42U,true,5.f,30.f,2.f);
    if(b.size()!=60U)return 2;
    const auto packet=message({a,b});
    if(!decode_terse_poses(packet,poses)||poses.size()!=2U||
       poses[0].local_id!=41U||poses[1].local_id!=42U||
       std::abs(poses[0].x-10.f)>.0001f||
       std::abs(poses[0].y-40.f)>.0001f||
       std::abs(poses[1].x-5.f)>.0001f||
       std::abs(poses[0].rw-1.f)>.001f)return 3;
    Transform existing{};
    existing.local_id=41U;
    existing.x=1.f;existing.y=1.f;existing.z=1.f;
    existing.sx=2.f;existing.sy=3.f;existing.sz=4.f;
    existing.mesh_asset_id="01234567-89ab-4cde-8fab-112233445566";
    existing.parent_id=73U;
    existing.volume_params_seen=true;
    existing.primitive=PrimitiveKind::Box;
    if(!apply_terse_pose(existing,poses[0])||existing.x!=10.f||
       existing.parent_id!=73U||existing.sx!=2.f||
       existing.primitive!=PrimitiveKind::Box||
       existing.mesh_asset_id!="01234567-89ab-4cde-8fab-112233445566")
       return 4;
    if(apply_terse_pose(existing,poses[0]))return 5;
    if(apply_terse_pose(existing,poses[1]))return 6;
    auto broken=packet;
    broken.body.pop_back(); // truncated TextureEntry
    const auto previous=poses.size();
    if(decode_terse_poses(broken,poses)||poses.size()!=previous)return 7;
    auto unsupported=a;unsupported.pop_back();
    if(!decode_terse_poses(message({unsupported}),poses)||
       !poses.empty())return 8;
    auto bad=a;bad[5]=1U; // flag implies missing collision plane
    if(!decode_terse_poses(message({bad}),poses)||!poses.empty())return 9;
    bad=a; // NaN region position
    bad[6]=0U;bad[7]=0U;bad[8]=0xc0U;bad[9]=0x7fU;
    if(!decode_terse_poses(message({bad}),poses)||!poses.empty())return 10;
    bad=a;u32(bad,0U); // extra trailing bytes invalid recognized format
    if(!decode_terse_poses(message({bad}),poses)||!poses.empty())return 11;
    auto duplicated=message({a,a});
    if(!decode_terse_poses(duplicated,poses)||poses.size()!=2U)return 12;
    return 0;
}
