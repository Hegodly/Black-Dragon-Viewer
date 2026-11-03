#pragma once
#include <cstdint>

namespace kami::graphics {
enum class Preset : std::uint32_t { Performance=0, Balanced=1, Quality=2, Potato=3 };

struct Settings {
    Preset preset=Preset::Balanced;
    unsigned render_scale_percent=100;
    unsigned fps_limit=60;  // 0 = uncapped; VSync may still limit presentation.
    bool vsync=true;
    bool pbr=true;
    bool animate_avatar=true;
    bool shadows=true;
    bool auto_quality=false;
    unsigned target_fps=60;
    bool atmosphere_haze=false;
    bool contact_ambient=true;
    bool diamond_glints=true;
    [[nodiscard]] unsigned terrain_divisions() const noexcept {
        switch(preset) {
        case Preset::Potato: return 8;
        case Preset::Performance: return 16;
        case Preset::Balanced: return 32;
        case Preset::Quality: return 64;
        }
        return 32;
    }
    [[nodiscard]] float sun_strength() const noexcept {
        switch(preset) {
        case Preset::Potato: return 0.28f;
        case Preset::Performance: return 0.48f;
        case Preset::Balanced: return 0.85f;
        case Preset::Quality: return 1.10f;
        }
        return 0.85f;
    }
};

[[nodiscard]] Settings normalize(Settings value) noexcept;
[[nodiscard]] Settings load();
void save(Settings value) noexcept;
[[nodiscard]] const wchar_t* preset_label(Preset preset) noexcept;
}
