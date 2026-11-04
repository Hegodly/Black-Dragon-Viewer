#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/simulator_agent_update.hpp"
#include "kami/protocol/world_wire.hpp"
#include <ws2tcpip.h>
#include <algorithm>
#include <climits>
#include <utility>
#include <windows.h>
#include <array>
#include <string>
#include <vector>

namespace kami::protocol::sim {
namespace {
void clear_login(LoginResult& result) noexcept {
    // Store only short-lived IDs and seed URLs; no passwords enter Circuit.
    if(!result.handoff.seed_capability.empty())
        SecureZeroMemory(result.handoff.seed_capability.data(),
                         result.handoff.seed_capability.size()*sizeof(wchar_t));
    result={};
}
bool send_all(SOCKET sock,const lludp::Packet& packet) noexcept {
    std::vector<std::uint8_t> bytes;
    if(!lludp::encode(packet,bytes)||bytes.empty()||
       bytes.size()>static_cast<std::size_t>(INT_MAX))return false;
    return send(sock,reinterpret_cast<const char*>(bytes.data()),
                static_cast<int>(bytes.size()),0)==static_cast<int>(bytes.size());
}
std::uint32_t get_le32(const std::vector<std::uint8_t>& body,std::size_t at) noexcept {
    return static_cast<std::uint32_t>(body[at])|
       (static_cast<std::uint32_t>(body[at+1])<<8)|
       (static_cast<std::uint32_t>(body[at+2])<<16)|
       (static_cast<std::uint32_t>(body[at+3])<<24);
}
}
bool Circuit::send_packet(lludp::Packet packet) noexcept {
    if(socket_==INVALID_SOCKET)return false;
    return send_all(socket_,packet);
}
bool Circuit::send_startup_messages() noexcept {
    if(socket_==INVALID_SOCKET||!login_.accepted())return false;
    lludp::Packet use,movement;
    if(!use_circuit_code(login_,1U,use)||
       !complete_agent_movement(login_,2U,movement))return false;
    if(attempts_>0) {
        use.flags|=lludp::kResent;
        movement.flags|=lludp::kResent;
    }
    // Start with UseCircuitCode before sending agent movement.
    if(!use_acked_&&!send_packet(std::move(use)))return false;
    if(!move_acked_&&!send_packet(std::move(movement)))return false;
    ++attempts_;
    last_retry_=std::chrono::steady_clock::now();
    return true;
}
bool Circuit::start(const LoginResult& accepted_login) noexcept {
    stop();
    if(!accepted_login.accepted()||!valid_handoff(accepted_login.handoff)||
       accepted_login.circuit_code==0)return false;
    WSADATA wsa{};
    if(WSAStartup(MAKEWORD(2,2),&wsa)!=0)return false;
    winsock_ready_=true;
    addrinfo hints{};
    hints.ai_family=AF_INET;
    hints.ai_socktype=SOCK_DGRAM;
    hints.ai_protocol=IPPROTO_UDP;
    addrinfo* addresses=nullptr;
    const std::string service=std::to_string(accepted_login.handoff.simulator_port);
    if(getaddrinfo(accepted_login.handoff.simulator_host.c_str(),
                   service.c_str(),&hints,&addresses)!=0||!addresses){
        stop();return false;
    }
    bool connected=false;
    for(auto* address=addresses;address;address=address->ai_next){
        SOCKET candidate=socket(address->ai_family,address->ai_socktype,
                                address->ai_protocol);
        if(candidate==INVALID_SOCKET)continue;
        if(connect(candidate,address->ai_addr,
                   static_cast<int>(address->ai_addrlen))==0){
            socket_=candidate;connected=true;break;
        }
        closesocket(candidate);
    }
    freeaddrinfo(addresses);
    if(!connected){stop();return false;}
    u_long nonblocking=1UL;
    if(ioctlsocket(socket_,FIONBIO,&nonblocking)!=0){
        stop();return false;
    }
    login_=accepted_login;
    state_=CircuitState::WaitingForSimulator;
    started_=std::chrono::steady_clock::now();
    if(!send_startup_messages()){
        stop();state_=CircuitState::Failed;return false;
    }
    return true;
}
void Circuit::handle_packet(const lludp::Packet& incoming) noexcept {
    ++packets_received_;
    // These are bounded packet/framing observations from the actual
    // authenticated simulator UDP circuit. No geometry is synthesized.
    const auto world=worldwire::inspect(incoming);
    if(world.kind==worldwire::Kind::LayerData){
        ++layer_packets_;
    }else if(world.kind==worldwire::Kind::ObjectFull||
             world.kind==worldwire::Kind::ObjectCompressed||
             world.kind==worldwire::Kind::ObjectCached||
             world.kind==worldwire::Kind::ObjectTerse||
             world.kind==worldwire::Kind::ObjectKilled){
        ++object_update_packets_;
    }else if(world.kind==worldwire::Kind::Invalid){
        ++invalid_world_packets_;
    }
    if(state_==CircuitState::WaitingForSimulator)
        state_=CircuitState::SimulatorResponded;
    // Inspect ACKs before dispatching received messages.
    for(std::uint32_t ack:incoming.appended_acks){
        if(ack==1U)use_acked_=true;
        if(ack==2U)move_acked_=true;
    }
    auto type=classify(incoming);
    if(type==Inbound::PacketAck){
        for(std::size_t i=5;i+4<=incoming.body.size();i+=4){
            const auto ack=get_le32(incoming.body,i);
            if(ack==1U)use_acked_=true;
            if(ack==2U)move_acked_=true;
        }
    }
    if(incoming.flags&lludp::kReliable) {
        lludp::Packet ack{};
        const std::array<std::uint32_t,1> seq={incoming.sequence};
        if(packet_ack(seq,sequence_++,ack))(void)send_packet(std::move(ack));
    }
    if(type==Inbound::StartPing){
        lludp::Packet reply{};
        if(complete_ping(incoming.body[1],sequence_++,reply))
            (void)send_packet(std::move(reply));
    }
    if(type==Inbound::RegionHandshake) {
        // Region label and water elevation are extracted from the actual
        // connected simulator, never from KAMI's offline DX12 test scene.
        RegionMetadata parsed{};
        if(decode_region_metadata(incoming,parsed))
            region_metadata_=std::move(parsed);
        region_seen_=true;
        if(!handshake_replied_){
            lludp::Packet reply{};
            if(region_handshake_reply(login_,sequence_++,reply)&&
               send_packet(std::move(reply)))handshake_replied_=true;
        }
    }
    if(type==Inbound::AgentMovementComplete &&
       movement_complete_identifies(incoming,login_)){
        movement_seen_=true;
        SimulatorPose position{};
        if(decode_movement_pose(incoming,login_,position))
            simulator_pose_=position;
    }
    if(region_seen_&&movement_seen_)
        state_=CircuitState::MovementConfirmed;
    else if(region_seen_)state_=CircuitState::RegionHandshakeReceived;
}
void Circuit::maybe_send_agent_update() noexcept {
    if(socket_==INVALID_SOCKET||!login_.accepted()||
       state_!=CircuitState::MovementConfirmed||
       !simulator_pose_.verified)return;
    const auto now=std::chrono::steady_clock::now();
    // Send neutral, simulator-derived interest camera 5 times per second.
    // No fake WASD controls, no offline viewport/scene coordinates.
    if(last_agent_update_!=std::chrono::steady_clock::time_point{}&&
       now-last_agent_update_<std::chrono::milliseconds(200))return;
    lludp::Packet packet{};
    if(build_agent_update(login_,simulator_pose_,sequence_,packet)&&
       send_packet(std::move(packet))){
        ++sequence_;
        ++agent_updates_sent_;
        last_agent_update_=now;
    }
}
void Circuit::maybe_retry() noexcept {
    if(socket_==INVALID_SOCKET||state_==CircuitState::Failed||
       movement_seen_)return;
    const auto now=std::chrono::steady_clock::now();
    if(started_!=std::chrono::steady_clock::time_point{}&&
       now-started_>std::chrono::seconds(15)){
        state_=CircuitState::Failed;return;
    }
    if(attempts_>=4U ||now-last_retry_<std::chrono::seconds(1))return;
    if(use_acked_&&move_acked_)return;
    if(!send_startup_messages())state_=CircuitState::Failed;
}
void Circuit::poll(unsigned budget) noexcept {
    if(socket_==INVALID_SOCKET||state_==CircuitState::Failed)return;
    budget=std::min(budget,32U); // frame-budget protection
    for(unsigned i=0;i<budget;++i){
        std::array<std::uint8_t,8192> bytes{};
        const auto got=recv(socket_,reinterpret_cast<char*>(bytes.data()),
                            static_cast<int>(bytes.size()),0);
        if(got==SOCKET_ERROR) {
            const int err=WSAGetLastError();
            if(err!=WSAEWOULDBLOCK){
                state_=CircuitState::Failed;
            }
            break;
        }
        if(got<=0)break;
        lludp::Packet packet{};
        if(lludp::decode(
            std::span<const std::uint8_t>(bytes.data(),static_cast<std::size_t>(got)),
            packet))handle_packet(packet);
    }
    maybe_retry();
    maybe_send_agent_update();
}
void Circuit::stop() noexcept {
    if(socket_!=INVALID_SOCKET){closesocket(socket_);socket_=INVALID_SOCKET;}
    if(winsock_ready_){WSACleanup();winsock_ready_=false;}
    clear_login(login_);
    region_metadata_={};
    simulator_pose_={};
    agent_updates_sent_=0;
    last_agent_update_={};
    state_=CircuitState::Closed;
    region_seen_=movement_seen_=use_acked_=move_acked_=handshake_replied_=false;
    packets_received_=0;
    layer_packets_=object_update_packets_=invalid_world_packets_=0;
    sequence_=1;attempts_=0;last_retry_={};started_={};
}
std::wstring Circuit::diagnostic() const {
    switch(state_){
    case CircuitState::Closed:return L"SIMULATOR UDP NOT CONNECTED";
    case CircuitState::WaitingForSimulator:return L"WAITING FOR SIMULATOR UDP RESPONSE";
    case CircuitState::SimulatorResponded:return L"SIMULATOR REPLIED; REGION NOT READY";
    case CircuitState::RegionHandshakeReceived:
        return L"REGION HANDSHAKE RECEIVED; AGENT MOVEMENT PENDING";
    case CircuitState::MovementConfirmed:
        return L"SIMULATOR MOVEMENT CONFIRMED; CAPS NEGOTIATION STARTING";
    case CircuitState::Failed:return L"SIMULATOR UDP ERROR";
    }
    return L"UNKNOWN SIMULATOR STATE";
}
} // namespace kami::protocol::sim
