#pragma once
#include "kami/features/rig.hpp"
#include "kami/features/body_physics.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace kami::rigphysics {
// Independent six-axis physics model. This is NOT copied from any other viewer.
enum class Axis : std::uint8_t {
    ChestVertical, ChestLateral, ChestDepth,
    ButtVertical, ButtLateral, BellyVertical, Count
};
inline constexpr std::size_t kAxes=static_cast<std::size_t>(Axis::Count);
struct AxisSettings {
    float max_effect_m=0.f,spring=50.f,gain=1.f,damping=13.f;
    float mass=1.f,gravity=0.f,drag=0.f;
};
struct Wearable {
    std::array<AxisSettings,kAxes> settings{};
    bool enabled=false;
};
struct NamedParameter {
    std::string_view name;
    float value=0.f;
};
// This boundary accepts validated, already-decoded named values. It is NOT an
// SL asset parser or a promise that normalized raw SL weights use these units.
[[nodiscard]] bool decode_named_parameters(
    std::span<const NamedParameter> fields,Wearable& output) noexcept;
[[nodiscard]] Wearable from_profile(avatar::BodyProfile profile) noexcept;
[[nodiscard]] Wearable sanitize(Wearable input) noexcept;
struct Output {
    std::array<float,kAxes> offsets{};
    [[nodiscard]] float at(Axis axis) const noexcept {
        return offsets[static_cast<std::size_t>(axis)];
    }
};
struct Budget {
    unsigned interval_frames=1;
    bool active=true;
};
[[nodiscard]] Budget budget_for(float camera_distance_m,
                                bool own_avatar) noexcept;
[[nodiscard]] rig::Pose build_pose(const Output& offsets) noexcept;
class Solver final {
public:
    Solver() noexcept;
    void set_profile(avatar::BodyProfile profile) noexcept;
    void set_wearable(Wearable wearable) noexcept;
    void reset() noexcept;
    void step(avatar::State motion,float dt) noexcept;
    [[nodiscard]] const Wearable& wearable() const noexcept {return wearable_;}
    [[nodiscard]] Output output() const noexcept {return output_;}
    [[nodiscard]] avatar::BodyProfile profile() const noexcept {return profile_;}
private:
    Wearable wearable_{};
    Output output_{};
    std::array<float,kAxes> velocity_{};
    avatar::BodyProfile profile_=avatar::BodyProfile::Natural;
    float previous_speed_=0.f,previous_heading_=0.f;
    bool previous_grounded_=true;
};
} // namespace kami::rigphysics
