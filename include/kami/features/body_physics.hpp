#pragma once
#include "kami/features/avatar_motion.hpp"
#include <cstdint>

namespace kami::avatar {
// Independent, local body-physics preview. Not Second Life wearable parsing.
// Units are meters, seconds and dimensionless spring/gain controls.
enum class BodyProfile : std::uint8_t { Off, Subtle, Natural, Lively };
struct RegionPhysics {
    float max_effect_m=0.f; // hard limit on local visual deformation
    float spring=60.f;
    float gain=1.f;
    float damping=14.f;
    float mass=1.f;
    float gravity=0.f;
    float drag=0.f;
};
struct PhysicsWearable {
    RegionPhysics breast{};
    RegionPhysics belly{};
    RegionPhysics butt{};
    bool enabled=false;
};
struct PhysicsOffsets {
    float breast_y=0.f;
    float belly_y=0.f;
    float butt_y=0.f;
};
[[nodiscard]] PhysicsWearable preview_wearable(BodyProfile preset) noexcept;
[[nodiscard]] PhysicsWearable sanitize(PhysicsWearable input) noexcept;
[[nodiscard]] const wchar_t* profile_label(BodyProfile preset) noexcept;

class BodyPhysics final {
public:
    BodyPhysics() noexcept;
    void set_profile(BodyProfile preset) noexcept;
    void set_wearable(PhysicsWearable wearable) noexcept;
    [[nodiscard]] BodyProfile profile() const noexcept {return profile_;}
    [[nodiscard]] const PhysicsWearable& wearable() const noexcept {return wearable_;}
    [[nodiscard]] PhysicsOffsets offsets() const noexcept {return values_;}
    void reset() noexcept;
    void step(State movement,float dt) noexcept;
private:
    BodyProfile profile_=BodyProfile::Natural;
    PhysicsWearable wearable_{};
    PhysicsOffsets values_{};
    float velocity_[3]{};
    float last_speed_=0.f;
    bool was_grounded_=true;
};
} // namespace kami::avatar
