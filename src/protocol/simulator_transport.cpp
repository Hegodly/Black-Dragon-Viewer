#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/world_wire.hpp"
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/live_chat.hpp"
#include "kami/protocol/avatar_appearance.hpp"
#include "kami/protocol/mesh_lod.hpp"
#include <ws2tcpip.h>
#include <algorithm>
#include <cmath>
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
    if(socket_==INVALID_SOCKET||!reliable_window_.can_send(packet))
        return false;
    // Track the original reliable sequence, including a transient failed
    // nonblocking send. The bounded retry queue will transmit it again.
    try {
        if(!reliable_window_.record_sent(packet,
                   reliable::Window::Clock::now()))return false;
    }catch(...){return false;}
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
bool Circuit::set_mesh_capability(std::wstring capability) noexcept {
    if(!caps::trusted_mesh_caps_url(capability)||!mesh_capability_url_.empty()||
       state_!=CircuitState::MovementConfirmed)return false;
    mesh_capability_url_=std::move(capability);
    mesh_scan_revision_=~std::uint64_t{0};
    mesh_last_scan_position_valid_=false;
    mesh_last_scan_x_=mesh_last_scan_y_=mesh_last_scan_z_=0.f;
    return true;
}
const objectspatial::Transform* Circuit::world_mesh_anchor(
    const objectspatial::Transform& item) const noexcept {
    if(item.parent_id==0U)return &item;
    std::array<std::uint32_t,16U> seen{};
    std::size_t depth=0U;
    auto parent=item.parent_id;
    while(parent!=0U&&depth<seen.size()){
        if(avatar_root_ids_.contains(parent)||
           std::find(seen.begin(),seen.begin()+depth,parent)!=
               seen.begin()+depth)return nullptr;
        seen[depth++]=parent;
        const auto root=spatial_objects_.find(parent);
        if(root!=spatial_objects_.end())
            return root->second.parent_id==0U?&root->second:nullptr;
        const auto linked=linked_objects_.find(parent);
        if(linked==linked_objects_.end())return nullptr;
        parent=linked->second.parent_id;
    }
    return nullptr;
}
void Circuit::prune_unreferenced_meshes() noexcept {
    if(loaded_meshes_.empty())return;
    // Hysteresis (176m vs 144m load radius) avoids thrashing across a
    // visibility boundary. Drop no-longer-needed CPU mesh data and its
    // finished scheduler entry; do not cancel an active fetch.
    const auto within_retention_radius=[&](const objectspatial::Transform& t) noexcept {
        return std::isfinite(t.x)&&std::isfinite(t.y)&&std::isfinite(t.z)&&
            std::abs(t.x-movement_position_.x)<=176.f&&
            std::abs(t.y-movement_position_.y)<=176.f&&
            std::abs(t.z-movement_position_.z)<=128.f;
    };
    const auto needed=[&](std::string_view asset) noexcept {
        const auto uses=[&](const auto& objects) noexcept {
            for(const auto& [id,t]:objects){
                (void)id;
                if(t.mesh_asset_id!=asset)continue;
                const auto* anchor=world_mesh_anchor(t);
                if(anchor&&within_retention_radius(*anchor))return true;
            }
            return false;
        };
        return uses(spatial_objects_)||uses(linked_objects_);
    };
    for(auto it=loaded_meshes_.begin();it!=loaded_meshes_.end();){
        if(needed(it->first)){++it;continue;}
        const auto triangles=it->second.triangles.size();
        loaded_mesh_triangle_count_=
            triangles>loaded_mesh_triangle_count_?0U:
                loaded_mesh_triangle_count_-triangles;
        (void)mesh_scheduler_.forget_finished(it->first);
        it=loaded_meshes_.erase(it);
        ++spatial_object_revision_;
    }
}
void Circuit::schedule_visible_meshes() noexcept {
    if(mesh_capability_url_.empty()||!movement_position_.verified)return;
    // Spatial-object revision alone is insufficient: walking from one
    // house to another must discover newly visible mesh assets even if
    // the simulator sends no full ObjectUpdate messages.
    const bool moved=!mesh_last_scan_position_valid_||
        std::hypot(std::hypot(
            movement_position_.x-mesh_last_scan_x_,
            movement_position_.y-mesh_last_scan_y_),
            movement_position_.z-mesh_last_scan_z_)>=12.f;
    if(mesh_scan_revision_==spatial_object_revision_&&!moved)return;
    prune_unreferenced_meshes();
    mesh_scan_revision_=spatial_object_revision_;
    mesh_last_scan_x_=movement_position_.x;
    mesh_last_scan_y_=movement_position_.y;
    mesh_last_scan_z_=movement_position_.z;
    mesh_last_scan_position_valid_=true;
    const auto close_to_agent=[&](const objectspatial::Transform& t) noexcept {
        return std::isfinite(t.x)&&std::isfinite(t.y)&&std::isfinite(t.z)&&
            std::abs(t.x-movement_position_.x)<=144.f&&
            std::abs(t.y-movement_position_.y)<=144.f&&
            std::abs(t.z-movement_position_.z)<=96.f;
    };
    const auto schedule=[&](const auto& objects) noexcept {
        for(const auto& [id,t]:objects){
            (void)id;
            if(t.mesh_asset_id.empty())continue;
            const auto* anchor=world_mesh_anchor(t);
            if(!anchor||!close_to_agent(*anchor))continue;
            const float d=std::hypot(
                std::hypot(anchor->x-movement_position_.x,
                           anchor->y-movement_position_.y),
                anchor->z-movement_position_.z);
            (void)mesh_scheduler_.request(t.mesh_asset_id,
                                         load::Kind::VisibleMesh,d,true,0U);
        }
    };
    schedule(spatial_objects_);
    schedule(linked_objects_);
}
void Circuit::maybe_fetch_mesh() noexcept {
    schedule_visible_meshes();
    if(state_!=CircuitState::MovementConfirmed)return;
    if(mesh_worker_.valid()){
        if(mesh_worker_.wait_for(std::chrono::seconds(0))!=
           std::future_status::ready)return;
        try{
            auto result=mesh_worker_.get();
            (void)mesh_scheduler_.download_finished(
                result.id,result.bytes,result.fetched);
            if(result.fetched){
                ++mesh_download_count_;
                const auto item=mesh_scheduler_.next_decode();
                if(item&&item->id==result.id){
                    const auto old=loaded_meshes_.find(result.id);
                    const auto prior=old==loaded_meshes_.end()?0U:
                        old->second.triangles.size();
                    const auto used=loaded_mesh_triangle_count_-
                        std::min(prior,loaded_mesh_triangle_count_);
                    const bool room=
                        (old!=loaded_meshes_.end()||
                         loaded_meshes_.size()<kMaxWorldMeshAssets)&&
                        used<=kMaxWorldMeshTriangles&&
                        result.lod.triangles.size()<=
                            kMaxWorldMeshTriangles-used;
                    const bool ok=result.decoded&&room;
                    (void)mesh_scheduler_.decode_finished(result.id,ok);
                    if(ok){
                        loaded_mesh_triangle_count_=
                            used+result.lod.triangles.size();
                        loaded_meshes_.insert_or_assign(
                            result.id,std::move(result.lod));
                        ++spatial_object_revision_;
                    }else ++mesh_failure_count_;
                }
            }else {
                ++mesh_failure_count_;
                // Failed HTTP connections / throttling are recoverable,
                // unlike 4xx missing assets or a malformed mesh payload.
                const auto status=result.http_status;
                const bool transient=status==0U||status==429U||
                    status==500U||status==502U||status==503U||
                    status==504U;
                const auto* job=mesh_scheduler_.find(result.id);
                if(transient&&job&&
                   job->retry_count<load::Scheduler::kMaxTransientRetries&&
                   mesh_retry_after_.size()<128U){
                    const auto delay=std::chrono::seconds(
                        1U<<std::min(job->retry_count,3U));
                    mesh_retry_after_.insert_or_assign(
                        result.id,std::chrono::steady_clock::now()+delay);
                }
            }
        }catch(...){++mesh_failure_count_;}
    }
    // Requeue only failed transient network fetches once their quiet period
    // elapses. A 404 or bad geometry is not retried endlessly.
    const auto retry_now=std::chrono::steady_clock::now();
    for(auto it=mesh_retry_after_.begin();it!=mesh_retry_after_.end();){
        if(retry_now>=it->second){
            (void)mesh_scheduler_.retry_failed(it->first);
            it=mesh_retry_after_.erase(it);
        }else ++it;
    }
    if(mesh_worker_.valid()||mesh_capability_url_.empty())return;
    const auto now=std::chrono::steady_clock::now();
    if(mesh_last_start_!=std::chrono::steady_clock::time_point{}&&
       now-mesh_last_start_<std::chrono::milliseconds(350))return;
    if(loaded_meshes_.size()>=kMaxWorldMeshAssets||
       loaded_mesh_triangle_count_>=kMaxWorldMeshTriangles)return;
    auto selected=mesh_scheduler_.next_download();
    if(!selected)return;
    mesh_last_start_=now;
    auto cap=mesh_capability_url_;
    mesh_worker_=std::async(std::launch::async,
       [url=std::move(cap),id=selected->id]()mutable{
        MeshResult result{};
        result.id=id;
        const auto reply=caps::fetch_mesh_asset(std::move(url),id);
        result.fetched=reply.fetched;
        result.http_status=reply.status;
        result.bytes=static_cast<std::uint32_t>(reply.bytes.size());
        if(reply.fetched)
            result.decoded=mesh::decode_mesh_lod(reply.bytes,result.lod);
        return result;
    });
}
bool Circuit::send_nearby_chat(std::wstring_view text) noexcept {
    if(state_!=CircuitState::MovementConfirmed||
       !region_metadata_.verified||!movement_position_.verified||
       socket_==INVALID_SOCKET)return false;
    const auto now=std::chrono::steady_clock::now();
    if(last_chat_sent_!=std::chrono::steady_clock::time_point{}&&
       now-last_chat_sent_<std::chrono::milliseconds(600))return false;
    lludp::Packet packet{};
    if(!build_nearby_chat(login_,text,sequence_,packet))return false;
    if(!send_packet(std::move(packet)))return false;
    ++sequence_;
    ++sent_chat_packets_;
    last_chat_sent_=now;
    // No local bubble: packet send is NOT server delivery confirmation.
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
    if(id==0U||avatar_root_ids_.contains(id)||
       spatial_objects_.contains(id)||linked_objects_.contains(id)||
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
        // Lost cached-root responses used to strand entire linked sets.
        // Bound retries to five, with a growing quiet period to avoid
        // flooding simulators if an object can never be recovered.
        if(state.attempts>=5U)continue;
        const auto quiet=std::chrono::seconds(8+4*state.attempts);
        if(state.sent_at!=std::chrono::steady_clock::time_point{}&&
           now-state.sent_at<quiet)continue;
        if(spatial_objects_.contains(id)||linked_objects_.contains(id))
            continue;
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
    // Acknowledge all outbound packet sequences, not only the two login
    // handshake packets. Appended LLUDP ACKs and PacketAck blocks may
    // arrive together, including on retransmitted frames.
    const auto confirmed=[this](std::uint32_t ack) noexcept {
        reliable_window_.acknowledge(ack);
        if(ack==1U)use_acked_=true;
        if(ack==2U)move_acked_=true;
    };
    for(const auto ack:incoming.appended_acks)confirmed(ack);
    const auto type=classify(incoming);
    if(type==Inbound::PacketAck){
        for(std::size_t i=5;i+4<=incoming.body.size();i+=4)
            confirmed(get_le32(incoming.body,i));
    }
    // ACK before replay suppression: a simulator may resend a packet
    // because our first ACK was lost. Do not decode the world twice.
    if(incoming.flags&lludp::kReliable){
        lludp::Packet acknowledgment{};
        const std::array<std::uint32_t,1> seq={incoming.sequence};
        if(packet_ack(seq,sequence_++,acknowledgment))
            (void)send_packet(std::move(acknowledgment));
        if(!reliable_window_.accept_incoming(incoming.sequence))
            return;
    }
    // These are bounded packet/framing observations from the actual
    // authenticated simulator UDP circuit. No geometry is synthesized.
    const auto world=worldwire::inspect(incoming);
    // These packet bodies are only admitted from the already connected,
    // authenticated simulator socket. Keep only bounded user-visible chat
    // and appearance counts; never persist message/asset identities.
    if(state_==CircuitState::MovementConfirmed&&
       region_metadata_.verified&&movement_position_.verified){
        ChatLine line{};
        if(decode_simulator_chat(incoming,line)){
            if(nearby_chat_.size()>=40U)nearby_chat_.erase(nearby_chat_.begin());
            nearby_chat_.push_back(std::move(line));
        }
        appearance::Observation avatar{};
        if(appearance::decode_avatar_appearance(incoming,avatar)){
            if(avatar_appearance_packets_<UINT_MAX)++avatar_appearance_packets_;
            std::array<std::uint8_t,16> self_id{};
            if(parse_uuid(login_.handoff.agent_id,self_id)&&
               avatar.avatar_id==self_id)
                own_avatar_visual_params_=avatar.visual_parameter_count;
        }
    }
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
                std::vector<objectspatial::Transform> linked;
                std::vector<std::uint32_t> parent_ids;
                std::vector<std::uint32_t> avatar_ids;
                std::vector<objectspatial::AvatarFullPose> avatar_poses;
                objectspatial::FullDecodeAudit frame{};
                if(objectspatial::decode_full_transforms(
                       incoming,objects,&frame,&parent_ids,&linked,
                       &avatar_ids,&avatar_poses)){
                    // Only the authenticated login AgentID can define our
                    // own avatar local ID and authoritative world origin.
                    std::array<std::uint8_t,16U> own_uuid{};
                    if(parse_uuid(login_.handoff.agent_id,own_uuid)){
                        for(const auto& avatar:avatar_poses){
                            if(avatar.full_id!=own_uuid)continue;
                            own_avatar_local_id_=avatar.local_id;
                            const float moved=std::hypot(std::hypot(
                                avatar.x-movement_position_.x,
                                avatar.y-movement_position_.y),
                                avatar.z-movement_position_.z);
                            if(moved>=.04f){
                                movement_position_.x=avatar.x;
                                movement_position_.y=avatar.y;
                                movement_position_.z=avatar.z;
                                ++spatial_object_revision_;
                            }
                            break;
                        }
                    }
                    ++spatial_object_frames_;
                    ++decoded_full_spatial_frames_;
                    full_decode_audit_.volume+=frame.volume;
                    full_decode_audit_.non_volume+=frame.non_volume;
                    full_decode_audit_.linked+=frame.linked;
                    full_decode_audit_.unsupported_motion+=frame.unsupported_motion;
                    full_decode_audit_.invalid_transform+=frame.invalid_transform;
                    full_decode_audit_.accepted_roots+=frame.accepted_roots;
                    full_decode_audit_.accepted_children+=frame.accepted_children;
                    full_decode_audit_.unsupported_shapes+=frame.unsupported_shapes;
                    full_decode_audit_.texture_references+=frame.texture_references;
                    full_decode_audit_.mesh_references+=frame.mesh_references;
                    full_decode_audit_.extra_records+=frame.extra_records;
                    full_decode_audit_.sculpt_records+=frame.sculpt_records;
                    full_decode_audit_.mesh_sculpt_records+=frame.mesh_sculpt_records;
                    full_decode_audit_.extra_envelopes+=frame.extra_envelopes;
                    full_decode_audit_.malformed_extra_envelopes+=frame.malformed_extra_envelopes;
                    full_decode_audit_.flexible_records+=frame.flexible_records;
                    full_decode_audit_.light_records+=frame.light_records;
                    full_decode_audit_.projector_records+=frame.projector_records;
                    full_decode_audit_.other_extra_records+=frame.other_extra_records;
                    full_decode_audit_.extended_mesh_records+=frame.extended_mesh_records;
                    full_decode_audit_.render_material_records+=frame.render_material_records;
                    full_decode_audit_.reflection_probe_records+=frame.reflection_probe_records;
                    for(const auto id:avatar_ids){
                        if(avatar_root_ids_.size()<2048U){
                            if(avatar_root_ids_.insert(id).second)
                                ++spatial_object_revision_;
                            missing_objects_.erase(id);
                        }
                    }
                    for(auto id:parent_ids)remember_missing_object(id);
                    for(const auto& child:linked){
                        if(linked_objects_.size()>=1024U&&
                           !linked_objects_.contains(child.local_id))continue;
                        const auto prior=linked_objects_.find(child.local_id);
                        if(prior==linked_objects_.end()||
                           !objectspatial::same_spatial_transform(prior->second,child)){
                            linked_objects_.insert_or_assign(child.local_id,child);
                            ++spatial_object_revision_;
                        }
                        missing_objects_.erase(child.local_id);
                    }
                    for(const auto& obj:objects){
                        if(spatial_objects_.size()>=512U &&
                           !spatial_objects_.contains(obj.local_id))continue;
                        const auto prior=spatial_objects_.find(obj.local_id);
                        if(prior==spatial_objects_.end()||
                           !objectspatial::same_spatial_transform(prior->second,obj)){
                            spatial_objects_.insert_or_assign(obj.local_id,obj);
                            ++spatial_object_revision_;
                        }
                        missing_objects_.erase(obj.local_id);
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
                std::vector<objectspatial::Transform> linked;
                objectspatial::CompressedDecodeAudit frame{};
                if(objectspatial::decode_compressed_transforms(
                       incoming,objects,&linked,&frame)){
                    compressed_decode_audit_.extras_seen+=frame.extras_seen;
                    compressed_decode_audit_.extra_records+=frame.extra_records;
                    compressed_decode_audit_.sculpt_records+=frame.sculpt_records;
                    compressed_decode_audit_.shape_fields+=frame.shape_fields;
                    compressed_decode_audit_.supported_shapes+=frame.supported_shapes;
                    compressed_decode_audit_.mesh_references+=frame.mesh_references;
                    compressed_decode_audit_.unsupported_optional+=
                        frame.unsupported_optional;
                    compressed_decode_audit_.legacy_particle_blocks+=
                        frame.legacy_particle_blocks;
                    ++spatial_object_frames_;
                    for(const auto& child:linked){
                        remember_missing_object(child.parent_id);
                        if(linked_objects_.size()>=1024U&&
                           !linked_objects_.contains(child.local_id))continue;
                        // Compressed child topology is not decoded yet;
                        // preserve an earlier verified full-update shape.
                        auto updated=child;
                        const auto previous=linked_objects_.find(child.local_id);
                        if(previous!=linked_objects_.end())
                            updated=objectspatial::merge_compressed_transform(
                                child,previous->second);
                        if(previous==linked_objects_.end()||
                           !objectspatial::same_spatial_transform(
                                previous->second,updated)){
                            linked_objects_.insert_or_assign(child.local_id,updated);
                            ++spatial_object_revision_;
                        }
                        missing_objects_.erase(child.local_id);
                    }
                    for(const auto& obj:objects){
                        if(spatial_objects_.size()>=512U &&
                           !spatial_objects_.contains(obj.local_id))continue;
                        auto updated=obj;
                        const auto previous=spatial_objects_.find(obj.local_id);
                        if(previous!=spatial_objects_.end())
                            updated=objectspatial::merge_compressed_transform(
                                obj,previous->second);
                        if(previous==spatial_objects_.end()||
                           !objectspatial::same_spatial_transform(
                                previous->second,updated)){
                            spatial_objects_.insert_or_assign(obj.local_id,updated);
                            ++spatial_object_revision_;
                        }
                        missing_objects_.erase(obj.local_id);
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
        case worldwire::Kind::ObjectTerse:{
            ++object_terse_packets_;
            if(region_seen_&&movement_seen_&&region_metadata_.verified&&
               movement_position_.verified){
                std::vector<objectspatial::TersePose> poses;
                if(objectspatial::decode_terse_poses(incoming,poses)){
                    for(const auto& pose:poses){
                        if(own_avatar_local_id_!=0U&&
                           pose.local_id==own_avatar_local_id_){
                            // Server-confirmed own avatar pose; update
                            // region-relative scene origin and allow new
                            // nearby mesh assets to become eligible.
                            if(pose.x>=0.f&&pose.x<256.f&&
                               pose.y>=0.f&&pose.y<256.f&&
                               pose.z>=-4096.f&&pose.z<8192.f){
                                const float moved=std::hypot(std::hypot(
                                    pose.x-movement_position_.x,
                                    pose.y-movement_position_.y),
                                    pose.z-movement_position_.z);
                                if(moved>=.04f){
                                    movement_position_.x=pose.x;
                                    movement_position_.y=pose.y;
                                    movement_position_.z=pose.z;
                                    ++spatial_object_revision_;
                                }
                            }
                            continue;
                        }
                        if(avatar_root_ids_.contains(pose.local_id))
                            continue; // never guess another avatar's pose
                        const auto root=spatial_objects_.find(pose.local_id);
                        if(root!=spatial_objects_.end()){
                            if(objectspatial::apply_terse_pose(
                                  root->second,pose))
                                ++spatial_object_revision_;
                            continue;
                        }
                        const auto child=linked_objects_.find(pose.local_id);
                        if(child!=linked_objects_.end()){
                            if(objectspatial::apply_terse_pose(
                                  child->second,pose))
                                ++spatial_object_revision_;
                            continue;
                        }
                        // Missing full update: ask for a verified object
                        // rather than inventing mesh geometry from a pose.
                        remember_missing_object(pose.local_id);
                    }
                }
            }
            break;
        }
        case worldwire::Kind::ObjectKilled:{
            ++object_killed_packets_;
            if(region_seen_&&movement_seen_){
                std::vector<std::uint32_t> ids;
                if(objectspatial::decode_killed_ids(incoming,ids))
                    for(const auto id:ids){
                        missing_objects_.erase(id);
                        if(id==own_avatar_local_id_)own_avatar_local_id_=0U;
                        if(avatar_root_ids_.erase(id)>0U)
                            ++spatial_object_revision_;
                        if(spatial_objects_.erase(id)>0U)
                            ++spatial_object_revision_;
                        if(linked_objects_.erase(id)>0U)
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
    if(region_seen_&&movement_seen_){
        state_=CircuitState::MovementConfirmed;
        mesh_scheduler_.set_online(true);
        texture_scheduler_.set_online(true);
    }
    else if(region_seen_)state_=CircuitState::RegionHandshakeReceived;
}
void Circuit::retry_unacknowledged() noexcept {
    if(socket_==INVALID_SOCKET||state_==CircuitState::Failed)return;
    // At most four retransmits per frame: no unbounded send/CPU spikes.
    try {
        const auto retries=reliable_window_.due(
            reliable::Window::Clock::now(),4U);
        for(const auto& packet:retries)
            (void)send_all(socket_,packet);
    }catch(...){
        // An allocation failure must not terminate the viewer frame loop.
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
    retry_unacknowledged();
    maybe_retry();
    maybe_send_agent_update();
    maybe_send_missing_object_requests();
    maybe_fetch_mesh();
    maybe_fetch_texture();
}
void Circuit::stop() noexcept {
    if(texture_worker_.valid()){
        try{(void)texture_worker_.get();}catch(...){}
    }
    if(!texture_capability_url_.empty()){
        SecureZeroMemory(texture_capability_url_.data(),
                         texture_capability_url_.size()*sizeof(wchar_t));
        texture_capability_url_.clear();
    }
    texture_scheduler_.set_online(false);
    decoded_textures_.clear();
    decoded_texture_bytes_=0U;
    texture_download_count_=texture_failure_count_=0U;
    texture_last_start_={};
    texture_scan_revision_=~std::uint64_t{0};
    texture_last_scan_position_valid_=false;
    texture_last_scan_x_=texture_last_scan_y_=texture_last_scan_z_=0.f;
    if(mesh_worker_.valid()){
        try{(void)mesh_worker_.get();}catch(...){}
    }
    if(!mesh_capability_url_.empty()){
        SecureZeroMemory(mesh_capability_url_.data(),
                         mesh_capability_url_.size()*sizeof(wchar_t));
        mesh_capability_url_.clear();
    }
    reliable_window_.clear();
    loaded_meshes_.clear();
    loaded_mesh_triangle_count_=0U;
    mesh_retry_after_.clear();
    mesh_download_count_=mesh_failure_count_=0U;
    mesh_last_start_={};
    mesh_scan_revision_=~std::uint64_t{0};
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
    nearby_chat_.clear();
    sent_chat_packets_=avatar_appearance_packets_=own_avatar_visual_params_=0;
    last_chat_sent_={};
    last_controls_at_={};
    layer_packets_=object_update_packets_=invalid_world_packets_=0;
    terrain_packets_=wind_packets_=cloud_packets_=unknown_layer_packets_=0;
    decoded_land_packets_=rejected_land_packets_=0;
    land_revision_=0;land_patches_.clear();
    spatial_object_revision_=0;
    spatial_object_frames_=rejected_spatial_frames_=0;
    decoded_full_spatial_frames_=0;
    full_decode_audit_={};
    compressed_decode_audit_={};
    spatial_objects_.clear();
    linked_objects_.clear();
    avatar_root_ids_.clear();
    own_avatar_local_id_=0U;
    mesh_scheduler_.set_online(false);
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
