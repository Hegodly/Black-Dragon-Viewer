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
[[nodiscard]] float sample_test_terrain_height(float x, float z) noexcept;
} // namespace kami::world
