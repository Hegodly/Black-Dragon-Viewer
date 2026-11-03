#include "kami/window.hpp"
#include "kami/appearance.hpp"

#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <string_view>

namespace kami::app {
namespace {
constexpr wchar_t kClassName[] = L"KAMI_BeyondReality_Original_Window";
constexpr std::array<std::wstring_view, 6> kPages = {
    L"HOME", L"WORLD", L"AVATAR", L"INVENTORY", L"MESSAGES", L"SETTINGS"
};
constexpr std::array<std::wstring_view, 6> kDescriptions = {
    L"Original Windows foundation. No imported viewer code.",
    L"Region login and world rendering are not implemented yet.",
    L"Avatar syncing, attachments and animation are not yet available.",
    L"Inventory is not connected. No inventory items can be changed.",
    L"Messages and delivery receipts are not implemented yet.",
    L"Change appearance with the button above or press T."
};
constexpr std::array<std::wstring_view, 6> kExplanations = {
    L"Written for KAMI  /  Native Direct2D + DirectWrite",
    L"World access requires independent authentication and transport.",
    L"Avatar identity will be shown only after real server login.",
    L"Future inventory operations will honor server permissions.",
    L"Nothing on this screen sends a message to another resident.",
    L"Choose Home, World, Avatar, Inventory, Messages or Settings."
};
D2D1_RECT_F box(float x, float y, float width, float height) {
    return D2D1::RectF(x, y, x+width, y+height);
}
} // namespace

bool Window::create(HINSTANCE instance, int show) {
    light_ = settings::light_appearance();
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                  factory_.ReleaseAndGetAddressOf()))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(text_factory_.ReleaseAndGetAddressOf())))) return false;

    auto font = [this](float size, DWRITE_FONT_WEIGHT weight,
                       Microsoft::WRL::ComPtr<IDWriteTextFormat>& result) {
        return text_factory_->CreateTextFormat(
            L"Segoe UI", nullptr, weight, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, size, L"en-us",
            result.ReleaseAndGetAddressOf());
    };
    if (FAILED(font(42.f, DWRITE_FONT_WEIGHT_BOLD, hero_font_)) ||
        FAILED(font(23.f, DWRITE_FONT_WEIGHT_SEMI_BOLD, title_font_)) ||
        FAILED(font(17.f, DWRITE_FONT_WEIGHT_NORMAL, body_font_)) ||
        FAILED(font(13.f, DWRITE_FONT_WEIGHT_NORMAL, small_font_))) return false;

    WNDCLASSEXW descriptor{};
    descriptor.cbSize = sizeof(descriptor);
    descriptor.hInstance = instance;
    descriptor.lpfnWndProc = &Window::dispatch;
    descriptor.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    descriptor.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    descriptor.lpszClassName = kClassName;
    descriptor.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    hwnd_ = CreateWindowExW(
        0, kClassName, L"KAMI Beyond Reality  |  Original Core (Development)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 770,
        nullptr, nullptr, instance, this);
    if (!hwnd_) return false;

    const BOOL dark = light_ ? FALSE : TRUE;
    (void)DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    // COD-like first-run hardware pipeline preparation happens automatically
    // on the opening KAMI screen, before the World tab is entered.
    shader_warmup_.begin();
    (void)SetTimer(hwnd_, 201, 33, nullptr);
    (void)SetTimer(hwnd_, 202, 125, nullptr);
    ShowWindow(hwnd_, show);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK Window::dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    auto* instance = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        auto* params = reinterpret_cast<CREATESTRUCTW*>(lp);
        instance = static_cast<Window*>(params->lpCreateParams);
        if (!instance) return FALSE;
        instance->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(instance));
    }
    return instance ? instance->handle(message, wp, lp)
                    : DefWindowProcW(hwnd, message, wp, lp);
}

