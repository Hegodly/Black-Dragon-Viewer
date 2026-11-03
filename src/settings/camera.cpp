#include "kami/camera/system.hpp"
#include <windows.h>

namespace kami::camera {
namespace {
constexpr wchar_t kPath[] = L"Software\\KAMI\\OriginalCore\\Camera";
DWORD read(HKEY key, const wchar_t* name, DWORD fallback) noexcept {
    DWORD value = fallback, bytes = sizeof(value), type = 0;
    if (RegQueryValueExW(key, name, nullptr, &type,
                         reinterpret_cast<BYTE*>(&value), &bytes) != ERROR_SUCCESS ||
        type != REG_DWORD || bytes != sizeof(value)) return fallback;
    return value;
}
void write(HKEY key, const wchar_t* name, DWORD value) noexcept {
    (void)RegSetValueExW(key, name, 0, REG_DWORD,
                        reinterpret_cast<const BYTE*>(&value), sizeof(value));
}
} // namespace

Settings load() {
    Settings value{};
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPath, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return value;
    value.mouse_sensitivity = read(key, L"MouseSensitivity", 50);
    value.smoothing = read(key, L"Smoothing", 15);
    value.follow_distance_cm = read(key, L"FollowDistanceCentimeters", 400);
    value.collision = read(key, L"TerrainCollision", 1) != 0;
    value.auto_recenter = read(key, L"AutoRecenter", 1) != 0;
    value.head_tracking = read(key, L"HeadTracking", 1) != 0;
    value.camera_shake = read(key, L"CameraShake", 0) != 0;
    RegCloseKey(key);
    return normalize(value);
}
void save(Settings value) noexcept {
    value = normalize(value);
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kPath, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return;
    write(key, L"MouseSensitivity", value.mouse_sensitivity);
    write(key, L"Smoothing", value.smoothing);
    write(key, L"FollowDistanceCentimeters", value.follow_distance_cm);
    write(key, L"TerrainCollision", value.collision ? 1U : 0U);
    write(key, L"AutoRecenter", value.auto_recenter ? 1U : 0U);
    write(key, L"HeadTracking", value.head_tracking ? 1U : 0U);
    write(key, L"CameraShake", value.camera_shake ? 1U : 0U);
    RegCloseKey(key);
}
} // namespace kami::camera
