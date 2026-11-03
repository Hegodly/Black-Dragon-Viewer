#include "kami/features/body_physics.hpp"
#include <algorithm>
#include <cmath>

namespace kami::avatar {
namespace {
float cap(float value,float low,float high,float fallback) noexcept {
    return std::isfinite(value)?std::clamp(value,low,high):fallback;
}
RegionPhysics safe(RegionPhysics r) noexcept {
    r.max_effect_m=cap(r.max_effect_m,0.f,.24f,0.f);
    r.spring=cap(r.spring,8.f,150.f,60.f);
    r.gain=cap(r.gain,0.f,3.f,1.f);
    r.damping=cap(r.damping,2.f,35.f,14.f);
    r.mass=cap(r.mass,.3f,4.f,1.f);
    r.gravity=cap(r.gravity,-1.f,1.f,0.f);
    r.drag=cap(r.drag,0.f,1.f,0.f);
    return r;
}
float drive(float phase,float speed,float vertical,float acceleration,
            float landed,int region) noexcept {
    const float frequency=region==0?2.1f:region==1?1.7f:2.3f;
    const float phase_shift=region==2?1.25f:region==1?.6f:0.f;
    const float step=std::sin(phase*frequency+phase_shift)*
        std::clamp(speed/4.4f,0.f,1.f);
    const float jump=std::clamp(vertical/6.f,-1.f,1.f);
    const float mix=region==0?.65f:region==1?.35f:.55f;
    return step*mix+jump*(region==1?.34f:.46f)+
           acceleration*.27f+landed*(region==1?.85f:.65f);
}
} // namespace

PhysicsWearable sanitize(PhysicsWearable value) noexcept {
    value.breast=safe(value.breast);
    value.belly=safe(value.belly);
    value.butt=safe(value.butt);
    return value;
}
PhysicsWearable preview_wearable(BodyProfile preset) noexcept {
    // Presets are original test values and are NOT Second Life wearable
    // slider values. Live wearable data must be loaded by a future adapter.
    switch(preset) {
    case BodyProfile::Off: return {};
    case BodyProfile::Subtle:
        return {{.045f,68.f,.75f,19.f,1.f,0.f,.2f},
                {.035f,52.f,.65f,18.f,1.15f,0.f,.2f},
                {.042f,62.f,.75f,18.f,1.1f,0.f,.2f},true};
    case BodyProfile::Natural:
        return {{.085f,58.f,1.f,13.f,1.f,0.f,.15f},
                {.070f,48.f,.9f,13.f,1.2f,0.f,.15f},
                {.080f,52.f,1.f,13.f,1.1f,0.f,.15f},true};
    case BodyProfile::Lively:
        return {{.145f,43.f,1.5f,9.f,1.f,0.f,.10f},
                {.120f,40.f,1.4f,10.f,1.15f,0.f,.10f},
                {.140f,42.f,1.5f,9.f,1.08f,0.f,.10f},true};
    }
    return {};
}
const wchar_t* profile_label(BodyProfile preset) noexcept {
    switch(preset) {
    case BodyProfile::Off:return L"OFF";
    case BodyProfile::Subtle:return L"SUBTLE";
    case BodyProfile::Natural:return L"NATURAL";
    case BodyProfile::Lively:return L"LIVELY";
    }
    return L"UNKNOWN";
}
BodyPhysics::BodyPhysics() noexcept:wearable_(preview_wearable(profile_)) {}
void BodyPhysics::reset() noexcept {
    values_={};
    for(float& value:velocity_)value=0.f;
    last_speed_=0.f;
    was_grounded_=true;
}
void BodyPhysics::set_profile(BodyProfile preset) noexcept {
    switch(preset) {
    case BodyProfile::Off:case BodyProfile::Subtle:
    case BodyProfile::Natural:case BodyProfile::Lively:break;
    default:preset=BodyProfile::Off;
    }
    profile_=preset;
    wearable_=sanitize(preview_wearable(preset));
    reset();
}
void BodyPhysics::set_wearable(PhysicsWearable wearable) noexcept {
    // Future network integration should pass decoded, permission-safe
    // physics wearable properties here, not guessed data from an avatar.
    wearable_=sanitize(wearable);
    reset();
}
void BodyPhysics::step(State movement,float dt) noexcept {
    if(!wearable_.enabled) {
        values_={};
        for(float& v:velocity_)v=0.f;
        return;
    }
    if(!std::isfinite(dt)||dt<=0.f||!std::isfinite(movement.animation_phase)||
       !std::isfinite(movement.horizontal_speed)||
       !std::isfinite(movement.vertical_velocity))return;
    const float speed=std::clamp(movement.horizontal_speed,0.f,8.f);
    const float jump=std::clamp(movement.vertical_velocity,-10.f,10.f);
    const float acceleration=std::clamp((speed-last_speed_)*1.2f,-1.f,1.f);
    const float landed=movement.grounded&&!was_grounded_?-1.f:0.f;
    last_speed_=speed;
    was_grounded_=movement.grounded;
    const float intent[3]={
        drive(movement.animation_phase,speed,jump,acceleration,landed,0),
        drive(movement.animation_phase,speed,jump,acceleration,landed,1),
        drive(movement.animation_phase,speed,jump,acceleration,landed,2)
    };
    const RegionPhysics* configs[]={&wearable_.breast,&wearable_.belly,&wearable_.butt};
    float* offsets[]={&values_.breast_y,&values_.belly_y,&values_.butt_y};
    float remaining=std::min(dt,.10f);
    while(remaining>0.000001f) {
        const float step=std::min(remaining,.008f);
        remaining-=step;
        for(int i=0;i<3;++i) {
            const auto& r=*configs[i];
            const float target=std::clamp(
                (intent[i]*r.gain-r.gravity*.18f)*r.max_effect_m,
                -r.max_effect_m,r.max_effect_m);
            const float acceleration_y=
                (r.spring*(target-*offsets[i])-
                 (r.damping+r.drag*12.f)*velocity_[i])/r.mass;
            velocity_[i]+=acceleration_y*step;
            *offsets[i]+=velocity_[i]*step;
            if(*offsets[i]>r.max_effect_m) {
                *offsets[i]=r.max_effect_m;
                velocity_[i]=std::min(velocity_[i],0.f);
            } else if(*offsets[i]<-r.max_effect_m) {
                *offsets[i]=-r.max_effect_m;
                velocity_[i]=std::max(velocity_[i],0.f);
            }
        }
    }
}
} // namespace kami::avatar
