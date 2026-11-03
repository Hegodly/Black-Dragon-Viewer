#pragma once
#include <cstddef>
#include <vector>

namespace kami::world {
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
    float metallic, roughness;
    float u, v;
    float body_region=0.f; // 0=none; local marker 1=breast, 2=belly, 3=butt
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
