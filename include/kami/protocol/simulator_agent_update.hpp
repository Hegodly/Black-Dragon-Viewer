#pragma once
#include "kami/protocol/lludp.hpp"
#include "kami/protocol/login.hpp"
#include <array>
#include <cstdint>

namespace kami::protocol::sim {
struct SimulatorPose {
    std::array<float,3> position{};
    std::array<float,3> look_at{1.f,0.f,0.f};
    bool verified=false;
};
// The source is the authenticated AgentMovementComplete UDP packet.
// KAMI's offline avatar and camera coordinates are never used.
[[nodiscard]] bool decode_movement_pose(
    const lludp::Packet& message,const LoginResult& login,
    SimulatorPose& out) noexcept;

// Linden message_template.msg, AgentUpdate High 4, static neutral controls.
// This does not implement live WASD input or a synced region camera.
[[nodiscard]] bool build_agent_update(
    const LoginResult& login,const SimulatorPose& pose,
    std::uint32_t seq,lludp::Packet& out) noexcept;
}
