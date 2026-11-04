#pragma once
#include <cstdint>
#include <span>
#include <string>

namespace kami::protocol::mesh {
// Extract a confirmed SL mesh UUID from ObjectUpdate ExtraParams,
// not a mesh file or renderable geometry. Invalid blobs return an empty ID.
// ExtraParams use LLDataPacker: count U8, then param type U16,
// binary-data length U32, followed by bounded bytes.
[[nodiscard]] std::string mesh_asset_from_extras(
    std::span<const std::uint8_t> extras) noexcept;
} // namespace kami::protocol::mesh
