#include "kami/protocol/simulator_wire.hpp"
#include <array>
#include <cstring>
#include <limits>
using namespace kami::protocol;
namespace{
LoginResult good(){
    LoginResult r{};
    r.status=LoginStatus::Accepted;
    r.handoff.agent_id="12345678-90ab-4cde-8fab-000000000001";
    r.handoff.session_id="12345678-90ab-4cde-8fab-000000000002";
    r.handoff.simulator_host="sim1234.agni.lindenlab.com";
    r.handoff.simulator_port=13000;
    r.handoff.seed_capability=L"https://sim1234.agni.lindenlab.com:12345/cap/abc";
    r.circuit_code=0x12345678U;
    return r;
}
}
int main(){
    const auto login=good();
    lludp::Packet packet{};
    if(!sim::use_circuit_code(login,1,packet)||packet.flags!=lludp::kReliable||
       packet.body.size()!=40U||packet.body[0]!=0xff||packet.body[3]!=3||
       packet.body[4]!=0x78||packet.body[7]!=0x12||
       packet.body[8]!=0x12||packet.body[9]!=0x34||
       packet.body[24]!=0x12||packet.body[25]!=0x34)return 1;
    std::vector<std::uint8_t> wire{};
    lludp::Packet decoded{};
    if(!lludp::encode(packet,wire)||!lludp::decode(wire,decoded)||
       decoded.body!=packet.body)return 2;
    if(!sim::complete_agent_movement(login,2,packet)||
       packet.body.size()!=40U||packet.body[3]!=0xf9||
       packet.body[4]!=0x12||packet.body[20]!=0x12||
       packet.body[36]!=0x78)return 3;
    if(!sim::region_handshake_reply(login,3,packet)||
       packet.body.size()!=40U||packet.body[3]!=0x95||
       packet.body[36]!=0U)return 4;
    const std::array<std::uint32_t,2> acks={0x12345678U,10U};
    if(!sim::packet_ack(acks,4,packet)||
       packet.body.size()!=13U||packet.body[4]!=2||
       packet.body[5]!=0x78||packet.body[8]!=0x12||
       sim::classify(packet)!=sim::Inbound::PacketAck)return 5;
    if(!sim::complete_ping(42,5,packet)||packet.body[0]!=2||
       packet.body[1]!=42)return 6;
    std::array<std::uint8_t,16> id{};
    if(!sim::parse_uuid(login.handoff.agent_id,id)||
       id[0]!=0x12||id[1]!=0x34||id[15]!=1||
       sim::parse_uuid("bad",id))return 7;
    auto rejected=good();rejected.status=LoginStatus::Rejected;
    if(sim::use_circuit_code(rejected,6,packet)||
       sim::complete_agent_movement(rejected,7,packet)||
       sim::region_handshake_reply(rejected,8,packet))return 8;
    auto movement=lludp::Packet{};
    movement.body={0xff,0xff,0x00,0xfa};
    std::array<std::uint8_t,16> agent{},session{};
    sim::parse_uuid(login.handoff.agent_id,agent);
    sim::parse_uuid(login.handoff.session_id,session);
    movement.body.insert(movement.body.end(),agent.begin(),agent.end());
    movement.body.insert(movement.body.end(),session.begin(),session.end());
    movement.body.insert(movement.body.end(),36U,0);
    if(!sim::movement_complete_identifies(movement,login))return 9;
    movement.body[5]^=1;
    if(sim::movement_complete_identifies(movement,login))return 10;
    if(sim::classify(movement)!=sim::Inbound::AgentMovementComplete)return 11;
    // Restore the synthetic agent ID before positive position decoding.
    movement.body[5]^=1;
    const std::array<float,3> position={128.f,96.f,22.5f};
    std::memcpy(movement.body.data()+36,position.data(),
                sizeof(float)*position.size());
    sim::AgentPosition agent_position{};
    if(!sim::decode_agent_movement_position(movement,login,agent_position)||
       !agent_position.verified||agent_position.x!=128.f||
       agent_position.y!=96.f||agent_position.z!=22.5f)return 16;
    const float impossible=std::numeric_limits<float>::infinity();
    std::memcpy(movement.body.data()+44,&impossible,sizeof(float));
    if(sim::decode_agent_movement_position(movement,login,agent_position)||
       agent_position.verified)return 17;
    lludp::Packet region{};
    region.flags=lludp::kReliable;
    region.body.assign(215U,0U);
    region.body[0]=0xff;region.body[1]=0xff;
    region.body[3]=0x94;region.body[9]=4;
    region.body[10]='T';region.body[11]='e';
    region.body[12]='s';region.body[13]='t';
    // WaterHeight follows variable-length SimName, SimOwner and manager bit.
    constexpr float water=31.25f;
    std::memcpy(region.body.data()+31,&water,sizeof(float));
    sim::RegionMetadata snapshot{};
    if(!sim::decode_region_metadata(region,snapshot)||
       !snapshot.verified||snapshot.name!="Test"||
       snapshot.water_height!=water)return 12;
    region.body[12]=0x0a;
    if(sim::decode_region_metadata(region,snapshot)||snapshot.verified)
        return 13;
    region.body[12]='s';region.body[9]=255;
    if(sim::decode_region_metadata(region,snapshot))return 14;
    region.body[9]=4;
    const float invalid=std::numeric_limits<float>::infinity();
    std::memcpy(region.body.data()+31,&invalid,sizeof(float));
    if(sim::decode_region_metadata(region,snapshot))return 15;
    // R10 authenticated, neutral interest-list packet. A High 4 message
    // has an exact 115-byte decoded body with ControlFlags=0.
    const sim::AgentPosition verified{128.f,96.f,22.5f,true};
    if(!sim::neutral_agent_update(login,verified,9U,packet,96.f)||
       packet.flags!=lludp::kZerocoded||packet.body.size()!=115U||
       packet.body[0]!=4||packet.body[1]!=0x12||
       packet.body[17]!=0x12||packet.body[57]!=0||
       packet.body[110]!=0||packet.body[111]!=0||
       packet.body[112]!=0||packet.body[113]!=0||
       packet.body[114]!=0)return 18;
    // CameraCenter starts at byte 58, Far at byte 106.
    const float cameraZ=22.5f+1.6f;
    float decodedZ=0.f,decodedFar=0.f;
    std::memcpy(&decodedZ,packet.body.data()+66,sizeof(float));
    std::memcpy(&decodedFar,packet.body.data()+106,sizeof(float));
    if(decodedZ!=cameraZ||decodedFar!=96.f)return 19;
    std::vector<std::uint8_t> updatedWire{};
    lludp::Packet decodedUpdate{};
    if(!lludp::encode(packet,updatedWire)||
       !lludp::decode(updatedWire,decodedUpdate)||
       decodedUpdate.sequence!=9U||decodedUpdate.body!=packet.body)
        return 20;
    sim::AgentPosition unverified=verified;
    unverified.verified=false;
    if(sim::neutral_agent_update(login,unverified,9U,packet)||
       sim::neutral_agent_update(rejected,verified,9U,packet)||
       sim::neutral_agent_update(login,verified,1U,packet)||
       sim::neutral_agent_update(login,verified,9U,packet,0.f))
        return 21;
    unverified=verified;
    unverified.z=std::numeric_limits<float>::infinity();
    if(sim::neutral_agent_update(login,unverified,9U,packet))
        return 22;
    const auto controls=sim::movement_flags(true,false,true,false,
        true,false,true,true);
    if(controls!=(sim::kAtPos|sim::kLeftNeg|sim::kTurnRight|
                  sim::kFastAt|sim::kUpPos))return 23;
    if(sim::movement_flags(true,true,true,true,true,true,false,false)!=0U)
        return 24;
    if(!sim::controlled_agent_update(login,verified,10U,packet,
                                    controls)||
       packet.body.size()!=115U)return 25;
    std::uint32_t sent=0U;
    std::memcpy(&sent,packet.body.data()+110U,sizeof(sent));
    if(sent!=controls)return 26;
    if(sim::controlled_agent_update(login,verified,11U,packet,0x10000000U)||
       sim::controlled_agent_update(rejected,verified,11U,packet,controls)||
       sim::controlled_agent_update(login,unverified,11U,packet,controls))
        return 27;
    return 0;
}
