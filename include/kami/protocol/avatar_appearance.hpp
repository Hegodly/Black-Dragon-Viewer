#pragma once
#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>

namespace kami::protocol::appearance {
// Bounded metadata from an official Low 158 AvatarAppearance packet.
// This is NOT an avatar skin, mesh, rig, or complete outfit decoder.
struct Observation {
    std::array<std::uint8_t,16> avatar_id{};
    unsigned visual_parameter_count=0;
    bool texture_entry_present=false;
    bool trial=false;
    bool extra_data_present=false;
};
[[nodiscard]] bool decode_avatar_appearance(const lludp::Packet& packet,
                                             Observation& out) noexcept;
} // namespace kami::protocol::appearance
