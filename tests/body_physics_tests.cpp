#include "kami/features/body_physics.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
using namespace kami::avatar;
int main() {
    BodyPhysics p;
    if(p.profile()!=BodyProfile::Natural||!p.wearable().enabled)return 1;
    State state{};state.grounded=true;
    float peak=0.f;
    for(int frame=0;frame<160;++frame) {
        state.horizontal_speed=3.2f;
        state.animation_phase=static_cast<float>(frame)*.13f;
        p.step(state,1.f/60.f);
        const auto x=p.offsets();
        peak=std::max(peak,std::max(std::abs(x.breast_y),
             std::max(std::abs(x.belly_y),std::abs(x.butt_y))));
        if(std::abs(x.breast_y)>.08501f||
           std::abs(x.belly_y)>.07001f||
           std::abs(x.butt_y)>.08001f)return 2;
    }
    if(peak<.007f)return 3;
    p.set_profile(BodyProfile::Off);
    if(p.wearable().enabled||p.offsets().breast_y!=0.f)return 4;
    for(int i=0;i<120;++i)p.step(state,1.f/60.f);
    if(p.offsets().breast_y!=0.f||p.offsets().belly_y!=0.f||
       p.offsets().butt_y!=0.f)return 5;
    p.set_profile(BodyProfile::Lively);
    state.grounded=false;state.vertical_velocity=5.f;
    for(int i=0;i<25;++i)p.step(state,1.f/60.f);
    if(std::abs(p.offsets().breast_y)<.008f)return 6;
    state.grounded=true;state.vertical_velocity=0.f;
    p.step(state,1.f/60.f);
    if(!std::isfinite(p.offsets().butt_y))return 7;
    p.reset();
    if(p.offsets().breast_y!=0.f||p.offsets().belly_y!=0.f||
       p.offsets().butt_y!=0.f)return 8;
    PhysicsWearable broken{};
    broken.enabled=true;
    broken.breast.max_effect_m=999.f;
    broken.breast.spring=std::numeric_limits<float>::quiet_NaN();
    broken.belly.max_effect_m=-2.f;
    p.set_wearable(broken);
    if(p.wearable().breast.max_effect_m>.24001f||
       !std::isfinite(p.wearable().breast.spring)||
       p.wearable().belly.max_effect_m!=0.f)return 9;
    p.step(state,std::numeric_limits<float>::infinity());
    if(!std::isfinite(p.offsets().breast_y))return 10;
    p.set_profile(BodyProfile::Subtle);
    if(p.profile()!=BodyProfile::Subtle||!p.wearable().enabled)return 11;
    if(profile_label(BodyProfile::Subtle)!=std::wstring_view(L"SUBTLE"))return 12;
    return 0;
}
