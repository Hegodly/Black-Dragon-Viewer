#include "kami/features/physics.hpp"
#include <algorithm>
#include <cmath>
namespace kami::physics {
void Spring::reset(float value) noexcept {
    position_=std::isfinite(value)?value:0.f;
    velocity_=0.f;
}
void Spring::step(float target,float delta_seconds) noexcept {
    if(!std::isfinite(target)||!std::isfinite(delta_seconds)||delta_seconds<=0) return;
    // Fixed-size substeps keep the motion stable at high or low FPS.
    float remaining=std::min(delta_seconds,.10f);
    while(remaining>0.f) {
        const float dt=std::min(.008f,remaining);
        remaining-=dt;
        const float acceleration=52.f*(target-position_)-13.5f*velocity_;
        velocity_+=acceleration*dt;
        position_+=velocity_*dt;
    }
}
} // namespace kami::physics
