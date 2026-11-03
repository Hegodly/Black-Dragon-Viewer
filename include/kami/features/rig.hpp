#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace kami::rig {
// Small original procedural test rig. Names correspond to common fitted-mesh
// concepts, not a complete decoded Second Life/Bento skeleton.
enum class Joint : std::uint8_t {
    Root, Pelvis, Chest, LeftPec, RightPec, Belly, Butt, Head,
    LeftButt, RightButt, Count
};
inline constexpr std::size_t kJointCount=static_cast<std::size_t>(Joint::Count);
inline constexpr std::size_t kInfluences=4;
struct Vec3 {float x=0.f,y=0.f,z=0.f;};
struct Matrix {
    // Row-major, row-vector convention; matches mul(float4(pos,1),M) in HLSL.
    std::array<float,16> elements{};
};
struct JointInfo {
    const char* name="";
    int parent=-1;
    Vec3 rest_local{};
};
[[nodiscard]] const std::array<JointInfo,kJointCount>& joints() noexcept;
[[nodiscard]] Matrix identity() noexcept;
[[nodiscard]] Vec3 transform_position(Matrix m,Vec3 p) noexcept;
[[nodiscard]] Vec3 transform_normal(Matrix m,Vec3 n) noexcept;
struct BonePose {
    Vec3 translation{}; // pose difference from rest, meters
    float yaw=0.f;
    float roll=0.f;
};
using Pose=std::array<BonePose,kJointCount>;
using Palette=std::array<Matrix,kJointCount>;
[[nodiscard]] bool make_palette(const Pose& pose,Palette& output) noexcept;
struct Influences {
    std::array<std::uint8_t,kInfluences> joints{};
    std::array<float,kInfluences> weights{};
};
struct Packed {
    std::uint32_t joints=0;
    std::uint32_t weights=0;
};
[[nodiscard]] bool validate_influences(Influences influence) noexcept;
[[nodiscard]] bool pack_influences(Influences in,Packed& output) noexcept;
[[nodiscard]] bool unpack_influences(Packed packed,Influences& output) noexcept;
[[nodiscard]] bool deform(Vec3 original,Vec3 normal,Packed packed,
                          const Palette& palette,
                          Vec3& position_out,Vec3& normal_out) noexcept;
} // namespace kami::rig
