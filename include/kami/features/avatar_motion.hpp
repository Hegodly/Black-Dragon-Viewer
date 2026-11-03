#pragma once
#include <cstdint>

namespace kami::avatar {
// Independent offline locomotion state. The Second Life simulator remains
// authoritative once network replication is implemented.
enum class Motion : std::uint8_t { Idle, Walking, Running, Jumping, Falling };
struct Controls {
    float forward=0.f; // -1 to 1
    float strafe=0.f;  // -1 to 1
    float turn=0.f;    // -1 to 1; positive rotates right
    bool run=false;
    bool jump=false;
};
struct State {
    float x=0.f, y=0.f, z=0.f;
    float heading=0.f;
    float horizontal_speed=0.f;
    float vertical_velocity=0.f;
    float animation_phase=0.f;
    float gait=0.f;
    bool grounded=true;
    Motion motion=Motion::Idle;
};
using TerrainHeight=float (*)(float,float);
class Controller final {
public:
    void reset(TerrainHeight terrain) noexcept;
    void set_controls(Controls controls) noexcept;
    void step(float delta_seconds,TerrainHeight terrain) noexcept;
    [[nodiscard]] const State& state() const noexcept {return state_;}
    [[nodiscard]] Controls controls() const noexcept {return controls_;}
private:
    State state_{};
    Controls controls_{};
    float velocity_x_=0.f,velocity_z_=0.f;
    bool jump_latched_=false;
};
[[nodiscard]] const wchar_t* motion_label(Motion motion) noexcept;
} // namespace kami::avatar
