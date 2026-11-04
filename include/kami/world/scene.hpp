#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace kami::world {
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
    float metallic, roughness;
    float u, v;
    float body_region=0.f; // legacy local preview; rigged vertices use weights
    std::uint32_t bone_indices=0;
    std::uint32_t bone_weights=0; // packed normalized UNORM8 x4; zero=unrigged
};
struct SceneMesh {
    std::vector<Vertex> vertices;
    std::size_t terrain_count = 0;
    std::size_t avatar_count = 0;
};

// Original procedural test assets, not Second Life content.
[[nodiscard]] SceneMesh build_test_scene(unsigned divisions = 32);
// R9: a flat, original DX12 mesh positioned from verified SL water
// elevation relative to a session-verified avatar elevation. This is
// NOT a terrain decoder, mesh asset, or water texture simulation.
[[nodiscard]] SceneMesh build_live_water_preview(float relative_water_y);
[[nodiscard]] float sample_test_terrain_height(float x, float z) noexcept;
} // namespace kami::world
