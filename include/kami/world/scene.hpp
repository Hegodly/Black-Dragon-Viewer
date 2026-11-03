#pragma once
#include <cstddef>
#include <vector>

namespace kami::world {
struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
};
struct SceneMesh {
    std::vector<Vertex> vertices;
    std::size_t terrain_count = 0;
    std::size_t avatar_count = 0;
};

// Original procedural test assets, not Second Life content.
[[nodiscard]] SceneMesh build_test_scene(unsigned divisions = 32);
} // namespace kami::world
