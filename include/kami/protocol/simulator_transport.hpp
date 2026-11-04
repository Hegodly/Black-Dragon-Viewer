#pragma once
#include "kami/protocol/login.hpp"
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include "kami/protocol/terrain.hpp"
#include <winsock2.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <map>

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
    std::chrono::steady_clock::time_point last_controls_at_{};
    std::uint32_t layer_packets_=0;
    std::uint32_t terrain_packets_=0,wind_packets_=0;
    std::uint32_t decoded_land_packets_=0,rejected_land_packets_=0;
    std::uint64_t land_revision_=0;
    std::map<std::uint16_t,terrain::Patch> land_patches_;
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
