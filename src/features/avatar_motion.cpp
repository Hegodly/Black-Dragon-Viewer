#include "kami/features/avatar_motion.hpp"
#include <algorithm>
#include <cmath>

namespace kami::avatar {
namespace {
constexpr float kPi=3.14159265358979323846f;
float ground(TerrainHeight sampler,float x,float z) noexcept {
    if(!sampler)return 0.f;
    const float result=sampler(x,z);
    return std::isfinite(result)&&result>-100.f?result:0.f;
}
float axis(float x) noexcept {
    return std::isfinite(x)?std::clamp(x,-1.f,1.f):0.f;
}
} // namespace

void Controller::reset(TerrainHeight sampler) noexcept {
    state_={};
    state_.y=ground(sampler,0.f,0.f);
    velocity_x_=velocity_z_=0.f;
    jump_latched_=false;
    controls_={};
}
void Controller::set_controls(Controls controls) noexcept {
    controls.forward=axis(controls.forward);
    controls.strafe=axis(controls.strafe);
    controls.turn=axis(controls.turn);
    controls_=controls;
}
void Controller::step(float delta_seconds,TerrainHeight sampler) noexcept {
    if(!std::isfinite(delta_seconds)||delta_seconds<=0.f)return;
    float remaining=std::min(delta_seconds,0.1f);
    const bool new_jump=controls_.jump&&!jump_latched_;
    jump_latched_=controls_.jump;
    bool jumped=false;
    // 8ms physics substeps keep jumps / contact stable independent of FPS.
    while(remaining>0.000001f) {
        const float dt=std::min(remaining,.008f);
        remaining-=dt;
        state_.heading+=controls_.turn*1.9f*dt;
        if(state_.heading>kPi)state_.heading-=2.f*kPi;
        if(state_.heading<-kPi)state_.heading+=2.f*kPi;
        const float forward=controls_.forward,strafe=controls_.strafe;
        const float magnitude=std::hypot(forward,strafe);
        const float norm=magnitude>1.f?1.f/magnitude:1.f;
        const float target_speed=controls_.run?4.4f:2.3f;
        const float dx=std::sin(state_.heading)*forward+
                       std::cos(state_.heading)*strafe;
        const float dz=std::cos(state_.heading)*forward-
                       std::sin(state_.heading)*strafe;
        const float blend=1.f-std::exp(-11.f*dt);
        velocity_x_+=(dx*norm*target_speed-velocity_x_)*blend;
        velocity_z_+=(dz*norm*target_speed-velocity_z_)*blend;
        if(magnitude<0.001f&&std::abs(velocity_x_)<0.005f)velocity_x_=0.f;
        if(magnitude<0.001f&&std::abs(velocity_z_)<0.005f)velocity_z_=0.f;
        const float x=std::clamp(state_.x+velocity_x_*dt,-13.5f,13.5f);
        const float z=std::clamp(state_.z+velocity_z_*dt,-13.5f,13.5f);
        const float moved=std::hypot(x-state_.x,z-state_.z);
        state_.x=x;state_.z=z;
        if((x<=-13.5f&&velocity_x_<0.f)||(x>=13.5f&&velocity_x_>0.f))
            velocity_x_=0.f;
        if((z<=-13.5f&&velocity_z_<0.f)||(z>=13.5f&&velocity_z_>0.f))
            velocity_z_=0.f;
        const float floor=ground(sampler,x,z);
        if(new_jump&&!jumped&&state_.grounded) {
            state_.vertical_velocity=5.1f;
            state_.grounded=false;
            jumped=true;
        }
        if(!state_.grounded) {
            state_.vertical_velocity-=12.8f*dt;
            state_.y+=state_.vertical_velocity*dt;
            if(state_.y<=floor&&state_.vertical_velocity<=0.f) {
                state_.y=floor;
                state_.vertical_velocity=0.f;
                state_.grounded=true;
            }
        } else {
            state_.y=floor;
            state_.vertical_velocity=0.f;
        }
        state_.horizontal_speed=std::hypot(velocity_x_,velocity_z_);
        state_.animation_phase+=moved*4.3f;
        if(state_.animation_phase>2.f*kPi)
            state_.animation_phase=std::fmod(state_.animation_phase,2.f*kPi);
        const float desired=state_.grounded?
            std::clamp(state_.horizontal_speed/2.3f,0.f,1.35f):0.f;
        state_.gait+=(desired-state_.gait)*(1.f-std::exp(-12.f*dt));
        if(!state_.grounded)state_.motion=
            state_.vertical_velocity>0.f?Motion::Jumping:Motion::Falling;
        else if(state_.horizontal_speed<0.12f)state_.motion=Motion::Idle;
        else state_.motion=controls_.run?Motion::Running:Motion::Walking;
    }
}
const wchar_t* motion_label(Motion state) noexcept {
    switch(state) {
    case Motion::Idle:return L"IDLE";
    case Motion::Walking:return L"WALK";
    case Motion::Running:return L"RUN";
    case Motion::Jumping:return L"JUMP";
    case Motion::Falling:return L"FALL";
    }
    return L"UNKNOWN";
}
} // namespace kami::avatar
