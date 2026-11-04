#include "kami/protocol/simulator_agent_update.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

using namespace kami::protocol;
namespace {
LoginResult login(){
    LoginResult r{};
    r.status=LoginStatus::Accepted;
    r.handoff.agent_id="12345678-90ab-4cde-8fab-000000000001";
    r.handoff.session_id="12345678-90ab-4cde-8fab-000000000002";
    r.handoff.simulator_host="sim1234.agni.lindenlab.com";
    r.handoff.simulator_port=13000;
    r.handoff.seed_capability=
        L"https://sim1234.agni.lindenlab.com:12345/cap/abc";
    r.circuit_code=0x12345678U;
    return r;
}
float read_float(const std::vector<std::uint8_t>& b,std::size_t p){
    float v=0;
    std::memcpy(&v,b.data()+p,4);
    return v;
}
}
int main(){
    auto auth=login();
    lludp::Packet movement{};
    movement.body={0xff,0xff,0x00,0xfa};
    std::array<std::uint8_t,16> agent{},session{};
    if(!sim::parse_uuid(auth.handoff.agent_id,agent)||
       !sim::parse_uuid(auth.handoff.session_id,session))return 1;
    movement.body.insert(movement.body.end(),agent.begin(),agent.end());
    movement.body.insert(movement.body.end(),session.begin(),session.end());
    movement.body.insert(movement.body.end(),44,0U);
    const std::array<float,3> position={120.f,144.f,30.f},at={1.f,0.f,0.f};
    for(unsigned i=0;i<3;++i){
        std::memcpy(movement.body.data()+36+i*4,&position[i],4);
        std::memcpy(movement.body.data()+48+i*4,&at[i],4);
    }
    sim::SimulatorPose pose{};
    if(!sim::decode_movement_pose(movement,auth,pose)||
       !pose.verified||pose.position!=position||
       std::abs(pose.look_at[0]-1.f)>0.001f)return 2;
    lludp::Packet packet{};
    if(!sim::build_agent_update(auth,pose,52,packet)||
       packet.sequence!=52||packet.flags!=lludp::kZerocoded||
       packet.body.size()!=115||packet.body[0]!=4)return 3;
    if(read_float(packet.body,58)!=117.5f||
       read_float(packet.body,62)!=144.f||
       read_float(packet.body,66)!=31.65f||
       read_float(packet.body,106)!=64.f)return 4;
    std::vector<std::uint8_t> bytes{};
    lludp::Packet decoded{};
    if(!lludp::encode(packet,bytes)||!lludp::decode(bytes,decoded)||
       decoded.body!=packet.body)return 5;
    auto rejected=auth;
    rejected.status=LoginStatus::Rejected;
    if(sim::build_agent_update(rejected,pose,55,packet))return 6;
    pose.verified=false;
    if(sim::build_agent_update(auth,pose,56,packet))return 7;
    pose.verified=true;
    movement.body[5]^=1U;
    sim::SimulatorPose spoofed{};
    if(sim::decode_movement_pose(movement,auth,spoofed))return 8;
    movement.body[5]^=1U;
    const float invalid=std::numeric_limits<float>::infinity();
    std::memcpy(movement.body.data()+36,&invalid,4);
    if(sim::decode_movement_pose(movement,auth,spoofed))return 9;
    return 0;
}
