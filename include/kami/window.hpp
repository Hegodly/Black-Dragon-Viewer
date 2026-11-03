#pragma once
#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string_view>
#include "kami/palette.hpp"
#include "kami/session.hpp"

namespace kami::app {
class Window final {
public:
    [[nodiscard]] bool create(HINSTANCE instance, int show);
private:
    static LRESULT CALLBACK dispatch(HWND window, UINT message, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT message, WPARAM wp, LPARAM lp);
    [[nodiscard]] bool create_resources();
    void paint();
    void resize(UINT width, UINT height);
    void toggle_theme();
    void select_page(unsigned index);
    void on_click(float x, float y);
    void rounded(D2D1_RECT_F bounds,float radius,D2D1_COLOR_F color);
    void label(std::wstring_view words,D2D1_RECT_F bounds,
               IDWriteTextFormat* format,D2D1_COLOR_F color);
    void draw(float width,float height,const kami::ui::Palette& palette);

    HWND hwnd_ = nullptr;
    bool light_ = false;
    unsigned page_ = 0;
    kami::protocol::Session session_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> surface_;
    Microsoft::WRL::ComPtr<IDWriteFactory> writing_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> hero_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> caption_;
};
}