LRESULT Window::handle(UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_, &ps);
        paint();
        EndPaint(hwnd_, &ps);
        return 0;
    }
    case WM_SIZE:
        resize(static_cast<UINT>(LOWORD(lp)), static_cast<UINT>(HIWORD(lp)));
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        click(static_cast<float>(GET_X_LPARAM(lp)),
              static_cast<float>(GET_Y_LPARAM(lp)));
        return 0;
    case WM_TIMER:
        if (wp == 201 && page_ == 1 && world_ready_ && !IsIconic(hwnd_))
            world_renderer_.draw();
        if (wp == 202) {
            const auto state=shader_warmup_.snapshot();
            if (page_ == 0) InvalidateRect(hwnd_, nullptr, FALSE);
            if (state.result != world::WarmupResult::Idle &&
                state.result != world::WarmupResult::Optimizing)
                (void)KillTimer(hwnd_, 202);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (page_ == 1 && world_ready_) {
            world_renderer_.zoom(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -0.7f : 0.7f);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if (page_ == 1 && world_ready_) {
            switch (wp) {
            case VK_LEFT: case 'A': world_renderer_.orbit(-.11f, 0.f); return 0;
            case VK_RIGHT: case 'D': world_renderer_.orbit(.11f, 0.f); return 0;
            case VK_UP: world_renderer_.orbit(0.f, .35f); return 0;
            case VK_DOWN: world_renderer_.orbit(0.f, -.35f); return 0;
            case 'W': world_renderer_.zoom(-.7f); return 0;
            case 'S': world_renderer_.zoom(.7f); return 0;
            }
        }
        if (wp >= VK_F1 && wp <= VK_F6) {
            select_page(static_cast<unsigned>(wp - VK_F1));
            return 0;
        }
        if (wp == 'T') {
            toggle_appearance();
            return 0;
        }
        break;
    case WM_GETMINMAXINFO: {
        auto* limits = reinterpret_cast<MINMAXINFO*>(lp);
        limits->ptMinTrackSize.x = 930;
        limits->ptMinTrackSize.y = 640;
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        (void)KillTimer(hwnd_, 201);
        (void)KillTimer(hwnd_, 202);
        world_renderer_.shutdown();
        target_.Reset();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd_, message, wp, lp);
}

bool Window::create_resources() {
    if (target_) return true;
    RECT client{};
    GetClientRect(hwnd_, &client);
    const auto pixels = D2D1::SizeU(
        static_cast<UINT>(std::max<LONG>(1, client.right)),
        static_cast<UINT>(std::max<LONG>(1, client.bottom)));
    return SUCCEEDED(factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                     D2D1::PixelFormat(), 96.f, 96.f),
        D2D1::HwndRenderTargetProperties(hwnd_, pixels),
        target_.ReleaseAndGetAddressOf()));
}

void Window::resize(UINT width, UINT height) {
    if (target_ && width && height) {
        if (target_->Resize(D2D1::SizeU(width, height)) == D2DERR_RECREATE_TARGET)
            target_.Reset();
    }
    update_viewport();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::update_viewport() {
    if (!hwnd_ || page_ != 1) {
        if (viewport_) ShowWindow(viewport_, SW_HIDE);
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int x=302, y=188;
    const int width=std::max<int>(128, client.right-350);
    const int height=std::max<int>(128, client.bottom-306);
    if (!viewport_) {
        viewport_=CreateWindowExW(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_BLACKRECT,
            x, y, width, height, hwnd_, nullptr,
            reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_, GWLP_HINSTANCE)),
            nullptr);
        if (!viewport_) return;
    }
    MoveWindow(viewport_,x,y,width,height,TRUE);
    if (!world_initialized_) {
        world_initialized_=true;
        world_ready_=world_renderer_.initialize(
            viewport_,static_cast<UINT>(width),static_cast<UINT>(height));
    } else if (world_ready_) {
        world_renderer_.resize(
            static_cast<UINT>(width),static_cast<UINT>(height));
        world_ready_=world_renderer_.ready();
    }
    ShowWindow(viewport_, world_ready_ ? SW_SHOW : SW_HIDE);
}

