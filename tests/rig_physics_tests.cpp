#include "kami/features/rig_physics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
using namespace kami::rigphysics;
int main() {
    Solver solver;
    if(!solver.wearable().enabled)return 1;
    kami::avatar::State state{};
    std::array<float,kAxes> peaks{};
    for(int i=0;i<220;++i){
        state.horizontal_speed=4.f;
        state.animation_phase=i*.15f;
        state.heading=i*.002f;
        solver.step(state,1.f/60.f);
        const auto out=solver.output();
        for(std::size_t j=0;j<kAxes;++j) {
            peaks[j]=std::max(peaks[j],std::abs(out.offsets[j]));
            if(!std::isfinite(out.offsets[j])||
               std::abs(out.offsets[j])>solver.wearable().settings[j].max_effect_m+.001f)
                return 2;
        }
    }
    for(float peak:peaks)if(peak<.001f)return 3;
    const auto pose=build_pose(solver.output());
    if(pose[static_cast<int>(kami::rig::Joint::LeftPec)].translation.x==
       pose[static_cast<int>(kami::rig::Joint::RightPec)].translation.x &&
       std::abs(solver.output().at(Axis::ChestLateral))>.0001f)return 4;
    solver.set_profile(kami::avatar::BodyProfile::Off);
    solver.step(state,1.f/60.f);
    for(float x:solver.output().offsets)if(x!=0.f)return 5;
    if(budget_for(2.f,false).interval_frames!=1||
       budget_for(20.f,false).interval_frames!=2||
       budget_for(30.f,false).interval_frames!=4||
       budget_for(70.f,false).active||
       !budget_for(999.f,true).active||
       budget_for(std::numeric_limits<float>::quiet_NaN(),false).active)return 6;
    Wearable decoded{};
    constexpr std::array<NamedParameter,4> parameters{{
        {"Breast_Physics_UpDown_Max_Effect",.12f},
        {"Breast_Physics_LeftRight_Max_Effect",.08f},
        {"Breast_Physics_Mass",1.6f},
        {"Belly_Physics_UpDown_Max_Effect",.05f}
    }};
    if(!decode_named_parameters(parameters,decoded)||
       !decoded.enabled||std::abs(decoded.settings[0].max_effect_m-.12f)>.0001f||
       std::abs(decoded.settings[1].max_effect_m-.08f)>.0001f||
       std::abs(decoded.settings[2].mass-1.6f)>.0001f||
       std::abs(decoded.settings[5].max_effect_m-.05f)>.0001f)return 7;
    const std::array<NamedParameter,2> duplicate{{
        {"Breast_Physics_Mass",1.f},{"Breast_Physics_Mass",2.f}
    }};
    if(decode_named_parameters(duplicate,decoded))return 8;
    const std::array<NamedParameter,1> unknown{{
        {"SomeUnknownParam",123.f}
    }};
    if(decode_named_parameters(unknown,decoded))return 9;
    const std::array<NamedParameter,1> invalid{{
        {"Butt_Physics_Mass",std::numeric_limits<float>::infinity()}
    }};
    if(decode_named_parameters(invalid,decoded))return 10;
    solver.set_wearable(from_profile(kami::avatar::BodyProfile::Lively));
    state.vertical_velocity=5.f;state.grounded=false;
    for(int i=0;i<40;++i)solver.step(state,1.f/60.f);
    if(std::abs(solver.output().at(Axis::ChestVertical))<.005f)return 11;
    state.grounded=true;state.vertical_velocity=0.f;
    solver.step(state,1.f/60.f);
    if(!std::isfinite(solver.output().at(Axis::ButtVertical)))return 12;
    if(kAxes!=8)return 13;
    // New channels must oscillate under gait, and OFF must disable both.
    solver.set_profile(kami::avatar::BodyProfile::Natural);
    float twist_peak=0.f,closure_peak=0.f;
    float smallest_open_close=0.f,largest_open_close=0.f;
    state.grounded=true;
    for(int i=0;i<240;++i) {
        state.horizontal_speed=4.2f;
        state.animation_phase=i*.17f;
        state.heading=std::sin(i*.02f)*.15f;
        solver.step(state,1.f/60.f);
        twist_peak=std::max(twist_peak,std::abs(solver.output().at(Axis::ChestTwist)));
        const float clap=solver.output().at(Axis::ButtOpenClose);
        closure_peak=std::max(closure_peak,std::abs(clap));
        smallest_open_close=std::min(smallest_open_close,clap);
        largest_open_close=std::max(largest_open_close,clap);
        if(clap<-.02901f||!std::isfinite(clap))return 19;
    }
    // Opening AND closing, rather than whole-body side sway.
    if(twist_peak<.001f||closure_peak<.001f||
       smallest_open_close>-.001f||largest_open_close<.001f)return 14;
    Output specified{};
    specified.offsets[static_cast<int>(Axis::ButtOpenClose)]=.04f;
    specified.offsets[static_cast<int>(Axis::ChestTwist)]=.06f;
    const auto open=build_pose(specified);
    using J=kami::rig::Joint;
    if(std::abs(open[static_cast<int>(J::LeftButt)].translation.x+.04f)>1e-6f ||
       std::abs(open[static_cast<int>(J::RightButt)].translation.x-.04f)>1e-6f)
        return 15;
    if(std::abs(open[static_cast<int>(J::LeftPec)].roll-.06f)>1e-6f ||
       std::abs(open[static_cast<int>(J::RightPec)].roll+.06f)>1e-6f)
        return 16;
    specified.offsets[static_cast<int>(Axis::ButtOpenClose)]=-.025f;
    const auto closed=build_pose(specified);
    if(!(closed[static_cast<int>(J::LeftButt)].translation.x>0.f &&
         closed[static_cast<int>(J::RightButt)].translation.x<0.f))
        return 17;
    // Custom KAMI parameter must stay distinct from SL wearable settings.
    Wearable custom{};
    constexpr std::array<NamedParameter,2> enhanced{{
        {"KAMI_Butt_OpenClose_Max_Effect",.20f},
        {"KAMI_Chest_Twist_Max_Effect",.90f}
    }};
    if(!decode_named_parameters(enhanced,custom)||
       custom.settings[static_cast<int>(Axis::ButtOpenClose)].max_effect_m>.09001f||
       custom.settings[static_cast<int>(Axis::ChestTwist)].max_effect_m>.20001f)
        return 20;
    solver.set_profile(kami::avatar::BodyProfile::Off);
    solver.step(state,1.f/60.f);
    if(solver.output().at(Axis::ButtOpenClose)!=0.f||
       solver.output().at(Axis::ChestTwist)!=0.f)return 18;
    return 0;
}
