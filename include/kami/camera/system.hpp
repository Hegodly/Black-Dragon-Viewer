#pragma once
#include <cstdint>

namespace kami::camera {

// KAMI-original camera math, independent of rendering API or viewer code.
inline constexpr float kVerticalFovDegrees = 75.f;

struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;
};
struct Settings {
    unsigned mouse_sensitivity = 50;  // Percentage; 50 = default.
    unsigned smoothing = 15;          // Percentage; zero = no interpolation.
    unsigned follow_distance_cm = 400;
    bool collision = true;
    bool auto_recenter = true;
    bool head_tracking = true;
    bool camera_shake = false;
};
[[nodiscard]] Settings normalize(Settings value) noexcept;
[[nodiscard]] Settings load();
void save(Settings value) noexcept;

enum class Shoulder : std::uint8_t { Center, Right, Left };
enum class StudioShot : std::uint8_t { FullBody, Portrait, Detail, Wide };
struct Pose {
    Vec3 eye{};
    Vec3 focus{};
    float fov_y_degrees = kVerticalFovDegrees; // Intentional internal fixed FOV.
};
using HeightSampler = float (*)(float x, float z);

class System final {
public:
    void set_settings(Settings settings) noexcept;
    [[nodiscard]] Settings settings() const noexcept { return settings_; }
    void pointer_delta(float dx, float dy) noexcept;
    void orbit(float yaw_radians, float pitch_radians) noexcept;
    void zoom(float delta_meters) noexcept;
    void set_subject(Vec3 feet, float heading_radians = 0.f) noexcept;
    void set_dragging(bool dragging) noexcept { dragging_ = dragging; if (dragging) idle_seconds_ = 0.f; }
    void toggle_precision() noexcept { precision_ = !precision_; }
    void toggle_focus_lock() noexcept { focus_lock_ = !focus_lock_; }
    void toggle_cinematic_orbit() noexcept { cinematic_orbit_ = !cinematic_orbit_; idle_seconds_ = 0.f; }
    void cycle_shoulder() noexcept;
    void apply_studio_shot(StudioShot shot) noexcept;
    void reset() noexcept;
    void update(float delta_seconds, float elapsed_seconds, HeightSampler terrain_height,
                bool animate_subject) noexcept;
    [[nodiscard]] Pose pose() const noexcept { return pose_; }
    [[nodiscard]] bool precision_mode() const noexcept { return precision_; }
    [[nodiscard]] bool focus_locked() const noexcept { return focus_lock_; }
    [[nodiscard]] bool cinematic_orbit() const noexcept { return cinematic_orbit_; }
    [[nodiscard]] Shoulder shoulder() const noexcept { return shoulder_; }
    [[nodiscard]] bool camera_clipped() const noexcept { return clipped_; }

private:
    Settings settings_{};
    Vec3 subject_{};
    Vec3 smoothed_focus_{0.f, 2.77f, 0.f};
    Vec3 held_focus_{0.f, 2.77f, 0.f};
    Pose pose_{};
    float subject_heading_ = 0.f;
    float desired_yaw_ = 0.65f;
    float yaw_ = 0.65f;
    float desired_pitch_ = 0.16f;
    float pitch_ = 0.16f;
    float distance_m_ = 4.f;
    float smoothed_distance_m_ = 4.f;
    float shoulder_amount_ = 0.f;
    float idle_seconds_ = 0.f;
    Shoulder shoulder_ = Shoulder::Center;
    bool dragging_ = false;
    bool precision_ = false;
    bool focus_lock_ = true;
    bool cinematic_orbit_ = false;
    bool clipped_ = false;
};
} // namespace kami::camera
