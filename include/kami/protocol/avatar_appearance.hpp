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
    std::uint16_t texture_entry_size=0;
    std::array<std::uint8_t,4096> texture_entry{};
    std::array<std::uint8_t,255> visual_parameters{};
};
[[nodiscard]] bool decode_avatar_appearance(const lludp::Packet& packet,
                                             Observation& out) noexcept;
} // namespace kami::protocol::appearance
