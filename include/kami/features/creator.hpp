#pragma once
#include "kami/world/scene.hpp"
#include <cstddef>
#include <string>

namespace kami::creator {
struct Audit {
    bool valid = false;
    std::size_t triangles = 0;
    std::size_t vertices = 0;
    std::size_t degenerate_triangles = 0;
    std::string problem;
};
// Native geometry preflight for exported mesh data. This is not an uploader,
// and does not claim to override Second Life server validation or limits.
[[nodiscard]] Audit inspect(const world::SceneMesh& mesh) noexcept;
} // namespace kami::creator
