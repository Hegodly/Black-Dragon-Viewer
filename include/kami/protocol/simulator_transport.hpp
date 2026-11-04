#pragma once
#include "kami/protocol/login.hpp"
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <winsock2.h>
#include <chrono>
#include <cstdint>
#include <string>

namespace kami::protocol::sim {
// A live WinSock *UDP circuit transport*; not a complete in-world
// session. R6 negotiates initial CAPS separately; event dispatch,
// AgentUpdate, live assets and world rendering are still absent.
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
    void stop() noexcept;
    [[nodiscard]] CircuitState state() const noexcept {return state_;}
    [[nodiscard]] bool region_seen() const noexcept {return region_seen_;}
    [[nodiscard]] bool movement_seen() const noexcept {return movement_seen_;}
    [[nodiscard]] const RegionMetadata& region_metadata() const noexcept {
        return region_metadata_;
    }
    [[nodiscard]] bool simulator_responded() const noexcept {return packets_received_>0;}
    [[nodiscard]] std::uint32_t packets_received() const noexcept {
        return packets_received_;
    }
    [[nodiscard]] std::uint32_t layer_packets() const noexcept {
        return layer_packets_;
    }
    [[nodiscard]] std::uint32_t object_update_packets() const noexcept {
        return object_update_packets_;
    }
    [[nodiscard]] std::uint32_t dropped_invalid_world_packets() const noexcept {
        return invalid_world_packets_;
    }
    [[nodiscard]] std::wstring diagnostic() const;
private:
    [[nodiscard]] bool send_packet(lludp::Packet packet) noexcept;
    [[nodiscard]] bool send_startup_messages() noexcept;
    void handle_packet(const lludp::Packet& incoming) noexcept;
    void maybe_retry() noexcept;
    SOCKET socket_=INVALID_SOCKET;
    bool winsock_ready_=false;
    LoginResult login_{};
    RegionMetadata region_metadata_{};
    CircuitState state_=CircuitState::Closed;
    bool region_seen_=false,movement_seen_=false;
    bool use_acked_=false,move_acked_=false,handshake_replied_=false;
    std::uint32_t packets_received_=0;
    std::uint32_t layer_packets_=0;
    std::uint32_t object_update_packets_=0;
    std::uint32_t invalid_world_packets_=0;
    std::uint32_t sequence_=1;
    unsigned attempts_=0;
    std::chrono::steady_clock::time_point last_retry_{};
    std::chrono::steady_clock::time_point started_{};
};
} // namespace kami::protocol::sim
