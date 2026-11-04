#pragma once
#include "kami/protocol/lludp.hpp"
#include <cstdint>
#include <vector>

namespace kami::protocol::objectspatial {
// Bounded transform-only subsets of ObjectUpdate and ObjectUpdateCompressed.
// A record is NOT a decoded prim shape, mesh, material, or avatar.
struct Transform {
    std::uint32_t local_id=0;
    float x=0.f,y=0.f,z=0.f;
    float sx=0.f,sy=0.f,sz=0.f;
};
// The full (High 12) message uses individual message-template fields,
// unlike the packed compressed (High 13) message.
[[nodiscard]] bool decode_full_transforms(
    const lludp::Packet& packet,std::vector<Transform>& out) noexcept;
[[nodiscard]] bool decode_compressed_transforms(
    const lludp::Packet& packet,std::vector<Transform>& out) noexcept;
[[nodiscard]] bool decode_killed_ids(
    const lludp::Packet& packet,std::vector<std::uint32_t>& out) noexcept;
} // namespace kami::protocol::objectspatial
