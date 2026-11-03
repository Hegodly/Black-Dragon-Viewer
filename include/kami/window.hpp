#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string_view>

#include "kami/palette.hpp"
#include "kami/session.hpp"
#include "kami/world/renderer.hpp"
#include "kami/world/warmup.hpp"

namespace kami::app {
class Window final {
public:
    [[nodiscard]] bool create(HINSTANCE instance, int show);
private:
    static LRESULT CALLBACK dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT message, WPARAM wp, LPARAM lp);
    [[nodiscard]] bool create_resources();
    void paint();
    void resize(UINT width, UINT height);
    void update_viewport();
    void select_page(unsigned page);
    void toggle_appearance();
    void click(float x, float y);
    void round_rect(D2D1_RECT_F bounds, float radius, D2D1_COLOR_F color);
    void label(std::wstring_view text, D2D1_RECT_F bounds, IDWriteTextFormat* font, D2D1_COLOR_F color);
    void draw_interface(float width, float height, const ui::Palette& palette);

    HWND hwnd_ = nullptr;
    HWND viewport_ = nullptr;
    bool world_initialized_ = false;
    bool world_ready_ = false;
    world::Renderer world_renderer_;
    world::ShaderWarmup shader_warmup_;
    bool light_ = false;
    unsigned page_ = 0;
    protocol::Session session_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<IDWriteFactory> text_factory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> hero_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> small_font_;
};
}
