#include "kami/features/rig.hpp"
#include <cmath>
#include <limits>
using namespace kami::rig;
int main() {
    Pose pose{};
    Palette matrix{};
    if(!make_palette(pose,matrix))return 1;
    const Vec3 p{-.2f,2.15f,.32f},n{0.f,1.f,0.f};
    Packed packed{};
    if(!pack_influences({{static_cast<std::uint8_t>(Joint::LeftPec),0,0,0},
                         {1.f,0.f,0.f,0.f}},packed))return 2;
    Vec3 out{},normal{};
    if(!deform(p,n,packed,matrix,out,normal))return 3;
    if(std::abs(out.x-p.x)>1e-5f||std::abs(out.y-p.y)>1e-5f)return 4;
    pose[static_cast<int>(Joint::LeftPec)].translation.y=.1f;
    if(!make_palette(pose,matrix))return 5;
    if(!deform(p,n,packed,matrix,out,normal)||std::abs(out.y-p.y-.1f)>1e-4f)return 6;
    pose[static_cast<int>(Joint::RightPec)].translation.y=-.1f;
    if(!make_palette(pose,matrix))return 7;
    Packed both{};
    if(!pack_influences({{static_cast<std::uint8_t>(Joint::LeftPec),
                          static_cast<std::uint8_t>(Joint::RightPec),0,0},
                         {.5f,.5f,0.f,0.f}},both))return 8;
    if(!deform(p,n,both,matrix,out,normal)||std::abs(out.y-p.y)>.003f)return 9;
    if(pack_influences({{120,0,0,0},{1.f,0.f,0.f,0.f}},both))return 10;
    if(pack_influences({{0,0,0,0},
            {std::numeric_limits<float>::quiet_NaN(),0.f,0.f,0.f}},both))return 11;
    if(pack_influences({{0,0,0,0},{0.f,0.f,0.f,0.f}},both))return 12;
    Packed invalid{};invalid.weights=255;invalid.joints=100;
    if(deform(p,n,invalid,matrix,out,normal))return 13;
    Pose invalid_pose{};invalid_pose[1].translation.y=
        std::numeric_limits<float>::infinity();
    if(make_palette(invalid_pose,matrix))return 14;
    if(!make_palette(Pose{},matrix))return 15;
    Packed no_skin{};
    if(!deform(p,n,no_skin,matrix,out,normal)||std::abs(out.y-p.y)>1e-5f)return 16;
    // Hierarchy: moving mChest offsets both breast joints, leaving pelvis.
    pose={};pose[static_cast<int>(Joint::Chest)].translation.y=.07f;
    if(!make_palette(pose,matrix)||!deform(p,n,packed,matrix,out,normal)||
       std::abs(out.y-p.y-.07f)>.0001f)return 17;
    // Both butt lobes must follow the common BUTT bone and diverge when
    // KAMI's local two-cheek helper pose opens the gap.
    pose={};
    pose[static_cast<int>(Joint::LeftButt)].translation.x=-.045f;
    pose[static_cast<int>(Joint::RightButt)].translation.x=.045f;
    if(kJointCount!=10 || !make_palette(pose,matrix))return 18;
    Packed l={},r={};
    if(!pack_influences({{static_cast<std::uint8_t>(Joint::LeftButt),0,0,0},
                         {1.f,0.f,0.f,0.f}},l) ||
       !pack_influences({{static_cast<std::uint8_t>(Joint::RightButt),0,0,0},
                         {1.f,0.f,0.f,0.f}},r))return 19;
    Vec3 left{},right{},left_normal{},right_normal{};
    if(!deform({-.22f,1.30f,-.265f},n,l,matrix,left,left_normal)||
       !deform({ .22f,1.30f,-.265f},n,r,matrix,right,right_normal))
        return 20;
    if(std::abs(left.x-(-.265f))>.0001f ||
       std::abs(right.x-.265f)>.0001f ||
       std::abs(left_normal.y-1.f)>.0001f)return 21;
    pose[static_cast<int>(Joint::LeftButt)].translation.x=.02f;
    pose[static_cast<int>(Joint::RightButt)].translation.x=-.02f;
    if(!make_palette(pose,matrix)||
       !deform({-.22f,1.30f,-.265f},n,l,matrix,left,left_normal)||
       !deform({ .22f,1.30f,-.265f},n,r,matrix,right,right_normal))
        return 22;
    if(!(left.x>-.22f && right.x<.22f))return 23;
    return 0;
}
