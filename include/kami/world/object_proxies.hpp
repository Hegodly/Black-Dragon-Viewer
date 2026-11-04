#pragma once
#include "kami/world/scene.hpp"
#include "kami/protocol/object_spatial.hpp"
#include <cstdint>
#include <map>
#include <cstddef>

namespace kami::world {
// Spatial proxies are amber frame boxes centered/scaled from REAL received
// simulator transforms. They are NOT prim profiles, meshes or textures.
[[nodiscard]] SceneMesh build_live_object_spatial_proxies(
    const std::map<std::uint32_t,protocol::objectspatial::Transform>& objects,
    float agent_x,float agent_y,float agent_z,
    std::size_t max_proxies=64U);
} // namespace kami::world
