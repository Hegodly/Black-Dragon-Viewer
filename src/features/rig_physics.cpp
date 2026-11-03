#include "kami/features/rig_physics.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>

namespace kami::rigphysics {
namespace {
struct Binding {
    std::string_view name;
    std::size_t axis;
    unsigned property;
    bool family;
};
constexpr auto kNames=std::to_array<Binding>({
    {"Breast_Physics_UpDown_Max_Effect",0,0,false},
    {"Breast_Physics_UpDown_Spring",0,1,false},
    {"Breast_Physics_UpDown_Gain",0,2,false},
    {"Breast_Physics_UpDown_Damping",0,3,false},
    {"Breast_Physics_LeftRight_Max_Effect",1,0,false},
    {"Breast_Physics_LeftRight_Spring",1,1,false},
    {"Breast_Physics_LeftRight_Gain",1,2,false},
    {"Breast_Physics_LeftRight_Damping",1,3,false},
    {"Breast_Physics_InOut_Max_Effect",2,0,false},
    {"Breast_Physics_InOut_Spring",2,1,false},
    {"Breast_Physics_InOut_Gain",2,2,false},
    {"Breast_Physics_InOut_Damping",2,3,false},
    {"Breast_Physics_Mass",0,4,true},
    {"Breast_Physics_Gravity",0,5,true},
    {"Breast_Physics_Drag",0,6,true},
    {"Butt_Physics_UpDown_Max_Effect",3,0,false},
    {"Butt_Physics_UpDown_Spring",3,1,false},
    {"Butt_Physics_UpDown_Gain",3,2,false},
    {"Butt_Physics_UpDown_Damping",3,3,false},
    {"Butt_Physics_LeftRight_Max_Effect",4,0,false},
    {"Butt_Physics_LeftRight_Spring",4,1,false},
    {"Butt_Physics_LeftRight_Gain",4,2,false},
    {"Butt_Physics_LeftRight_Damping",4,3,false},
    {"Butt_Physics_Mass",3,4,true},
    {"Butt_Physics_Gravity",3,5,true},
    {"Butt_Physics_Drag",3,6,true},
    {"Belly_Physics_UpDown_Max_Effect",5,0,false},
    {"Belly_Physics_UpDown_Spring",5,1,false},
    {"Belly_Physics_UpDown_Gain",5,2,false},
    {"Belly_Physics_UpDown_Damping",5,3,false},
    {"Belly_Physics_Mass",5,4,false},
    {"Belly_Physics_Gravity",5,5,false},
    {"Belly_Physics_Drag",5,6,false},
    // KAMI-exclusive preview controls. Not standard Second Life wearables.
    {"KAMI_Chest_Twist_Max_Effect",6,0,false},
    {"KAMI_Chest_Twist_Spring",6,1,false},
    {"KAMI_Chest_Twist_Gain",6,2,false},
    {"KAMI_Chest_Twist_Damping",6,3,false},
    {"KAMI_Butt_OpenClose_Max_Effect",7,0,false},
    {"KAMI_Butt_OpenClose_Spring",7,1,false},
    {"KAMI_Butt_OpenClose_Gain",7,2,false},
    {"KAMI_Butt_OpenClose_Damping",7,3,false}
});
float finite_clamp(float value,float low,float high,float fallback) noexcept {
    return std::isfinite(value)?std::clamp(value,low,high):fallback;
}
AxisSettings region(avatar::RegionPhysics p,float factor) noexcept {
    return {p.max_effect_m*factor,p.spring,p.gain,
            p.damping,p.mass,p.gravity,p.drag};
}
void write(AxisSettings& a,unsigned property,float value) noexcept {
    switch(property) {
    case 0:a.max_effect_m=value;break;
    case 1:a.spring=value;break;
    case 2:a.gain=value;break;
    case 3:a.damping=value;break;
    case 4:a.mass=value;break;
    case 5:a.gravity=value;break;
    case 6:a.drag=value;break;
    }
}
} // namespace

Wearable sanitize(Wearable result) noexcept {
    for(auto& a:result.settings) {
        a.max_effect_m=finite_clamp(a.max_effect_m,0.f,.24f,0.f);
        a.spring=finite_clamp(a.spring,8.f,150.f,55.f);
        a.gain=finite_clamp(a.gain,0.f,3.f,1.f);
        a.damping=finite_clamp(a.damping,2.f,35.f,13.f);
        a.mass=finite_clamp(a.mass,.3f,4.f,1.f);
        a.gravity=finite_clamp(a.gravity,-1.f,1.f,0.f);
        a.drag=finite_clamp(a.drag,0.f,1.f,0.f);
    }
    // Angles are radians. Cheek separation is meters per-side. Limits
    // avoid implausible twisting or skin intersections in the local test.
    result.settings[static_cast<std::size_t>(Axis::ChestTwist)].max_effect_m=
        std::min(result.settings[static_cast<std::size_t>(Axis::ChestTwist)].max_effect_m,
                 .20f);
    result.settings[static_cast<std::size_t>(Axis::ButtOpenClose)].max_effect_m=
        std::min(result.settings[static_cast<std::size_t>(Axis::ButtOpenClose)].max_effect_m,
                 .09f);
    return result;
}
Wearable from_profile(avatar::BodyProfile profile) noexcept {
    const auto p=avatar::preview_wearable(profile);
    Wearable w{};
    w.enabled=p.enabled;
    w.settings={{
        region(p.breast,1.f),region(p.breast,.6f),
        region(p.breast,.45f),region(p.butt,1.f),
        region(p.butt,.65f),region(p.belly,1.f),
        // KAMI-exclusive rotation (radians) and inward/outward cheek motion.
        region(p.breast,.52f),region(p.butt,.53f)
    }};
    return sanitize(w);
}
bool decode_named_parameters(std::span<const NamedParameter> fields,
                             Wearable& output) noexcept {
    if(fields.size()>96)return false;
    Wearable candidate=from_profile(avatar::BodyProfile::Natural);
    std::array<bool,kNames.size()> seen{};
    bool found=false;
    for(const auto& field:fields) {
        if(field.name.size()>96||!std::isfinite(field.value))return false;
        for(std::size_t i=0;i<kNames.size();++i) {
            const auto& entry=kNames[i];
            if(field.name!=entry.name)continue;
            if(seen[i])return false;
            seen[i]=true;
            found=true;
            if(entry.family) {
                const std::size_t last=entry.axis==0?3:5;
                for(std::size_t n=entry.axis;n<last;++n)
                    write(candidate.settings[n],entry.property,field.value);
            } else {
                write(candidate.settings[entry.axis],entry.property,field.value);
            }
            break;
        }
    }
    if(!found)return false;
    output=sanitize(candidate);
    output.enabled=true;
    return true;
}
Budget budget_for(float distance,bool own_avatar) noexcept {
    if(own_avatar)return {1,true};
    if(!std::isfinite(distance)||distance<0.f)return {1,false};
    if(distance<12.f)return {1,true};
    if(distance<26.f)return {2,true};
    if(distance<44.f)return {4,true};
    return {1,false};
}
rig::Pose build_pose(const Output& o) noexcept {
    rig::Pose p{};
    const float rise=o.at(Axis::ChestVertical);
    const float sway=o.at(Axis::ChestLateral);
    const float depth=o.at(Axis::ChestDepth);
    p[static_cast<std::size_t>(rig::Joint::LeftPec)].translation=
        {-sway,rise,depth};
    p[static_cast<std::size_t>(rig::Joint::RightPec)].translation=
        {sway,rise,depth};
    p[static_cast<std::size_t>(rig::Joint::Belly)].translation.y=
        o.at(Axis::BellyVertical);
    p[static_cast<std::size_t>(rig::Joint::Butt)].translation={
        o.at(Axis::ButtLateral),o.at(Axis::ButtVertical),0.f};
    // Opposing rotations are evaluated by skinning, not direct vertex offsets.
    // Only KAMI's preview mesh uses this extra effect until real SL rig
    // retargeting and compatible mesh deformation are implemented.
    const float chest_twist=o.at(Axis::ChestTwist);
    p[static_cast<std::size_t>(rig::Joint::LeftPec)].roll=chest_twist;
    p[static_cast<std::size_t>(rig::Joint::RightPec)].roll=-chest_twist;
    // Positive closure value opens the two lobes away from their centerline;
    // negative closes them. Helpers are children of the common BUTT joint.
    const float opening=o.at(Axis::ButtOpenClose);
    p[static_cast<std::size_t>(rig::Joint::LeftButt)].translation.x=-opening;
    p[static_cast<std::size_t>(rig::Joint::RightButt)].translation.x=opening;
    return p;
}
Solver::Solver() noexcept {set_profile(profile_);}
void Solver::set_profile(avatar::BodyProfile profile) noexcept {
    switch(profile) {
    case avatar::BodyProfile::Off:case avatar::BodyProfile::Subtle:
    case avatar::BodyProfile::Natural:case avatar::BodyProfile::Lively:break;
    default:profile=avatar::BodyProfile::Off;
    }
    profile_=profile;
    wearable_=from_profile(profile);
    reset();
}
void Solver::set_wearable(Wearable wearable) noexcept {
    wearable_=sanitize(wearable);
    reset();
}
void Solver::reset() noexcept {
    output_={};velocity_={};
    previous_speed_=previous_heading_=0.f;
    previous_grounded_=true;
}
void Solver::step(avatar::State motion,float delta) noexcept {
    if(!wearable_.enabled){output_={};velocity_={};return;}
    if(!std::isfinite(delta)||delta<=0.f||
       !std::isfinite(motion.horizontal_speed)||
       !std::isfinite(motion.vertical_velocity)||
       !std::isfinite(motion.heading)||
       !std::isfinite(motion.animation_phase))return;
    const float dt=std::min(delta,.10f);
    const float speed=std::clamp(motion.horizontal_speed,0.f,8.f);
    const float walk=std::clamp(speed/4.4f,0.f,1.f);
    const float jump=std::clamp(motion.vertical_velocity/6.f,-1.f,1.f);
    const float accel=std::clamp((speed-previous_speed_)*1.2f,-1.f,1.f);
    const float landed=motion.grounded&&!previous_grounded_?-1.f:0.f;
    float d_heading=motion.heading-previous_heading_;
    if(d_heading>3.14159265f)d_heading-=6.2831853f;
    if(d_heading< -3.14159265f)d_heading+=6.2831853f;
    const float turning=std::clamp(d_heading/std::max(dt,.001f)*.16f,-1.f,1.f);
    previous_speed_=speed;previous_heading_=motion.heading;
    previous_grounded_=motion.grounded;
    const float phase=motion.animation_phase;
    const std::array<float,kAxes> input{{
        std::sin(phase*1.9f)*walk*.7f+jump*.5f+accel*.25f+landed*.7f,
        std::cos(phase*1.35f)*walk*.45f+turning*.6f,
        std::sin(phase*2.1f)*walk*.38f+accel*.5f,
        -std::sin(phase*1.8f)*walk*.7f+jump*.45f+landed*.5f,
        std::sin(phase*1.4f)*walk*.5f-turning*.45f,
        std::sin(phase*1.65f)*walk*.4f+jump*.25f+landed*.75f,
        // Rotational chest sway reacts to direction changes and gait.
        std::cos(phase*1.53f)*walk*.55f+turning*.70f+accel*.14f,
        // In-out cheek motion: negative drives closure/contact, positive
        // opens and separates. The spring carries momentum through reversal.
        std::sin(phase*1.88f+.40f)*walk*.85f+accel*.30f+landed*.28f
    }};
    float remaining=dt;
    while(remaining>0.000001f) {
        const float h=std::min(remaining,.008f);
        remaining-=h;
        for(std::size_t i=0;i<kAxes;++i) {
            const auto& a=wearable_.settings[i];
            float& value=output_.offsets[i],&speed_y=velocity_[i];
            const float target=std::clamp(
                (input[i]*a.gain-a.gravity*.18f)*a.max_effect_m,
                -a.max_effect_m,a.max_effect_m);
            const float force=(a.spring*(target-value)-
                (a.damping+a.drag*12.f)*speed_y)/a.mass;
            speed_y+=force*h;
            value+=speed_y*h;
            if(value>a.max_effect_m) {
                value=a.max_effect_m;speed_y=std::min(0.f,speed_y);
            }
            if(value< -a.max_effect_m) {
                value=-a.max_effect_m;speed_y=std::max(0.f,speed_y);
            }
            if(i==static_cast<std::size_t>(Axis::ButtOpenClose)) {
                // Procedural cheek centerlines start 0.44m apart with .19m
                // half-widths. No interpenetration: closure <= .03m / side.
                // Add light rebound on contact, rather than two rigid
                // surfaces tunnelling through each other during a fast clap.
                constexpr float kContactLimit=-.029f;
                if(value<kContactLimit) {
                    value=kContactLimit;
                    if(speed_y<0.f)speed_y=-speed_y*.20f;
                }
            }
        }
    }
}
} // namespace kami::rigphysics
