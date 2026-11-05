#pragma once
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/mesh_lod.hpp"
#include "kami/world/scene.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace kami::world {
struct PrimitiveBatch {
    SceneMesh scene{};
    std::size_t root_shapes=0,linked_shapes=0;
    // Eligibility diagnostics for live data; no UUIDs or simulator IDs.
    std::size_t eligible_shapes=0,unresolved_parent_shapes=0;
    std::size_t out_of_range_shapes=0,invalid_pose_shapes=0;
    std::size_t budget_deferred_shapes=0;
    // Every unresolved supported child counted exactly once.
    std::size_t missing_anchor_shapes=0,avatar_attachment_shapes=0;
    std::size_t cycle_shapes=0,invalid_parent_shapes=0;
    std::size_t deep_parent_shapes=0,unique_missing_ids=0;
};
// Finite, server-originated *untextured* simple primitive previews.
// Root transforms must be region-space; linked transforms are composed
// only after their root (and any intermediate linked parent) is verified.
// Mesh/sculpt/cut/hollow primitives must NOT be replaced by fake geometry.
[[nodiscard]] PrimitiveBatch build_live_primitive_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    float agent_x,float agent_y,float agent_z,
    std::size_t limit=128U,
    const std::set<std::uint32_t>& avatar_roots={});
struct MeshBatch {
    SceneMesh scene{};
    std::size_t root_meshes=0,linked_meshes=0;
    std::size_t emitted_triangles=0,waiting_for_parent=0;
    std::size_t textured_triangles=0; // authentic default TE + decoded mesh UV
};
// Render ONLY vertices decoded from real mesh assets (no surrogate boxes).
// Shared SL-to-DX12 transforms with the validated prim hierarchy builder.
[[nodiscard]] MeshBatch build_live_mesh_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,protocol::mesh::LOD>& loaded,
    float agent_x,float agent_y,float agent_z,
    std::size_t max_triangles=18000U,
    const std::map<std::string,std::uint16_t>& texture_layers={});
} // namespace kami::world
