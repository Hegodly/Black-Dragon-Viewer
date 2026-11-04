#pragma once
#include "kami/protocol/terrain.hpp"
#include "kami/world/scene.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>
#include <limits>

namespace kami::world {
struct LiveTerrainMesh {
    SceneMesh scene{};
    std::size_t quads=0;
    std::size_t stitched_quads=0;
    std::size_t skipped_quads=0;
    std::size_t skipped_discontinuity_quads=0;
};
// Builds the selected patches from *actual decoded simulator elevations*.
// A patch holds 16x16 samples but the last sample column has to connect
// to the next patch's first column. Never synthesize absent patch heights.
[[nodiscard]] LiveTerrainMesh build_live_terrain_mesh(
    const std::map<std::uint16_t,protocol::terrain::Patch>& decoded,
    const std::vector<const protocol::terrain::Patch*>& selected,
    float agent_x,float agent_y,float agent_z,
    float verified_water_z=std::numeric_limits<float>::quiet_NaN());
} // namespace kami::world
