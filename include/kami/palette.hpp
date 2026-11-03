#pragma once
#include <d2d1.h>
namespace kami::ui {
struct Palette {
    D2D1_COLOR_F background;
    D2D1_COLOR_F surface;
    D2D1_COLOR_F elevated;
    D2D1_COLOR_F text;
    D2D1_COLOR_F muted;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F accent_soft;
    D2D1_COLOR_F line;
};
[[nodiscard]] Palette colors(bool light) noexcept;
}
