#include "kami/features/avatar_motion.hpp"
#include "kami/world/scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
using kami::avatar::Controller;
using kami::avatar::Controls;
using kami::avatar::Motion;
static void advance(Controller& controller,int frames=120) {
    for(int i=0;i<frames;++i)
        controller.step(1.f/60.f,kami::world::sample_test_terrain_height);
}
int main() {
    Controller a;
    a.reset(kami::world::sample_test_terrain_height);
    if(std::abs(a.state().y-kami::world::sample_test_terrain_height(0,0))>.0001f)return 1;
    a.set_controls({1.f,0.f,0.f,false,false});
    advance(a,60);
    if(a.state().z<1.2f||a.state().motion!=Motion::Walking)return 2;
    if(a.state().gait<.6f||a.state().animation_phase<.1f)return 3;
    const float speed=a.state().horizontal_speed;
    a.set_controls({1.f,0.f,0.f,true,false});
    advance(a,60);
    if(a.state().horizontal_speed<speed+1.f||a.state().motion!=Motion::Running)return 4;
    a.set_controls({0.f,0.f,0.f,false,true});
    a.step(1.f/60.f,kami::world::sample_test_terrain_height);
    if(a.state().grounded||a.state().motion!=Motion::Jumping)return 5;
    float peak=a.state().y;
    for(int i=0;i<70;++i) {
        a.step(1.f/60.f,kami::world::sample_test_terrain_height);
        peak=std::max(peak,a.state().y);
    }
    if(peak<=kami::world::sample_test_terrain_height(a.state().x,a.state().z)+.3f||
       !a.state().grounded)return 6;
    advance(a,60);
    if(!a.state().grounded)return 7; // Holding jump never repeats.
    a.set_controls({0.f,0.f,1.f,false,false});
    advance(a,30);
    if(a.state().heading<.6f)return 8;
    a.set_controls({1.f,1.f,0.f,false,false});
    advance(a,40);
    if(!std::isfinite(a.state().x)||!std::isfinite(a.state().z))return 9;
    a.set_controls({});
    advance(a,150);
    if(a.state().motion!=Motion::Idle||a.state().horizontal_speed>.12f)return 10;
    a.set_controls({std::numeric_limits<float>::quiet_NaN(),2.f,0.f,false,false});
    a.step(std::numeric_limits<float>::infinity(),
           kami::world::sample_test_terrain_height);
    if(!std::isfinite(a.state().x)||a.controls().forward!=0.f||
       a.controls().strafe!=1.f)return 11;
    a.reset(kami::world::sample_test_terrain_height);
    a.set_controls({1.f,0.f,0.f,true,false});
    advance(a,750);
    if(a.state().z>13.5f||!std::isfinite(a.state().y))return 12;
    a.reset(kami::world::sample_test_terrain_height);
    if(a.state().motion!=Motion::Idle||a.state().horizontal_speed!=0.f||
       a.state().animation_phase!=0.f)return 13;
    return 0;
}
