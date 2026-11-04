#include "kami/protocol/simulator_agent_update.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace kami::protocol::sim {
namespace {
void write_float(std::vector<std::uint8_t>& body,float value){
    std::array<std::uint8_t,4> b{};
    static_assert(sizeof(float)==4);
    std::memcpy(b.data(),&value,4);
    body.insert(body.end(),b.begin(),b.end());
}
void write_vec(std::vector<std::uint8_t>& body,
               const std::array<float,3>& v){
    for(float f:v)write_float(body,f);
}
}
bool decode_movement_pose(const lludp::Packet& packet,
                          const LoginResult& login,
                          SimulatorPose& out) noexcept {
    out={};
    if(!movement_complete_identifies(packet,login)||
       packet.body.size()<60U)return false;
    for(unsigned k=0;k<3;++k){
        std::memcpy(&out.position[k],packet.body.data()+36+4*k,4);
        std::memcpy(&out.look_at[k],packet.body.data()+48+4*k,4);
        if(!std::isfinite(out.position[k])||
           !std::isfinite(out.look_at[k]))return false;
    }
    if(out.position[0]<-16.f||out.position[0]>512.f||
       out.position[1]<-16.f||out.position[1]>512.f||
       out.position[2]<-4096.f||out.position[2]>8192.f)
        return false;
    const float len=std::hypot(out.look_at[0],out.look_at[1]);
    if(!std::isfinite(len)||len<0.01f)return false;
    // A horizontal interest camera avoids relying on offline DX12 pose.
    out.look_at={out.look_at[0]/len,out.look_at[1]/len,0.f};
    out.verified=true;
    return true;
}
bool build_agent_update(const LoginResult& login,
                        const SimulatorPose& pose,
                        std::uint32_t seq,lludp::Packet& out) noexcept {
    if(!login.accepted()||!valid_handoff(login.handoff)||
       !pose.verified||seq==0)return false;
    for(float a:pose.position)if(!std::isfinite(a))return false;
    const auto& look=pose.look_at;
    if(!std::isfinite(look[0])||!std::isfinite(look[1])||
       !std::isfinite(look[2])||
       std::abs(std::hypot(look[0],look[1])-1.f)>0.01f||
       std::abs(look[2])>0.01f)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet packet{};
    packet.sequence=seq;
    packet.flags=lludp::kZerocoded; // Message template: High 4 Zerocoded
    packet.body={4U};
    packet.body.insert(packet.body.end(),agent.begin(),agent.end());
    packet.body.insert(packet.body.end(),session.begin(),session.end());
    // SL quaternion on the wire uses its x/y/z components.
    // Identity body/head rotation while movement control is unimplemented.
    write_vec(packet.body,{0.f,0.f,0.f}); // BodyRotation
    write_vec(packet.body,{0.f,0.f,0.f}); // HeadRotation
    packet.body.push_back(0U); // State: idle
    // Position and facing came from the authenticated simulator.
    const auto left=std::array<float,3>{-look[1],look[0],0.f};
    write_vec(packet.body,{pose.position[0]-look[0]*2.5f,
                           pose.position[1]-look[1]*2.5f,
                           pose.position[2]+1.65f}); // CameraCenter
    write_vec(packet.body,look);                    // CameraAtAxis
    write_vec(packet.body,left);                    // CameraLeftAxis
    write_vec(packet.body,{0.f,0.f,1.f});          // CameraUpAxis
    write_float(packet.body,64.f); // Initial Far draw distance (m)
    packet.body.insert(packet.body.end(),4U,0U); // neutral ControlFlags U32
    packet.body.push_back(0U); // Flags U8
    if(packet.body.size()!=115U)return false;
    out=std::move(packet);return true;
}
}