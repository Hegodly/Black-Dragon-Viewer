#pragma once
#include "kami/protocol/login.hpp"
#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kami::protocol::sim {
enum class Inbound : std::uint8_t {
    Invalid, Other, StartPing, RegionHandshake, AgentMovementComplete,
    PacketAck
};
// Real simulator-sourced data, not a procedural terrain/asset representation.
// Sanitized to printable ASCII for the diagnostic UI; no raw packet logging.
struct RegionMetadata {
    std::string name;
    float water_height=0.f; // SL region absolute Z in meters
    bool verified=false;
};
struct AgentPosition {
    float x=0.f,y=0.f,z=0.f; // SL axes, Z is elevation
    bool verified=false;
};
[[nodiscard]] bool decode_agent_movement_position(
    const lludp::Packet& packet,const LoginResult& authorized_login,
    AgentPosition& out) noexcept;
[[nodiscard]] bool decode_region_metadata(
    const lludp::Packet& packet,RegionMetadata& out) noexcept;
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
// Public SL control flag subset: no click, edit, teleport or fly bits.
inline constexpr std::uint32_t kAtPos=0x00000001U;
inline constexpr std::uint32_t kAtNeg=0x00000002U;
inline constexpr std::uint32_t kLeftPos=0x00000004U;
inline constexpr std::uint32_t kLeftNeg=0x00000008U;
inline constexpr std::uint32_t kUpPos=0x00000010U;
inline constexpr std::uint32_t kTurnLeft=0x02000000U;
inline constexpr std::uint32_t kTurnRight=0x04000000U;
inline constexpr std::uint32_t kFastAt=0x00000400U;
inline constexpr std::uint32_t kAllowedMovement=
    kAtPos|kAtNeg|kLeftPos|kLeftNeg|kUpPos|kTurnLeft|kTurnRight|kFastAt;
[[nodiscard]] std::uint32_t movement_flags(
    bool forward,bool backward,bool right,bool left,bool turn_right,
    bool turn_left,bool run,bool jump) noexcept;
[[nodiscard]] bool controlled_agent_update(const LoginResult& authorized_login,
                                           const AgentPosition& position,
                                           std::uint32_t sequence,
                                           lludp::Packet& output,
                                           std::uint32_t control_flags,
                                           float draw_distance=96.f) noexcept;
// Authenticated neutral AgentUpdate High 4 to request simulator interest.
[[nodiscard]] bool neutral_agent_update(const LoginResult& authorized_login,
                                        const AgentPosition& position,
                                        std::uint32_t sequence,
                                        lludp::Packet& output,
                                        float draw_distance=96.f) noexcept;
// Second Life Medium 3 RequestMultipleObjects, CacheMissType=0 only.
// Requests data; does NOT manipulate an object's world state.
[[nodiscard]] bool request_missing_objects(
    const LoginResult& authorized_login,
    std::span<const std::uint32_t> local_ids,
    std::uint32_t sequence,lludp::Packet& output) noexcept;
[[nodiscard]] bool packet_ack(std::span<const std::uint32_t> sequences,
                              std::uint32_t sequence,
                              lludp::Packet& output) noexcept;
[[nodiscard]] bool complete_ping(std::uint8_t ping_id,std::uint32_t sequence,
                                 lludp::Packet& output) noexcept;
[[nodiscard]] Inbound classify(const lludp::Packet& packet) noexcept;
[[nodiscard]] bool movement_complete_identifies(
    const lludp::Packet& packet,const LoginResult& authorized_login) noexcept;
} // namespace kami::protocol::sim
