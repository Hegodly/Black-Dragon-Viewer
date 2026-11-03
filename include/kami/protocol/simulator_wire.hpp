#pragma once
#include "kami/protocol/login.hpp"
#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace kami::protocol::sim {
enum class Inbound : std::uint8_t {
    Invalid, Other, StartPing, RegionHandshake, AgentMovementComplete,
    PacketAck
};
// UUIDs on the LLUDP wire are RFC/network-order bytes, not little-endian
// native Windows GUID structures. Avoid accidental endianness corruption.
[[nodiscard]] bool parse_uuid(std::string_view text,
                              std::array<std::uint8_t,16>& result) noexcept;
[[nodiscard]] bool use_circuit_code(const LoginResult& authorized_login,
                                    std::uint32_t sequence,
                                    lludp::Packet& output) noexcept;
[[nodiscard]] bool complete_agent_movement(const LoginResult& authorized_login,
                                           std::uint32_t sequence,
                                           lludp::Packet& output) noexcept;
[[nodiscard]] bool region_handshake_reply(const LoginResult& authorized_login,
                                          std::uint32_t sequence,
                                          lludp::Packet& output) noexcept;
[[nodiscard]] bool packet_ack(std::span<const std::uint32_t> sequences,
                              std::uint32_t sequence,
                              lludp::Packet& output) noexcept;
[[nodiscard]] bool complete_ping(std::uint8_t ping_id,std::uint32_t sequence,
                                 lludp::Packet& output) noexcept;
[[nodiscard]] Inbound classify(const lludp::Packet& packet) noexcept;
[[nodiscard]] bool movement_complete_identifies(
    const lludp::Packet& packet,const LoginResult& authorized_login) noexcept;
} // namespace kami::protocol::sim
