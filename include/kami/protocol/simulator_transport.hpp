#pragma once
#include "kami/protocol/login.hpp"
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/terrain.hpp"
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/live_chat.hpp"
#include "kami/protocol/avatar_appearance.hpp"
#include "kami/protocol/load_scheduler.hpp"
#include "kami/protocol/mesh_lod.hpp"
#include "kami/protocol/caps.hpp"
#include <winsock2.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <future>
#include <vector>
#include <string_view>

namespace kami::protocol::sim {
// A live WinSock *UDP circuit transport*; not a complete in-world
// session. R10 sends neutral AgentUpdate interest-list heartbeats;
// event body dispatch, live assets and world rendering are still absent.
enum class CircuitState : std::uint8_t {
    Closed, WaitingForSimulator, SimulatorResponded,
    RegionHandshakeReceived, MovementConfirmed, Failed
};
class Circuit final {
public:
    Circuit()=default;
    Circuit(const Circuit&)=delete;
    Circuit& operator=(const Circuit&)=delete;
    ~Circuit(){stop();}
    [[nodiscard]] bool start(const LoginResult& accepted_login) noexcept;
    void poll(unsigned packet_budget=12U) noexcept;
    // UI must refresh controls; after 300ms they automatically release.
    void set_requested_controls(std::uint32_t flags) noexcept;
    // Only the connected simulator can send/receive nearby chat.
    [[nodiscard]] bool send_nearby_chat(std::wstring_view text) noexcept;
    [[nodiscard]] const std::vector<ChatLine>& nearby_chat() const noexcept {
        return nearby_chat_;
    }
    [[nodiscard]] std::uint32_t sent_chat_packets() const noexcept {
        return sent_chat_packets_;
    }
    [[nodiscard]] std::uint32_t own_avatar_visual_params() const noexcept {
        return own_avatar_visual_params_;
    }
    [[nodiscard]] std::uint32_t avatar_appearance_packets() const noexcept {
        return avatar_appearance_packets_;
    }
    [[nodiscard]] std::uint32_t last_controls_sent() const noexcept {
        return last_controls_sent_;
    }
    // Counts nonzero AgentUpdate controls actually sent, not movement
    // accepted or authoritative pose replicated by the simulator.
    [[nodiscard]] std::uint32_t movement_control_packets_sent() const noexcept {
        return movement_control_packets_sent_;
    }
    void stop() noexcept;
    [[nodiscard]] CircuitState state() const noexcept {return state_;}
    [[nodiscard]] bool region_seen() const noexcept {return region_seen_;}
    [[nodiscard]] bool movement_seen() const noexcept {return movement_seen_;}
    [[nodiscard]] const RegionMetadata& region_metadata() const noexcept {
        return region_metadata_;
    }
    [[nodiscard]] const AgentPosition& movement_position() const noexcept {
        return movement_position_;
    }
    [[nodiscard]] bool simulator_responded() const noexcept {return packets_received_>0;}
    [[nodiscard]] std::uint32_t packets_received() const noexcept {
        return packets_received_;
    }
    [[nodiscard]] std::uint32_t agent_updates_sent() const noexcept {
        return agent_updates_sent_;
    }
    [[nodiscard]] std::uint32_t layer_packets() const noexcept {
        return layer_packets_;
    }
    [[nodiscard]] std::uint32_t object_update_packets() const noexcept {
        return object_update_packets_;
    }
    [[nodiscard]] std::uint32_t terrain_packets() const noexcept {return terrain_packets_;}
    [[nodiscard]] std::uint32_t decoded_land_packets() const noexcept {
        return decoded_land_packets_;
    }
    [[nodiscard]] std::uint32_t rejected_land_packets() const noexcept {
        return rejected_land_packets_;
    }
    [[nodiscard]] std::uint64_t land_revision() const noexcept {
        return land_revision_;
    }
    [[nodiscard]] std::uint64_t spatial_object_revision() const noexcept {
        return spatial_object_revision_;
    }
    [[nodiscard]] const std::map<std::uint32_t,objectspatial::Transform>&
    decoded_spatial_objects() const noexcept {return spatial_objects_;}
    [[nodiscard]] const std::map<std::uint32_t,objectspatial::Transform>&
    decoded_linked_objects() const noexcept {return linked_objects_;}
    [[nodiscard]] const std::set<std::uint32_t>& known_avatar_roots() const noexcept {
        return avatar_root_ids_;
    }
    [[nodiscard]] bool set_mesh_capability(std::wstring capability) noexcept;
    [[nodiscard]] const std::map<std::string,mesh::LOD>&
    decoded_mesh_assets() const noexcept {return loaded_meshes_;}
    [[nodiscard]] std::size_t mesh_download_count() const noexcept {
        return mesh_download_count_;
    }
    [[nodiscard]] std::size_t mesh_failure_count() const noexcept {
        return mesh_failure_count_;
    }
    [[nodiscard]] load::Stats mesh_asset_stats() const noexcept {
        return mesh_scheduler_.statistics();
    }
    [[nodiscard]] std::size_t retained_linked_objects() const noexcept {
        return linked_objects_.size();
    }
    [[nodiscard]] std::uint32_t spatial_object_frames() const noexcept {
        return spatial_object_frames_;
    }
    [[nodiscard]] std::uint32_t rejected_spatial_frames() const noexcept {
        return rejected_spatial_frames_;
    }
    [[nodiscard]] std::uint32_t decoded_full_spatial_frames() const noexcept {
        return decoded_full_spatial_frames_;
    }
    [[nodiscard]] const objectspatial::FullDecodeAudit&
    full_decode_audit() const noexcept {return full_decode_audit_;}
    [[nodiscard]] std::size_t decoded_root_count() const noexcept {
        return spatial_objects_.size();
    }
    [[nodiscard]] std::size_t pending_object_requests() const noexcept {
        return missing_objects_.size();
    }
    [[nodiscard]] std::uint32_t requested_object_ids() const noexcept {
        return requested_object_ids_;
    }
    [[nodiscard]] std::uint32_t cached_object_ids() const noexcept {
        return cached_object_ids_;
    }
    [[nodiscard]] const std::map<std::uint16_t,terrain::Patch>&
    decoded_land_patches() const noexcept {return land_patches_;}
    [[nodiscard]] std::uint32_t wind_packets() const noexcept {return wind_packets_;}
    [[nodiscard]] std::uint32_t cloud_packets() const noexcept {return cloud_packets_;}
    [[nodiscard]] std::uint32_t unknown_layer_packets() const noexcept {
        return unknown_layer_packets_;
    }
    [[nodiscard]] std::uint32_t full_object_packets() const noexcept {
        return object_full_packets_;
    }
    [[nodiscard]] std::uint32_t compressed_object_packets() const noexcept {
        return object_compressed_packets_;
    }
    [[nodiscard]] std::uint32_t cached_object_packets() const noexcept {
        return object_cached_packets_;
    }
    [[nodiscard]] std::uint32_t terse_object_packets() const noexcept {
        return object_terse_packets_;
    }
    [[nodiscard]] std::uint32_t killed_object_packets() const noexcept {
        return object_killed_packets_;
    }
    [[nodiscard]] std::uint32_t dropped_invalid_world_packets() const noexcept {
        return invalid_world_packets_;
    }
    [[nodiscard]] std::wstring diagnostic() const;
private:
    [[nodiscard]] bool send_packet(lludp::Packet packet) noexcept;
    [[nodiscard]] bool send_startup_messages() noexcept;
    void maybe_send_agent_update() noexcept;
    void maybe_send_missing_object_requests() noexcept;
    void maybe_fetch_mesh() noexcept;
    void remember_missing_object(std::uint32_t id) noexcept;
    void handle_packet(const lludp::Packet& incoming) noexcept;
    void maybe_retry() noexcept;
    SOCKET socket_=INVALID_SOCKET;
    bool winsock_ready_=false;
    LoginResult login_{};
    RegionMetadata region_metadata_{};
    AgentPosition movement_position_{};
    CircuitState state_=CircuitState::Closed;
    bool region_seen_=false,movement_seen_=false;
    bool use_acked_=false,move_acked_=false,handshake_replied_=false;
    std::uint32_t packets_received_=0;
    std::uint32_t agent_updates_sent_=0;
    std::uint32_t requested_controls_=0,last_controls_sent_=0;
    std::uint32_t movement_control_packets_sent_=0;
    std::vector<ChatLine> nearby_chat_;
    std::uint32_t sent_chat_packets_=0;
    std::uint32_t avatar_appearance_packets_=0,own_avatar_visual_params_=0;
    std::chrono::steady_clock::time_point last_chat_sent_{};
    std::chrono::steady_clock::time_point last_controls_at_{};
    std::uint32_t layer_packets_=0;
    std::uint32_t terrain_packets_=0,wind_packets_=0;
    std::uint32_t decoded_land_packets_=0,rejected_land_packets_=0;
    std::uint64_t land_revision_=0;
    std::map<std::uint16_t,terrain::Patch> land_patches_;
    // Untextured root volume positions/sizes from verified connected
    // ObjectUpdateCompressed frames, NOT actual mesh or texture assets.
    std::map<std::uint32_t,objectspatial::Transform> spatial_objects_;
    // Safe local-space transforms kept until their parent region pose
    // is confirmed. Avatar attachments stay unresolved and invisible.
    std::map<std::uint32_t,objectspatial::Transform> linked_objects_;
    std::set<std::uint32_t> avatar_root_ids_; // known, never rendered
    load::Scheduler mesh_scheduler_{};
    std::wstring mesh_capability_url_; // in-memory bearer only
    struct MeshResult {
        std::string id;
        std::uint32_t bytes=0;
        bool fetched=false,decoded=false;
        mesh::LOD lod;
    };
    std::future<MeshResult> mesh_worker_;
    std::map<std::string,mesh::LOD> loaded_meshes_;
    std::size_t mesh_download_count_=0,mesh_failure_count_=0;
    std::chrono::steady_clock::time_point mesh_last_start_{};
    struct MissingObject {
        unsigned attempts=0U;
        std::chrono::steady_clock::time_point sent_at{};
    };
    std::map<std::uint32_t,MissingObject> missing_objects_;
    std::uint32_t requested_object_ids_=0,cached_object_ids_=0;
    std::chrono::steady_clock::time_point last_object_request_{};
    std::uint64_t spatial_object_revision_=0;
    std::uint32_t spatial_object_frames_=0,rejected_spatial_frames_=0;
    std::uint32_t decoded_full_spatial_frames_=0;
    objectspatial::FullDecodeAudit full_decode_audit_{};
    std::uint32_t cloud_packets_=0,unknown_layer_packets_=0;
    std::uint32_t object_update_packets_=0;
    std::uint32_t object_full_packets_=0,object_compressed_packets_=0;
    std::uint32_t object_cached_packets_=0,object_terse_packets_=0;
    std::uint32_t object_killed_packets_=0;
    std::uint32_t invalid_world_packets_=0;
    // Sequences 1/2 are reserved for startup packets.
    std::uint32_t sequence_=3;
    unsigned attempts_=0;
    std::chrono::steady_clock::time_point last_agent_update_{};
    std::chrono::steady_clock::time_point last_retry_{};
    std::chrono::steady_clock::time_point started_{};
};
} // namespace kami::protocol::sim
