#pragma once
#include "kami/protocol/terrain.hpp"
#include "kami/world/scene.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

namespace kami::world {
struct LiveMaskedWater {
    SceneMesh scene{};
    std::size_t submerged_triangles=0;
    std::size_t shoreline_triangles=0;
};
// Samples only authenticated decoded SL land. A water plane covers
// submerged ground, NOT every patch and not the entire region.
// Shorelines are clipped against interpolated actual terrain corners.
[[nodiscard]] LiveMaskedWater build_live_masked_water(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    const std::vector<const protocol::terrain::Patch*>& selected,
    float agent_x,float agent_y,float agent_z,float water_z,
    std::size_t max_vertices=96000U);
} // namespace kami::world
