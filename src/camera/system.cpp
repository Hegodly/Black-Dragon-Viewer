#include "kami/camera/system.hpp"
#include <algorithm>
#include <cmath>

namespace kami::camera {
namespace {
constexpr float kPi = 3.14159265358979323846f;
float lerp(float from, float to, float alpha) noexcept {
    return from + (to - from) * alpha;
}
float response(float dt, unsigned smoothing) noexcept {
    if (smoothing == 0) return 1.f;
    const float tau = .015f + .40f * static_cast<float>(smoothing) / 100.f;
    return 1.f - std::exp(-dt / tau);
}
float finite_or(float value, float fallback) noexcept {
    return std::isfinite(value) ? value : fallback;
}
} // namespace

Settings normalize(Settings value) noexcept {
    value.mouse_sensitivity = std::clamp(value.mouse_sensitivity, 1U, 100U);
    value.smoothing = std::min(value.smoothing, 100U);
    value.follow_distance_cm = std::clamp(value.follow_distance_cm, 150U, 1000U);
    return value;
}

void System::set_settings(Settings settings) noexcept {
    settings = normalize(settings);
    const bool distance_changed = settings_.follow_distance_cm != settings.follow_distance_cm;
    settings_ = settings;
    if (distance_changed) distance_m_ = static_cast<float>(settings.follow_distance_cm) / 100.f;
}
void System::pointer_delta(float dx, float dy) noexcept {
    if (!std::isfinite(dx) || !std::isfinite(dy)) return;
    const float rate = .0032f * static_cast<float>(settings_.mouse_sensitivity) / 50.f
                       * (precision_ ? .35f : 1.f);
    orbit(std::clamp(dx, -500.f, 500.f) * rate,
          std::clamp(dy, -500.f, 500.f) * rate);
}
void System::orbit(float yaw_radians, float pitch_radians) noexcept {
    if (!std::isfinite(yaw_radians) || !std::isfinite(pitch_radians)) return;
    desired_yaw_ += yaw_radians;
    desired_pitch_ = std::clamp(desired_pitch_ + pitch_radians, -.45f, 1.15f);
    idle_seconds_ = 0.f;
}
void System::zoom(float delta_meters) noexcept {
    if (!std::isfinite(delta_meters)) return;
    distance_m_ = std::clamp(distance_m_ + delta_meters, 1.5f, 10.f);
    idle_seconds_ = 0.f;
}
void System::set_subject(Vec3 feet, float heading_radians) noexcept {
    if (!std::isfinite(feet.x) || !std::isfinite(feet.y) || !std::isfinite(feet.z) ||
        !std::isfinite(heading_radians)) return;
    subject_ = feet;
    subject_heading_ = heading_radians;
}
void System::cycle_shoulder() noexcept {
    switch (shoulder_) {
    case Shoulder::Center: shoulder_ = Shoulder::Right; break;
    case Shoulder::Right: shoulder_ = Shoulder::Left; break;
    case Shoulder::Left: shoulder_ = Shoulder::Center; break;
    }
}
void System::apply_studio_shot(StudioShot shot) noexcept {
    // KAMI-original framing choices; no scripted avatar pose change.
    switch(shot) {
    case StudioShot::FullBody:
        distance_m_=4.f;desired_pitch_=.16f;shoulder_=Shoulder::Center;break;
    case StudioShot::Portrait:
        distance_m_=2.6f;desired_pitch_=.04f;shoulder_=Shoulder::Right;break;
    case StudioShot::Detail:
        distance_m_=1.5f;desired_pitch_=-.09f;shoulder_=Shoulder::Center;break;
    case StudioShot::Wide:
        distance_m_=8.f;desired_pitch_=.30f;shoulder_=Shoulder::Center;break;
    }
    desired_yaw_=subject_heading_+.65f;
    focus_lock_=true;
    idle_seconds_=0.f;
}
void System::reset() noexcept {
    desired_yaw_ = subject_heading_ + .65f;
    desired_pitch_ = .16f;
    distance_m_ = static_cast<float>(settings_.follow_distance_cm) / 100.f;
    shoulder_ = Shoulder::Center;
    precision_ = false;
    focus_lock_ = true;
    cinematic_orbit_ = false;
    idle_seconds_ = 0.f;
}
void System::update(float delta_seconds, float elapsed_seconds,
                    HeightSampler terrain_height, bool animate_subject) noexcept {
    const float dt = std::clamp(finite_or(delta_seconds, 0.f), 0.f, .1f);
    const float time = finite_or(elapsed_seconds, 0.f);
    if (!dragging_) idle_seconds_ += dt;
    if (cinematic_orbit_) {
        desired_yaw_ += .20f * dt;
        idle_seconds_ = 0.f;
    } else if (settings_.auto_recenter && !dragging_ && idle_seconds_ >= 2.f) {
        // Recenter against the avatar heading, not an arbitrary world axis.
        const float behind = subject_heading_ + .65f;
        desired_yaw_ = lerp(desired_yaw_, behind, 1.f - std::exp(-dt * 1.5f));
        desired_pitch_ = lerp(desired_pitch_, .16f, 1.f - std::exp(-dt * 1.5f));
    }
    const float a = response(dt, settings_.smoothing);
    yaw_ = lerp(yaw_, desired_yaw_, a);
    pitch_ = lerp(pitch_, desired_pitch_, a);
    smoothed_distance_m_ = lerp(smoothed_distance_m_, distance_m_, a);
    const float desired_shoulder = shoulder_ == Shoulder::Right ? 0.55f :
                                   shoulder_ == Shoulder::Left ? -0.55f : 0.f;
    shoulder_amount_ = lerp(shoulder_amount_, desired_shoulder, a);

    const float head_height = settings_.head_tracking ? 2.77f : 1.7f;
    const float bob = settings_.head_tracking && animate_subject ?
         .055f * std::sin(time * 2.f + head_height * .25f) : 0.f;
    const Vec3 focus_target{subject_.x, subject_.y + head_height + bob, subject_.z};
    if (focus_lock_) held_focus_ = focus_target;
    smoothed_focus_.x = lerp(smoothed_focus_.x, held_focus_.x, a);
    smoothed_focus_.y = lerp(smoothed_focus_.y, held_focus_.y, a);
    smoothed_focus_.z = lerp(smoothed_focus_.z, held_focus_.z, a);

    const float cp = std::cos(pitch_);
    const Vec3 along{std::sin(yaw_) * cp, std::sin(pitch_), std::cos(yaw_) * cp};
    const Vec3 sideways{std::cos(yaw_), 0.f, -std::sin(yaw_)};
    // Shoulder offset moves the framing, and the collision ray starts at
    // the actual shoulder-shifted target to avoid penetrating terrain.
    const Vec3 focus{smoothed_focus_.x + sideways.x * shoulder_amount_,
                     smoothed_focus_.y, smoothed_focus_.z + sideways.z * shoulder_amount_};
    const float desired = smoothed_distance_m_;
    float safe = desired;
    clipped_ = false;
    if (settings_.collision && terrain_height) {
        constexpr int kSamples = 28;
        for (int i = 1; i <= kSamples; ++i) {
            const float d = desired * static_cast<float>(i) / kSamples;
            const Vec3 p{focus.x + along.x * d, focus.y + along.y * d,
                         focus.z + along.z * d};
            const float floor = terrain_height(p.x, p.z);
            if (std::isfinite(floor) && p.y < floor + .22f) {
                safe = std::max(.3f, desired * static_cast<float>(i - 1) / kSamples);
                clipped_ = true;
                break;
            }
        }
    }
    Vec3 eye{focus.x + along.x * safe, focus.y + along.y * safe,
             focus.z + along.z * safe};
    if (settings_.camera_shake) {
        // Explicit opt-in: off by default and never used for camera smoothing.
        eye.y += .007f * std::sin(time * 17.f);
    }
    pose_ = {eye, focus, kVerticalFovDegrees};
}
} // namespace kami::camera
