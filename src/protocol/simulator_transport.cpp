#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/world_wire.hpp"
#include "kami/protocol/object_spatial.hpp"
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
void Circuit::set_requested_controls(std::uint32_t flags) noexcept {
    requested_controls_=(flags&kAllowedMovement);
    last_controls_at_=std::chrono::steady_clock::now();
}
void Circuit::maybe_send_agent_update() noexcept {
    if(state_!=CircuitState::MovementConfirmed||
       !movement_position_.verified||socket_==INVALID_SOCKET)return;
    const auto now=std::chrono::steady_clock::now();
    if(last_agent_update_!=std::chrono::steady_clock::time_point{}&&
       now-last_agent_update_<std::chrono::milliseconds(200))return;
    std::uint32_t controls=0U;
    if(last_controls_at_!=std::chrono::steady_clock::time_point{}&&
       now-last_controls_at_<std::chrono::milliseconds(300))
        controls=requested_controls_;
    lludp::Packet update{};
    if(!controlled_agent_update(login_,movement_position_,sequence_,update,
                                controls))return;
    if(send_packet(std::move(update))){
        ++sequence_;
        ++agent_updates_sent_;
        last_controls_sent_=controls;
        if(controls!=0U && movement_control_packets_sent_!=UINT_MAX)
            ++movement_control_packets_sent_;
        last_agent_update_=now;
    }
}
void Circuit::remember_missing_object(std::uint32_t id) noexcept {
    if(id==0U||spatial_objects_.contains(id)||
       missing_objects_.contains(id)||missing_objects_.size()>=256U)return;
    missing_objects_.try_emplace(id);
}
void Circuit::maybe_send_missing_object_requests() noexcept {
    if(state_!=CircuitState::MovementConfirmed||
       !movement_position_.verified||socket_==INVALID_SOCKET||
       missing_objects_.empty())return;
    const auto now=std::chrono::steady_clock::now();
    if(last_object_request_!=std::chrono::steady_clock::time_point{}&&
       now-last_object_request_<std::chrono::milliseconds(400))return;
    std::vector<std::uint32_t> ids;
    ids.reserve(16U);
    for(const auto& [id,state]:missing_objects_){
        if(state.attempts>=2U)continue;
        if(state.sent_at!=std::chrono::steady_clock::time_point{}&&
           now-state.sent_at<std::chrono::seconds(8))continue;
        if(spatial_objects_.contains(id))continue;
        ids.push_back(id);
        if(ids.size()==16U)break;
    }
    if(ids.empty())return;
    lludp::Packet pkt{};
    if(!request_missing_objects(login_,ids,sequence_,pkt))return;
    if(send_packet(std::move(pkt))){
        ++sequence_;
        last_object_request_=now;
        for(auto id:ids){
            auto entry=missing_objects_.find(id);
            if(entry!=missing_objects_.end()){
                ++entry->second.attempts;
                entry->second.sent_at=now;
            }
        }
        requested_object_ids_+=static_cast<std::uint32_t>(ids.size());
    }
}
void Circuit::handle_packet(const lludp::Packet& incoming) noexcept {
    ++packets_received_;
    // These are bounded packet/framing observations from the actual
    // authenticated simulator UDP circuit. No geometry is synthesized.
    const auto world=worldwire::inspect(incoming);
    if(world.kind==worldwire::Kind::LayerData){
        ++layer_packets_;
        switch(world.layer_type){
        case worldwire::LayerType::Terrain:{
            ++terrain_packets_;
            if(region_seen_&&movement_seen_&&region_metadata_.verified&&
               movement_position_.verified){
                std::vector<terrain::Patch> decoded;
                if(terrain::decode_land_layer(incoming,decoded)){
                    ++decoded_land_packets_;
                    for(auto& patch:decoded){
                        const auto key=static_cast<std::uint16_t>(
                            static_cast<unsigned>(patch.y)*32U+patch.x);
                        if(land_patches_.size()>=1024U &&
                           !land_patches_.contains(key))continue;
                        land_patches_.insert_or_assign(key,std::move(patch));
                        ++land_revision_;
                    }
                }else ++rejected_land_packets_;
            }
            break;
        }
        case worldwire::LayerType::Wind:++wind_packets_;break;
        case worldwire::LayerType::Cloud:++cloud_packets_;break;
        default:++unknown_layer_packets_;break;
        }
    }else if(world.kind==worldwire::Kind::Invalid){
        ++invalid_world_packets_;
    }else{
        switch(world.kind){
        case worldwire::Kind::ObjectFull:{
            ++object_full_packets_;
            // R23: real SL High 12 full-update blocks have a completely
            // different message-template layout from packed High 13.
            // Only finite root volume position/size is accepted, never
            // invented mesh, texture, or skinned avatar geometry.
            if(region_seen_&&movement_seen_&&region_metadata_.verified&&
               movement_position_.verified){
                std::vector<objectspatial::Transform> objects;
                std::vector<std::uint32_t> parent_ids;
                objectspatial::FullDecodeAudit frame{};
                if(objectspatial::decode_full_transforms(
                       incoming,objects,&frame,&parent_ids)){
                    ++spatial_object_frames_;
                    ++decoded_full_spatial_frames_;
                    full_decode_audit_.volume+=frame.volume;
                    full_decode_audit_.non_volume+=frame.non_volume;
                    full_decode_audit_.linked+=frame.linked;
                    full_decode_audit_.unsupported_motion+=frame.unsupported_motion;
                    full_decode_audit_.invalid_transform+=frame.invalid_transform;
                    full_decode_audit_.accepted_roots+=frame.accepted_roots;
                    for(auto id:parent_ids)remember_missing_object(id);
                    for(const auto& obj:objects){
                        if(spatial_objects_.size()>=512U &&
                           !spatial_objects_.contains(obj.local_id))continue;
                        spatial_objects_.insert_or_assign(obj.local_id,obj);
                        missing_objects_.erase(obj.local_id);
                        ++spatial_object_revision_;
                    }
                }else ++rejected_spatial_frames_;
            }
            break;
        }
        case worldwire::Kind::ObjectCompressed:{
            ++object_compressed_packets_;
            if(region_seen_&&movement_seen_&&region_metadata_.verified&&
               movement_position_.verified){
                std::vector<objectspatial::Transform> objects;
                if(objectspatial::decode_compressed_transforms(incoming,objects)){
                    ++spatial_object_frames_;
                    for(const auto& obj:objects){
                        if(spatial_objects_.size()>=512U &&
                           !spatial_objects_.contains(obj.local_id))continue;
                        spatial_objects_.insert_or_assign(obj.local_id,obj);
                        missing_objects_.erase(obj.local_id);
                        ++spatial_object_revision_;
                    }
                }else ++rejected_spatial_frames_;
            }
            break;
        }
        case worldwire::Kind::ObjectCached:{
            ++object_cached_packets_;
            if(region_seen_&&movement_seen_&&region_metadata_.verified&&
               movement_position_.verified){
                std::vector<std::uint32_t> ids;
                if(objectspatial::decode_cached_ids(incoming,ids)){
                    cached_object_ids_+=static_cast<std::uint32_t>(ids.size());
                    for(auto id:ids)remember_missing_object(id);
                }
            }
            break;
        }
        case worldwire::Kind::ObjectTerse:++object_terse_packets_;break;
        case worldwire::Kind::ObjectKilled:{
            ++object_killed_packets_;
            if(region_seen_&&movement_seen_){
                std::vector<std::uint32_t> ids;
                if(objectspatial::decode_killed_ids(incoming,ids))
                    for(const auto id:ids){
                        missing_objects_.erase(id);
                        if(spatial_objects_.erase(id)>0U)
                            ++spatial_object_revision_;
                    }
            }
            break;
        }
        default:break;
        }
        if(world.kind==worldwire::Kind::ObjectFull||
           world.kind==worldwire::Kind::ObjectCompressed||
           world.kind==worldwire::Kind::ObjectCached||
           world.kind==worldwire::Kind::ObjectTerse||
           world.kind==worldwire::Kind::ObjectKilled)
            ++object_update_packets_;
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
        AgentPosition parsed{};
        if(decode_agent_movement_position(incoming,login_,parsed))
            movement_position_=parsed;
    }
    if(region_seen_&&movement_seen_)
        state_=CircuitState::MovementConfirmed;
    else if(region_seen_)state_=CircuitState::RegionHandshakeReceived;
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
    maybe_send_missing_object_requests();
}
void Circuit::stop() noexcept {
    if(socket_!=INVALID_SOCKET){closesocket(socket_);socket_=INVALID_SOCKET;}
    if(winsock_ready_){WSACleanup();winsock_ready_=false;}
    clear_login(login_);
    region_metadata_={};
    movement_position_={};
    state_=CircuitState::Closed;
    region_seen_=movement_seen_=use_acked_=move_acked_=handshake_replied_=false;
    packets_received_=agent_updates_sent_=0;
    requested_controls_=last_controls_sent_=0;
    movement_control_packets_sent_=0;
    last_controls_at_={};
    layer_packets_=object_update_packets_=invalid_world_packets_=0;
    terrain_packets_=wind_packets_=cloud_packets_=unknown_layer_packets_=0;
    decoded_land_packets_=rejected_land_packets_=0;
    land_revision_=0;land_patches_.clear();
    spatial_object_revision_=0;
    spatial_object_frames_=rejected_spatial_frames_=0;
    decoded_full_spatial_frames_=0;
    full_decode_audit_={};
    spatial_objects_.clear();
    missing_objects_.clear();
    requested_object_ids_=cached_object_ids_=0U;
    last_object_request_={};
    object_full_packets_=object_compressed_packets_=0;
    object_cached_packets_=object_terse_packets_=object_killed_packets_=0;
    sequence_=3;attempts_=0;last_retry_={};started_={};
    last_agent_update_={};
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
