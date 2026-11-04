#pragma once
#include "kami/features/avatar_motion.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <algorithm>
#include <cmath>

namespace kami::world {
// Only the initial spawn is authenticated. All later movement below is a
// LOCAL visual prediction, never a server-authoritative position update.
struct AvatarMarkerPose {
    float x=0.f,y=0.f,z=0.f,heading=0.f;
    bool moving=false;
};
class LiveAvatarPreview final {
public:
    [[nodiscard]] bool reset(const protocol::sim::AgentPosition& spawn) noexcept {
        clear();
        if(!spawn.verified||!std::isfinite(spawn.x)||
           !std::isfinite(spawn.y)||!std::isfinite(spawn.z)||
           spawn.x<0.f||spawn.x>=256.f||spawn.y<0.f||
           spawn.y>=256.f||spawn.z< -4096.f||spawn.z>8192.f)return false;
        origin_=spawn;valid_=true;
        pose_.y=-1.f; // estimated feet offset, not a server terrain height
        return true;
    }
    void clear() noexcept { valid_=false;origin_={};pose_={}; }
    [[nodiscard]] bool active() const noexcept {return valid_;}
    [[nodiscard]] const AvatarMarkerPose& pose() const noexcept {return pose_;}
    void step(float dt,avatar::Controls controls) noexcept {
        if(!valid_||!std::isfinite(dt)||dt<=0.f)return;
        dt=std::clamp(dt,0.f,.1f);
        const auto safe=[](float x){return std::isfinite(x)?
            std::clamp(x,-1.f,1.f):0.f;};
        pose_.heading=std::remainder(pose_.heading+
            safe(controls.turn)*2.f*dt,6.28318530718f);
        float forward=safe(controls.forward);
        float strafe=safe(controls.strafe);
        const float length=std::sqrt(forward*forward+strafe*strafe);
        if(length>1.f){forward/=length;strafe/=length;}
        const float speed=controls.run?5.f:3.f;
        const float si=std::sin(pose_.heading),co=std::cos(pose_.heading);
        pose_.x=std::clamp(pose_.x+
            (si*forward+co*strafe)*speed*dt,-origin_.x,255.99f-origin_.x);
        pose_.z=std::clamp(pose_.z+
            (co*forward-si*strafe)*speed*dt,-origin_.y,255.99f-origin_.y);
        pose_.moving=length>.01f;
        // Jump control goes to SL but visual height requires authoritative
        // simulator position and collision, so we do not invent that here.
    }
private:
    bool valid_=false;
    protocol::sim::AgentPosition origin_{};
    AvatarMarkerPose pose_{};
};
} // namespace kami::world
