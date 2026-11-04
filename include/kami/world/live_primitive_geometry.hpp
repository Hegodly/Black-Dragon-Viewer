#pragma once
#include "kami/protocol/object_spatial.hpp"
#include "kami/world/scene.hpp"
#include <cstddef>
#include <cstdint>
#include <map>

namespace kami::world {
struct PrimitiveBatch {
    SceneMesh scene{};
    std::size_t root_shapes=0,linked_shapes=0;
    // Eligibility diagnostics for live data; no UUIDs or simulator IDs.
    std::size_t eligible_shapes=0,unresolved_parent_shapes=0;
    std::size_t out_of_range_shapes=0,invalid_pose_shapes=0;
    std::size_t budget_deferred_shapes=0;
};
// Finite, server-originated *untextured* simple primitive previews.
// Root transforms must be region-space; linked transforms are composed
// only after their root (and any intermediate linked parent) is verified.
// Mesh/sculpt/cut/hollow primitives must NOT be replaced by fake geometry.
[[nodiscard]] PrimitiveBatch build_live_primitive_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    float agent_x,float agent_y,float agent_z,
    std::size_t limit=64U);
} // namespace kami::world
