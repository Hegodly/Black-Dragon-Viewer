#pragma once
#include "kami/protocol/texture_budget.hpp"
#include "kami/protocol/object_spatial.hpp"
#include "kami/protocol/mesh_lod.hpp"
#include "kami/protocol/texture_decode.hpp"
#include "kami/world/scene.hpp"
#include "kami/world/pbr_material_table.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace kami::world {
// Hard upper bound for the Quality live-region primitive draw budget.
inline constexpr std::size_t kMaxQualityPrimitiveShapes=2048U;
// The live DX12 region upload includes land, water, proxies, meshes,
// simple primitives and an optional avatar marker.
inline constexpr std::size_t kLiveSceneVertexLimit=4000000U;
inline constexpr std::size_t kLiveAvatarVertexReserve=1000U;
[[nodiscard]] inline std::size_t live_world_vertices_remaining(
    std::size_t already_used) noexcept {
    constexpr auto usable=kLiveSceneVertexLimit-kLiveAvatarVertexReserve;
    return already_used<usable?usable-already_used:0U;
}
struct PrimitiveBatch {
    SceneMesh scene{};
    std::size_t root_shapes=0,linked_shapes=0;
    std::size_t textured_triangles=0U;
    // Eligibility diagnostics for live data; no UUIDs or simulator IDs.
    std::size_t eligible_shapes=0,unresolved_parent_shapes=0;
    std::size_t out_of_range_shapes=0,invalid_pose_shapes=0;
    std::size_t budget_deferred_shapes=0;
    // Every unresolved supported child counted exactly once.
    std::size_t missing_anchor_shapes=0,avatar_attachment_shapes=0;
    std::size_t cycle_shapes=0,invalid_parent_shapes=0;
    std::size_t deep_parent_shapes=0,unique_missing_ids=0;
};
// Finite, server-originated simple prim geometry with TE diffuse textures.
// Root transforms must be region-space; linked transforms are composed
// only after their root (and any intermediate linked parent) is verified.
// Mesh/sculpt/cut/hollow primitives must NOT be replaced by fake geometry.
[[nodiscard]] PrimitiveBatch build_live_primitive_geometry(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    float agent_x,float agent_y,float agent_z,
    std::size_t limit=128U,
    const std::set<std::uint32_t>& avatar_roots={},
    const std::map<std::string,std::uint16_t>& texture_layers={},
    const PBRFaceIndices& pbr_faces={},
    std::size_t max_vertices=4000000U);
// Select decoded JPEG2000 image IDs actually used by visible static mesh
// AND supported simple prim faces. Prioritize distance, not UUID ordering of
// the CPU cache. Preserve existing DX12 array slice numbers when possible
// to avoid needless rebinds. Never select missing/invalid image pixels.
[[nodiscard]] std::map<std::string,std::uint16_t>
select_visible_mesh_texture_layers(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& roots,
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& children,
    const std::set<std::uint32_t>& avatar_roots,
    const std::map<std::string,protocol::mesh::LOD>& loaded,
    const std::map<std::string,protocol::texture::DecodedTexture>& decoded,
    float agent_x,float agent_y,float agent_z,
    std::size_t max_layers=protocol::texture::kGPUImageLayers,
    const std::map<std::string,std::uint16_t>& previous={},
    const protocol::pbr::Materials& materials={},const protocol::pbr::Overrides& overrides={},
    const std::vector<std::string>& extra_images={});

struct MeshBatch {
    SceneMesh scene{};
    std::size_t root_meshes=0,linked_meshes=0;
    std::size_t emitted_triangles=0,waiting_for_parent=0;
    std::size_t textured_triangles=0; // authentic default TE + decoded mesh UV
    // R66: mutually exclusive classifications for authenticated mesh
    // instances. These metrics distinguish interest/LOD/culling problems
    // from DX12 vertex-budget exhaustion without changing what is drawn.
    std::size_t eligible_instances=0;
    std::size_t missing_lod_instances=0;
    std::size_t out_of_range_instances=0;
    std::size_t invalid_pose_instances=0;
    std::size_t undrawn_eligible_instances=0;
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
    const std::map<std::string,std::uint16_t>& texture_layers={},
    const PBRFaceIndices& pbr_faces={});
} // namespace kami::world

