#pragma once
#include "kami/world/scene.hpp"
#include "kami/features/rig.hpp"
#include <array>
#include <cstddef>
#include <span>

namespace kami::rig {
enum class Compatibility { Unrigged, Partial, PhysicsReady, Invalid };
struct Audit {
    Compatibility status=Compatibility::Unrigged;
    std::array<std::size_t,kJointCount> joint_affected_vertices{};
    std::size_t vertex_count=0;
    std::size_t weighted_vertices=0;
    std::size_t corrupt_vertices=0;
    bool left_pec=false,right_pec=false,belly=false,butt=false;
};
[[nodiscard]] Audit inspect(std::span<const world::Vertex> vertices) noexcept;
[[nodiscard]] const wchar_t* compatibility_label(Compatibility status) noexcept;
} // namespace kami::rig
