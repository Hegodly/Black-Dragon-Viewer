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
    c.set_settings({50, 0, 400, true, true, false, false});
    c.reset();
    c.update(.1f, 2.2f, flat, false);
    if (!near(c.pose().focus.y, 1.7f)) return 13;
    return 0;
}
