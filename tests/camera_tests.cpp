#include "kami/camera/system.hpp"
#include <cmath>
#include <limits>

namespace {
bool near(float a, float b, float epsilon = .015f) {
    return std::abs(a - b) <= epsilon;
}
float flat(float, float) { return -100.f; }
float wall(float x, float) { return x > .5f ? 5.f : -100.f; }
}
int main() {
    using namespace kami::camera;
    const Settings defaults{};
    if (defaults.mouse_sensitivity != 50 || defaults.smoothing != 15 ||
        defaults.follow_distance_cm != 400 || !defaults.collision ||
        !defaults.auto_recenter || !defaults.head_tracking ||
        defaults.camera_shake) return 1;
    const auto invalid = normalize({0, 999, 2000, true, true, true, false});
    if (invalid.mouse_sensitivity != 1 || invalid.smoothing != 100 ||
        invalid.follow_distance_cm != 1000) return 2;
    System c;
    c.update(.016f, 0.f, flat, false);
    if (!near(c.pose().fov_y_degrees, 75.f) || !std::isfinite(c.pose().eye.x) ||
        !near(c.pose().focus.y, 2.77f)) return 3;
    c.pointer_delta(100.f, 20.f);
    c.update(.12f, .12f, flat, false);
    if (!(c.pose().eye.x > 0.f)) return 4;
    c.cycle_shoulder();
    if (c.shoulder() != Shoulder::Right) return 5;
    c.cycle_shoulder();
    if (c.shoulder() != Shoulder::Left) return 6;
    c.cycle_shoulder();
    if (c.shoulder() != Shoulder::Center) return 7;
    c.toggle_precision();
    c.toggle_cinematic_orbit();
    c.reset();
    if (c.precision_mode() || c.cinematic_orbit() || c.shoulder() != Shoulder::Center)
        return 8;
    c.set_subject({0.f, 0.f, 0.f}, 1.f);
    c.zoom(-100.f);
    c.update(.1f, 1.f, wall, false);
    if (!(c.pose().eye.x == c.pose().eye.x)) return 9;
    c.zoom(100.f);
    c.update(.1f, 2.f, flat, false);
    if (!std::isfinite(c.pose().eye.y)) return 10;
    const auto before = c.pose();
    c.pointer_delta(std::numeric_limits<float>::quiet_NaN(), 30.f);
    c.update(.0f, 2.f, flat, false);
    if (!near(c.pose().eye.x, before.eye.x)) return 11;
    c.reset();
    c.set_settings({50, 0, 400, true, false, true, false});
    c.update(.1f, 2.f, flat, false);
    const float baseline = c.pose().eye.x;
    c.orbit(.6f, 0.f);
    c.update(.1f, 2.1f, flat, false);
    if (near(c.pose().eye.x, baseline)) return 12;
    c.apply_studio_shot(StudioShot::Portrait);
    c.update(.1f,2.1f,flat,false);
    if(c.shoulder()!=Shoulder::Right)return 14;
    c.apply_studio_shot(StudioShot::Wide);
    if(c.shoulder()!=Shoulder::Center)return 15;
    c.set_settings({50, 0, 400, true, true, false, false});
    c.reset();
    c.update(.1f, 2.2f, flat, false);
    if (!near(c.pose().focus.y, 1.7f)) return 13;
    // Regression: an authenticated land patch located away from the
    // original offline mannequin must be frameable by the wide camera.
    c.set_settings({50,0,400,true,false,true,false});
    c.set_subject({48.f,-2.f,17.f});
    c.apply_studio_shot(StudioShot::Wide);
    c.update(.1f,2.3f,nullptr,false);
    const auto live=c.pose();
    if(!near(live.focus.x,48.f)||!near(live.focus.z,17.f)||
       !near(live.focus.y,.77f)||!(live.eye.y>live.focus.y)||
       !std::isfinite(live.eye.x)||!std::isfinite(live.eye.z))
        return 16;
    // In live follow, camera belongs behind +Z-facing marker. While
    // turning, it should hold still so the avatar's rotation is visible.
    System follow;
    follow.set_settings({50,0,400,true,true,false,false});
    follow.set_subject({0.f,0.f,0.f},0.f);
    follow.set_live_avatar_follow(true,false);
    for(unsigned i=0;i<50;++i)
        follow.update(.1f,3.f+i*.1f,nullptr,false);
    const auto rear=follow.pose();
    if(!(rear.eye.z<rear.focus.z-1.f))return 17;
    follow.set_subject({0.f,0.f,0.f},3.14159265f);
    follow.set_live_avatar_follow(true,true);
    for(unsigned i=0;i<10;++i)
        follow.update(.1f,9.f+i*.1f,nullptr,false);
    const auto during=follow.pose();
    if(!(during.eye.z<during.focus.z-1.f))return 18;
    follow.set_live_avatar_follow(true,false);
    for(unsigned i=0;i<55;++i)
        follow.update(.1f,10.f+i*.1f,nullptr,false);
    const auto complete=follow.pose();
    if(!(complete.eye.z>complete.focus.z+1.f))return 19;
    // Scenic mode deliberately centers a real decoded land point well
    // below the default avatar/skyward follow camera.
    System scenic;
    scenic.set_settings({50,0,400,true,true,true,false});
    scenic.set_subject({2.f,-2.77f,3.f},0.f);
    scenic.set_scenic_overlook(true);
    if(!scenic.scenic_overlook_active())return 20;
    for(int i=0;i<20;++i)
        scenic.update(.1f,12.f+.1f*i,nullptr,false);
    const auto overview=scenic.pose();
    if(!(overview.eye.y>overview.focus.y+7.f)||
       !near(overview.focus.x,2.f,.15f)||
       !near(overview.focus.z,3.f,.15f))return 21;
    scenic.set_scenic_overlook(false);
    if(scenic.scenic_overlook_active())return 22;
    return 0;
}
