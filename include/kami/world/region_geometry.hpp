#pragma once
#include "kami/world/scene.hpp"
#include <cstdint>

namespace kami::protocol { class Bootstrap; }
namespace kami::world {

// Only *decoded*, permission-checked region triangles enter this stage.
// This is a DX12 upload boundary, not an SL mesh/texture asset decoder.
enum class RegionGeometryError : std::uint8_t {
    None, OfflineOrUnverified, InvalidSections, TooLarge,
    InvalidPosition, InvalidNormal, InvalidMaterial, UnsupportedSkinning
};
[[nodiscard]] RegionGeometryError validate_decoded_region(
    const protocol::Bootstrap& session,
    const SceneMesh& mesh,
    bool verified_region_data) noexcept;
[[nodiscard]] const wchar_t* region_geometry_error(
    RegionGeometryError error) noexcept;
} // namespace kami::world
