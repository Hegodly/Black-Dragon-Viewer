#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace kami::protocol::mesh {
struct Triangle {
    std::array<float,9> pos{}; // 3 SL local XYZ positions
    std::array<float,6> uv{};  // 3 UV pairs (untextured for now)
};
struct LOD {
    std::vector<Triangle> triangles;
    std::size_t decoded_bytes=0;
};
// Validates binary LLSD asset header, LoD offsets, zlib/gzip payload,
// mesh vertex domains and triangle indices. Finite, static geometry only.
// This does not decode avatar rig weights, PBR materials, or textures.
[[nodiscard]] bool decode_mesh_lod(
    std::span<const std::uint8_t> asset,LOD& out) noexcept;
} // namespace kami::protocol::mesh