void Window::round_rect(D2D1_RECT_F bounds, float radius, D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (SUCCEEDED(target_->CreateSolidColorBrush(color, brush.GetAddressOf()))) {
        target_->FillRoundedRectangle(
            D2D1::RoundedRect(bounds, radius, radius), brush.Get());
    }
}

void Window::label(std::wstring_view text, D2D1_RECT_F bounds,
                   IDWriteTextFormat* font, D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if (font && SUCCEEDED(target_->CreateSolidColorBrush(color, brush.GetAddressOf()))) {
        target_->DrawTextW(
            text.data(), static_cast<UINT32>(text.size()), font,
            bounds, brush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
}

void Window::draw_interface(float width, float height, const ui::Palette& p) {
    round_rect(box(18,18,238,height-36),28.f,p.surface);
    label(L"K A M I",box(42,47,185,52),title_font_.Get(),p.text);
    label(L"B E Y O N D  R E A L I T Y",box(41,103,200,25),small_font_.Get(),p.accent);

    for (unsigned index = 0; index < kPages.size(); ++index) {
        const float y = 151.f + 56.f * static_cast<float>(index);
        if (page_ == index) round_rect(box(30,y,212,44),15.f,p.accent_soft);
        label(kPages[index],box(49,y+10,175,28),body_font_.Get(),
              page_==index?p.accent:p.muted);
    }
    label(L"ORIGINAL CORE / v0.1",box(42,height-65,189,25),small_font_.Get(),p.muted);

    const float contentX = 279.f;
    const float contentWidth = width - 305.f;
    label(L"KAMI  /  INDEPENDENT WINDOWS APPLICATION",box(contentX,28,620,24),
          small_font_.Get(),p.muted);
    round_rect(box(width-195,23,163,44),22.f,p.surface);
    label(light_?L"LIGHT MODE   [T]":L"DARK MODE   [T]",
          box(width-176,36,145,24),small_font_.Get(),p.accent);

    if (page_ == 1) {
        round_rect(box(contentX,90,contentWidth,height-178),30.f,p.elevated);
        label(L"WORLD  /  DIRECTX 12",box(contentX+28,105,contentWidth-55,45),
              title_font_.Get(),p.text);
        label(L"Offline KAMI scene lab: GPU geometry, lighting, depth and materials",
              box(contentX+30,151,contentWidth-50,30),small_font_.Get(),p.accent);
        if (!world_ready_) {
            label(world_initialized_?world_renderer_.error():L"Loading DX12 viewport...",
                  box(contentX+35,234,contentWidth-70,55),body_font_.Get(),p.muted);
        }
        label(L"A/D or arrows: orbit  |  W/S or wheel: zoom  |  Up/Down: tilt",
              box(contentX+29,height-103,contentWidth-50,30),small_font_.Get(),p.muted);
        label(L"Procedural test scene only. SL region and avatar transport: not connected.",
              box(contentX+29,height-71,contentWidth-50,30),small_font_.Get(),p.muted);
        return;
    }

    round_rect(box(contentX,90,contentWidth,241),30.f,p.elevated);
    label(kPages[page_],box(contentX+30,115,contentWidth-55,62),
          hero_font_.Get(),p.text);
    label(kDescriptions[page_],box(contentX+32,193,contentWidth-55,67),
          body_font_.Get(),p.muted);
    label(kExplanations[page_],box(contentX+32,279,contentWidth-55,34),
          small_font_.Get(),p.accent);

    const float gap = 18.f;
    const float cardWidth = (contentWidth-gap)/2.f;
    round_rect(box(contentX,354,cardWidth,180),23.f,p.surface);
    round_rect(box(contentX+cardWidth+gap,354,cardWidth,180),23.f,p.surface);

    label(L"SECOND LIFE CONNECTION",box(contentX+25,377,cardWidth-50,27),
          small_font_.Get(),p.muted);
    label(session_.can_enter_world()?L"CONNECTED":L"OFFLINE",
          box(contentX+25,412,cardWidth-50,44),title_font_.Get(),p.text);
    label(L"No simulated login",box(contentX+25,475,cardWidth-50,34),
          small_font_.Get(),p.muted);

    if (page_ == 0) {
        // Startup preparation is genuine D3D12 bytecode and GPU-specific PSO
        // compilation. The progress is not a simulated loading animation.
        const auto state=shader_warmup_.snapshot();
        label(L"DX12 SHADER OPTIMIZATION",
              box(contentX+cardWidth+40,377,cardWidth-52,27),
              small_font_.Get(),p.muted);
        std::wstring message;
        std::wstring detail;
        switch (state.result) {
        case world::WarmupResult::Idle:
        case world::WarmupResult::Optimizing:
            message=L"OPTIMIZING...";
            detail=std::to_wstring(state.percent)+L"%  /  Preparing GPU shaders";
            break;
        case world::WarmupResult::CacheReady:
            message=L"SHADERS READY";
            detail=L"Hardware cache reused - no recompilation";
            break;
        case world::WarmupResult::Optimized:
            message=L"COMPLETE";
            detail=state.cache_written ?
                L"GPU-specific shaders cached for future launches" :
                L"Shaders ready - disk cache unavailable";
            break;
        case world::WarmupResult::NoHardware:
            message=L"DX12 UNAVAILABLE";
            detail=L"No supported hardware GPU detected";
            break;
        case world::WarmupResult::Failed:
            message=L"OPTIMIZATION FAILED";
            detail=L"World will retry its shader pipeline";
            break;
        }
        label(message,box(contentX+cardWidth+40,414,cardWidth-52,40),
              title_font_.Get(),p.text);
        label(detail,box(contentX+cardWidth+40,470,cardWidth-52,25),
              small_font_.Get(),p.muted);
        const float barWidth=std::max(18.f,cardWidth-84.f);
        round_rect(box(contentX+cardWidth+40,510,barWidth,8),4.f,p.accent_soft);
        if (state.percent>0) {
            round_rect(box(contentX+cardWidth+40,510,
                barWidth*(static_cast<float>(state.percent)/100.f),8),
                4.f,p.accent);
        }
    } else {
        label(L"GRAPHICS ENGINE",box(contentX+cardWidth+40,377,cardWidth-52,27),
              small_font_.Get(),p.muted);
        label(L"DIRECTX 12",box(contentX+cardWidth+40,412,cardWidth-52,44),
              title_font_.Get(),p.text);
        label(L"Offline 3D scene lab - use F2",
              box(contentX+cardWidth+40,475,cardWidth-52,34),
              small_font_.Get(),p.muted);
    }
    label(L"ALPHA 0.1  /  This application is not yet a Second Life viewer.",
          box(contentX,height-66,contentWidth,32),
          small_font_.Get(),p.muted);
}

void Window::paint() {
    if (!create_resources()) return;
    const auto size = target_->GetSize();
    const auto p = ui::colors(light_);
    target_->BeginDraw();
    target_->Clear(p.background);
    draw_interface(size.width, size.height, p);
    if (target_->EndDraw() == D2DERR_RECREATE_TARGET) target_.Reset();
}

void Window::toggle_appearance() {
    light_ = !light_;
    settings::set_light_appearance(light_);
    const BOOL dark = light_ ? FALSE : TRUE;
    (void)DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::select_page(unsigned index) {
    if (index >= kPages.size()) return;
    page_ = index;
    update_viewport();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::click(float x, float y) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float width = static_cast<float>(client.right);
    if (x >= width-195.f && x <= width-32.f && y >= 23.f && y < 67.f) {
        toggle_appearance();
        return;
    }
    if (x >= 30.f && x <= 242.f) {
        for (unsigned index = 0; index < kPages.size(); ++index) {
            const float top = 151.f + 56.f*static_cast<float>(index);
            if (y >= top && y < top+44.f) {
                select_page(index);
                return;
            }
        }
    }
}
} // namespace kami::app
