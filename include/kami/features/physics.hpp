#pragma once
namespace kami::physics {
// Deterministic procedural avatar-preview spring; no SL skeleton simulation yet.
class Spring final {
public:
    void reset(float value=0.f) noexcept;
    void step(float target,float delta_seconds) noexcept;
    [[nodiscard]] float position() const noexcept {return position_;}
    [[nodiscard]] float velocity() const noexcept {return velocity_;}
private:
    float position_=0.f,velocity_=0.f;
};
} // namespace kami::physics
