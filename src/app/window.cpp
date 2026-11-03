#include "kami/window.hpp"
#include "kami/appearance.hpp"
#include "kami/features/viewer_foundation.hpp"

#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <string_view>
#include <chrono>
#include <mmsystem.h>
#include <string>
#include <commdlg.h>
#include <filesystem>
#include <utility>
#include <cwchar>

namespace kami::app {
namespace {
constexpr wchar_t kClassName[] = L"KAMI_BeyondReality_Original_Window";
constexpr wchar_t kViewportClassName[] = L"KAMI_Native_Camera_Viewport";
constexpr int kMessengerEditId=4101;
constexpr std::array<std::wstring_view, 8> kPages = {
    L"HOME", L"WORLD", L"AVATAR", L"INVENTORY", L"MESSAGES", L"SETTINGS",
    L"CREATOR STUDIO", L"PHOTO STUDIO"
};
constexpr std::array<std::wstring_view, 8> kDescriptions = {
    L"Original Windows foundation. No imported viewer code.",
    L"Region login and world rendering are not implemented yet.",
    L"Avatar syncing, attachments and animation are not yet available.",
    L"Inventory is not connected. No inventory items can be changed.",
    L"Messages and delivery receipts are not implemented yet.",
    L"Change appearance with the button above or press T.",
    L"Local OBJ mesh preview and geometry validation.",
    L"Native DX12 live grading and PNG exports."
};
constexpr std::array<std::wstring_view, 8> kExplanations = {
    L"Written for KAMI  /  Native Direct2D + DirectWrite",
    L"World access requires independent authentication and transport.",
    L"Avatar identity will be shown only after real server login.",
    L"Future inventory operations will honor server permissions.",
    L"Nothing on this screen sends a message to another resident.",
    L"Choose Home, World, Avatar, Inventory, Messages or Settings.",
    L"Local mesh import only. Second Life upload service is not connected.",
    L"Photo export captures actual DX12 viewport pixels with a PNG encoder."
};
D2D1_RECT_F box(float x, float y, float width, float height) {
    return D2D1::RectF(x, y, x+width, y+height);
}
} // namespace

bool Window::create(HINSTANCE instance, int show) {
    light_ = settings::light_appearance();
    graphics_ = graphics::load();
    camera_settings_ = camera::load();
    photo_settings_ = photo::load();
    world_renderer_.set_graphics(graphics_);
    world_renderer_.set_camera_settings(camera_settings_);
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
        FAILED(font(13.f, DWRITE_FONT_WEIGHT_NORMAL, small_font_)) ||
        FAILED(font(16.f, DWRITE_FONT_WEIGHT_NORMAL, messenger_centered_font_)) ||
        FAILED(font(12.f, DWRITE_FONT_WEIGHT_MEDIUM, messenger_centered_small_)))
        return false;
    messenger_centered_font_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    messenger_centered_font_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    messenger_centered_font_->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
    messenger_centered_small_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    messenger_centered_small_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    WNDCLASSEXW descriptor{};
    descriptor.cbSize = sizeof(descriptor);
    descriptor.hInstance = instance;
    descriptor.lpfnWndProc = &Window::dispatch;
    descriptor.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    descriptor.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    descriptor.lpszClassName = kClassName;
    descriptor.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClassExW(&descriptor) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    WNDCLASSEXW viewportClass{};
    viewportClass.cbSize=sizeof(viewportClass);
    viewportClass.hInstance=instance;
    viewportClass.lpfnWndProc=&Window::viewport_dispatch;
    viewportClass.hCursor=LoadCursorW(nullptr,IDC_CROSS);
    viewportClass.lpszClassName=kViewportClassName;
    viewportClass.style=CS_HREDRAW|CS_VREDRAW;
    if (!RegisterClassExW(&viewportClass) &&
        GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;

    hwnd_ = CreateWindowExW(
        0, kClassName, L"KAMI Beyond Reality  |  Original Core (Development)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 770,
        nullptr, nullptr, instance, this);
    if (!hwnd_) return false;
    messenger_edit_=CreateWindowExW(0,L"EDIT",L"",
        WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL,
        0,0,10,30,hwnd_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kMessengerEditId)),
        instance,nullptr);
    if(!messenger_edit_)return false;
    (void)SendMessageW(messenger_edit_,EM_LIMITTEXT,512,0);
    (void)SendMessageW(messenger_edit_,WM_SETFONT,
        reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    messenger_edit_brush_=CreateSolidBrush(RGB(27,40,64));
    if(!messenger_edit_brush_)return false;

    const BOOL dark = light_ ? FALSE : TRUE;
    (void)DwmSetWindowAttribute(hwnd_, 20, &dark, sizeof(dark));
    // COD-like first-run hardware pipeline preparation happens automatically
    // on the opening KAMI screen, before the World tab is entered.
    shader_warmup_.begin();
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

LRESULT CALLBACK Window::viewport_dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    auto* app=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        const auto* create=reinterpret_cast<CREATESTRUCTW*>(lp);
        app=static_cast<Window*>(create->lpCreateParams);
        if(!app) return FALSE;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
    }
    return app?app->viewport_handle(hwnd,message,wp,lp):
               DefWindowProcW(hwnd,message,wp,lp);
}

LRESULT Window::viewport_handle(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    switch(message) {
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        SetCapture(window);
        camera_dragging_=true;
        pointer_x_=GET_X_LPARAM(lp);
        pointer_y_=GET_Y_LPARAM(lp);
        world_renderer_.set_camera_dragging(true);
        return 0;
    case WM_MOUSEMOVE:
        if(camera_dragging_ && GetCapture()==window) {
            const int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);
            world_renderer_.pointer_move(static_cast<float>(x-pointer_x_),
                                         static_cast<float>(y-pointer_y_));
            pointer_x_=x;
            pointer_y_=y;
        }
        return 0;
    case WM_LBUTTONUP:
        if(GetCapture()==window) ReleaseCapture();
        camera_dragging_=false;
        world_renderer_.set_camera_dragging(false);
        return 0;
    case WM_CAPTURECHANGED:
        camera_dragging_=false;
        world_renderer_.set_camera_dragging(false);
        return 0;
    case WM_MOUSEWHEEL:
        if(world_ready_)
            world_renderer_.zoom(GET_WHEEL_DELTA_WPARAM(wp)>0?-.35f:.35f);
        return 0;
    case WM_RBUTTONDOWN:
        SetFocus(hwnd_);
        world_renderer_.cycle_camera_shoulder();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProcW(window,message,wp,lp);
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
    case WM_CTLCOLOREDIT:
        if(reinterpret_cast<HWND>(lp)==messenger_edit_) {
            const HDC dc=reinterpret_cast<HDC>(wp);
            SetTextColor(dc,light_?RGB(18,36,58):RGB(237,244,255));
            SetBkColor(dc,light_?RGB(247,250,255):RGB(27,40,64));
            return reinterpret_cast<LRESULT>(messenger_edit_brush_);
        }
        break;
    case WM_COMMAND:
        if(LOWORD(wp)==kMessengerEditId && HIWORD(wp)==EN_CHANGE &&
           !messenger_edit_sync_ && messenger_edit_) {
            const int count=GetWindowTextLengthW(messenger_edit_);
            if(count>=0 && count<=512) {
                std::wstring value(static_cast<std::size_t>(count)+1,L'\0');
                const int copied=GetWindowTextW(messenger_edit_,value.data(),count+1);
                value.resize(static_cast<std::size_t>(std::max(0,copied)));
                (void)messenger_.set_draft(std::move(value));
            }
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        click(static_cast<float>(GET_X_LPARAM(lp)),
              static_cast<float>(GET_Y_LPARAM(lp)));
        return 0;
    case WM_TIMER:
        if (wp == 202) {
            const auto state=shader_warmup_.snapshot();
            if (page_ == 0) InvalidateRect(hwnd_, nullptr, FALSE);
            if (state.result != world::WarmupResult::Idle &&
                state.result != world::WarmupResult::Optimizing)
                (void)KillTimer(hwnd_, 202);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if ((page_ == 1 || page_ == 6 || page_ == 7) && world_ready_) {
            world_renderer_.zoom(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -0.7f : 0.7f);
            return 0;
        }
        break;
    case WM_KEYDOWN:
        if(page_ == 7 && wp >= '1' && wp <= '7') {
            advance_photo(static_cast<unsigned>(wp-'1'));
            return 0;
        }
        if(page_ == 6 && wp == 'I') {open_creator_obj();return 0;}
        if(page_ == 6 && wp == 'R') {
            world_renderer_.clear_creator_preview();
            creator_preview_={};creator_file_.clear();
            creator_message_=L"Original scene restored. Import an OBJ to preview.";
            InvalidateRect(hwnd_,nullptr,FALSE);
            return 0;
        }
        if(page_ == 5 && wp == 'C') {
            camera_settings_page_ = !camera_settings_page_;
            graphics_effects_page_ = false;
            InvalidateRect(hwnd_,nullptr,FALSE);
            return 0;
        }
        if(page_ == 5 && wp == 'G') {
            graphics_effects_page_ = !graphics_effects_page_;
            camera_settings_page_ = false;
            InvalidateRect(hwnd_,nullptr,FALSE);
            return 0;
        }
        if(page_ == 5 && wp >= '1' && wp <= '7') {
            if(camera_settings_page_) advance_camera(static_cast<unsigned>(wp-'1'));
            else if(graphics_effects_page_) advance_graphics_effects(static_cast<unsigned>(wp-'1'));
            else advance_graphics(static_cast<unsigned>(wp-'1'));
            return 0;
        }
        if ((page_ == 1 || page_ == 6 || page_ == 7) && world_ready_) {
            switch (wp) {
            case VK_LEFT: case 'A': world_renderer_.orbit(-.11f, 0.f); return 0;
            case VK_RIGHT: case 'D': world_renderer_.orbit(.11f, 0.f); return 0;
            case VK_UP: world_renderer_.orbit(0.f, .35f); return 0;
            case VK_DOWN: world_renderer_.orbit(0.f, -.35f); return 0;
            case 'W': world_renderer_.zoom(-.7f); return 0;
            case 'S': world_renderer_.zoom(.7f); return 0;
            case 'Q': world_renderer_.cycle_camera_shoulder(); return 0;
            case 'O': world_renderer_.toggle_cinematic_orbit(); return 0;
            case 'F': world_renderer_.toggle_camera_focus(); return 0;
            case 'P': world_renderer_.toggle_camera_precision(); return 0;
            case 'R': world_renderer_.reset_camera(); return 0;
            }
        }
        if (wp >= VK_F1 && wp <= VK_F8) {
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
        if(messenger_edit_brush_){DeleteObject(messenger_edit_brush_);messenger_edit_brush_=nullptr;}
        messenger_edit_=nullptr;
        if(high_resolution_timer_) (void)timeEndPeriod(1);
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
    update_messenger_composer();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::update_viewport() {
    if (!hwnd_ || (page_ != 1 && page_ != 6 && page_ != 7)) {
        if (viewport_) ShowWindow(viewport_, SW_HIDE);
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int x=302, y=188;
    const int width=std::max<int>(128, client.right-
        ((page_==6||page_==7)?690:350));
    const int height=std::max<int>(128, client.bottom-306);
    if (!viewport_) {
        viewport_=CreateWindowExW(0,kViewportClassName,L"",
            WS_CHILD | WS_VISIBLE,
            x,y,width,height,hwnd_,nullptr,
            reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd_,GWLP_HINSTANCE)),
            this);
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
        const float y = 151.f + 50.f * static_cast<float>(index);
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

    if(page_ == 5) {
        round_rect(box(contentX,90,contentWidth,height-165),29.f,p.elevated);
        round_rect(box(contentX+contentWidth-194.f,108,164,42),14.f,p.surface);
        label(camera_settings_page_?L"GRAPHICS  [C]":L"CAMERA  [C]",
              box(contentX+contentWidth-179.f,120,147,24),small_font_.Get(),p.accent);
        round_rect(box(contentX+contentWidth-370.f,108,164,42),14.f,p.surface);
        label(graphics_effects_page_?L"BASIC  [G]":L"DX12 FX  [G]",
              box(contentX+contentWidth-354.f,120,147,24),small_font_.Get(),p.accent);
        if(graphics_effects_page_) {
            label(L"KAMI NATIVE FX + ADAPTIVE FPS",
                  box(contentX+28,113,contentWidth-415,38),title_font_.Get(),p.text);
            label(L"DirectX 12 GPU shader options - click a row or press 1-5",
                  box(contentX+28,153,contentWidth-55,28),small_font_.Get(),p.muted);
            const wchar_t* names[]={
                L"1  AUTO RENDER RESOLUTION",L"2  TARGET FRAME RATE",
                L"3  ATMOSPHERE HAZE",L"4  MATERIAL CONTACT SHADING",
                L"5  METALLIC MICROGLINTS"
            };
            const std::wstring values[]={
                graphics_.auto_quality?L"ON (hysteresis + cooldown)":L"OFF",
                std::to_wstring(graphics_.target_fps)+L" FPS",
                graphics_.atmosphere_haze?L"DX12 SHADER ON":L"OFF",
                graphics_.contact_ambient?L"DX12 SHADER ON":L"OFF",
                graphics_.diamond_glints?L"DX12 SHADER ON":L"OFF"
            };
            for(unsigned i=0;i<5;++i) {
                const float y=186.f+55.f*static_cast<float>(i);
                round_rect(box(contentX+24,y,contentWidth-48,51),14.f,p.surface);
                label(names[i],box(contentX+45,y+4,contentWidth-85,20),small_font_.Get(),p.muted);
                label(values[i],box(contentX+45,y+26,contentWidth-85,26),body_font_.Get(),p.accent);
            }
            label(L"Native shading, not SSR, volumetric raymarch or screen-space AO.",
                  box(contentX+26,height-114,contentWidth-52,28),small_font_.Get(),p.muted);
            label(L"DLSS / XeSS / FSR / frame generation require separate SDK integration.",
                  box(contentX+26,height-83,contentWidth-52,28),small_font_.Get(),p.muted);
            return;
        }
        if(camera_settings_page_) {
            label(L"KAMI CAMERA  /  ORIGINAL ENGINE",
                  box(contentX+28,113,contentWidth-225,38),title_font_.Get(),p.text);
            label(L"Fixed 75 degree FOV  /  Click a row or press 1 through 7",
                  box(contentX+28,153,contentWidth-55,28),small_font_.Get(),p.muted);
            const wchar_t* names[]={
                L"1  MOUSE SENSITIVITY",L"2  CAMERA SMOOTHING",
                L"3  FOLLOW DISTANCE",L"4  TERRAIN COLLISION",
                L"5  AUTO RECENTER",L"6  HEAD TARGET TRACKING",
                L"7  CAMERA SHAKE"
            };
            const std::wstring values[]={
                std::to_wstring(camera_settings_.mouse_sensitivity)+L"%",
                std::to_wstring(camera_settings_.smoothing)+L"%",
                std::to_wstring(camera_settings_.follow_distance_cm/100)+L"m",
                camera_settings_.collision?L"ON":L"OFF",
                camera_settings_.auto_recenter?L"ON":L"OFF",
                camera_settings_.head_tracking?L"ON":L"OFF",
                camera_settings_.camera_shake?L"ON":L"OFF"
            };
            for(unsigned i=0;i<7;++i) {
                const float y=186.f+55.f*static_cast<float>(i);
                round_rect(box(contentX+24,y,contentWidth-48,51),14.f,p.surface);
                label(names[i],box(contentX+45,y+4,contentWidth-85,20),
                      small_font_.Get(),p.muted);
                label(values[i],box(contentX+45,y+26,contentWidth-85,26),
                      body_font_.Get(),p.accent);
            }
            label(L"Exclusive: Q shoulder swap  |  O cinematic orbit  |  P precision look",
                  box(contentX+26,height-114,contentWidth-52,28),
                  small_font_.Get(),p.muted);
            label(L"F focus lock  |  R reset  |  Mouse drag to orbit in World",
                  box(contentX+26,height-83,contentWidth-52,28),
                  small_font_.Get(),p.muted);
            return;
        }
        label(L"GRAPHICS + FPS  /  DIRECTX 12",
              box(contentX+28,113,contentWidth-225,38),title_font_.Get(),p.text);
        label(L"Renderer controls - click a row or press 1 through 7",
              box(contentX+28,153,contentWidth-55,28),small_font_.Get(),p.muted);
        const wchar_t* names[]={
            L"1  QUALITY PRESET",L"2  WORLD RENDER SCALE",
            L"3  FRAME RATE LIMIT",L"4  VERTICAL SYNC",
            L"5  PBR MATERIAL LIGHTING",L"6  AVATAR PREVIEW MOTION",
            L"7  SOFT SHADOW MAPPING"
        };
        std::wstring preset=graphics::preset_label(graphics_.preset);
        std::wstring scale=std::to_wstring(graphics_.render_scale_percent)+L"%";
        std::wstring fps=graphics_.fps_limit==0?L"UNLIMITED":
                         std::to_wstring(graphics_.fps_limit)+L" FPS";
        const std::wstring values[]={
            preset,scale,fps,graphics_.vsync?L"ON":L"OFF",
            graphics_.pbr?L"GGX METALLIC / ROUGHNESS":L"OFF / BASIC LAMBERT",
            graphics_.animate_avatar?L"ON":L"OFF",
            graphics_.shadows?L"DX12 1024px + 3x3 PCF":L"OFF / SKIP SHADOW PASS"
        };
        for(unsigned i=0;i<7;++i) {
            const float y=186.f+55.f*static_cast<float>(i);
            round_rect(box(contentX+24,y,contentWidth-48,51),14.f,p.surface);
            label(names[i],box(contentX+45,y+4,contentWidth-85,20),
                  small_font_.Get(),p.muted);
            label(values[i],box(contentX+45,y+26,contentWidth-85,26),
                  body_font_.Get(),p.accent);
        }
        label(L"Presets alter actual geometry + lighting. Render scale changes DX12 pixels.",
              box(contentX+26,height-114,contentWidth-52,28),
              small_font_.Get(),p.muted);
        label(L"No fake DLSS, frame generation, or ray tracing toggles.",
              box(contentX+26,height-83,contentWidth-52,28),
              small_font_.Get(),p.muted);
        return;
    }

    if(page_==6 || page_==7) {
        const bool creator=page_==6;
        round_rect(box(contentX,90,contentWidth,height-178),30.f,p.elevated);
        label(creator?L"KAMI CREATOR STUDIO":L"KAMI PHOTO STUDIO",
              box(contentX+28,105,contentWidth-56,44),title_font_.Get(),p.text);
        label(creator?L"Local mesh inspection / DX12 preview":
                      L"Live DX12 color grading / PNG export",
              box(contentX+28,151,contentWidth-55,27),small_font_.Get(),p.accent);
        const float panel=width-359.f;
        round_rect(box(panel,188,326.f,height-306),20.f,p.surface);
        if(creator) {
            label(L"MESH PREVIEW",box(panel+18,204,290,30),body_font_.Get(),p.text);
            round_rect(box(panel+18,249,290,43),13.f,p.accent_soft);
            label(L"IMPORT OBJ   [I]",box(panel+33,260,260,26),
                  body_font_.Get(),p.accent);
            round_rect(box(panel+18,301,290,42),13.f,p.accent_soft);
            label(L"RESTORE SCENE   [R]",box(panel+33,311,260,26),
                  body_font_.Get(),p.accent);
            label(L"Audit result",box(panel+19,365,285,24),small_font_.Get(),p.muted);
            if(world_renderer_.creator_preview_active()) {
                label(creator_preview_.audit.valid?L"VALID TRIANGLE GEOMETRY":
                       L"INVALID MESH",
                      box(panel+19,398,288,31),body_font_.Get(),p.accent);
                label(L"Triangles: "+std::to_wstring(creator_preview_.audit.triangles),
                      box(panel+19,442,290,25),small_font_.Get(),p.text);
                label(L"Positions: "+std::to_wstring(creator_preview_.source_positions),
                      box(panel+19,470,290,25),small_font_.Get(),p.text);
                label(L"Source polygons: "+std::to_wstring(creator_preview_.source_polygons),
                      box(panel+19,498,290,25),small_font_.Get(),p.text);
                if(height>=720.f) {
                    // Clearly labelled offline estimate, not an SL fee quote.
                    const auto estimate=viewer::estimate_scene_cost(
                        2048,2048,2,creator_preview_.audit.triangles,512);
                    label(L"2 x 2K textures + mesh estimate:",
                          box(panel+19,532,290,23),small_font_.Get(),p.muted);
                    label(std::to_wstring(estimate.total_mb)+L" MiB uncompressed",
                          box(panel+19,559,290,24),small_font_.Get(),p.accent);
                }
            } else {
                label(L"NO MODEL SELECTED",box(panel+19,398,290,31),
                      body_font_.Get(),p.muted);
            }
            label(creator_message_,box(contentX+30,height-109,contentWidth-60,32),
                  small_font_.Get(),p.accent);
            label(L"Local preview only; simulator upload not connected.",
                  box(contentX+30,height-76,contentWidth-60,28),
                  small_font_.Get(),p.muted);
        } else {
            label(L"PHOTO CONTROLS",box(panel+18,204,290,30),
                  body_font_.Get(),p.text);
            round_rect(box(panel+18,245,290,43),13.f,p.accent_soft);
            label(L"CAPTURE PNG",box(panel+34,254,260,29),
                  body_font_.Get(),p.accent);
            const wchar_t* names[]={
                L"1  STYLE",L"2  EXPOSURE",L"3  CONTRAST",
                L"4  SATURATION",L"5  VIGNETTE",L"6  LIVE GRADING",L"7  CAMERA SHOT"
            };
            const std::wstring values[]={
                photo::preset_name(photo_settings_.preset),
                std::to_wstring(photo_settings_.exposure_tenths)+L" / 10 EV",
                std::to_wstring(photo_settings_.contrast_percent)+L"%",
                std::to_wstring(photo_settings_.saturation_percent)+L"%",
                std::to_wstring(photo_settings_.vignette_percent)+L"%",
                photo_settings_.enabled?L"ON":L"OFF",
                photo_shot_==0?L"FULL BODY":
                photo_shot_==1?L"PORTRAIT":
                photo_shot_==2?L"DETAIL":L"WIDE"
            };
            for(unsigned i=0;i<7;++i) {
                const float y=300.f+32.f*static_cast<float>(i);
                round_rect(box(panel+16,y,294,30),9.f,p.elevated);
                label(names[i],box(panel+27,y+4,150,26),
                      small_font_.Get(),p.muted);
                label(values[i],box(panel+174,y+5,130,26),
                      small_font_.Get(),p.accent);
            }
            label(photo_message_,box(contentX+29,height-109,contentWidth-58,30),
                  small_font_.Get(),p.accent);
            label(L"Exports the actual DX12 scene. No external filters.",
                  box(contentX+29,height-76,contentWidth-55,29),
                  small_font_.Get(),p.muted);
        }
        if(!world_ready_)label(world_initialized_?world_renderer_.error():
             L"Starting DX12 viewport...",box(contentX+30,190,400,45),
             small_font_.Get(),p.muted);
        return;
    }

    if (page_ == 1) {
        round_rect(box(contentX,90,contentWidth,height-178),30.f,p.elevated);
        label(L"WORLD  /  DIRECTX 12",box(contentX+28,105,contentWidth-55,45),
              title_font_.Get(),p.text);
        label(L"Offline KAMI scene lab: GPU geometry, lighting, depth and materials",
              box(contentX+30,151,contentWidth-50,30),small_font_.Get(),p.accent);
        const std::wstring fps_label=L"FPS: "+std::to_wstring(fps_display_);
        label(fps_label,box(contentX+contentWidth-130,116,112,28),
              small_font_.Get(),p.accent);
        if (!world_ready_) {
            label(world_initialized_?world_renderer_.error():L"Loading DX12 viewport...",
                  box(contentX+35,234,contentWidth-70,55),body_font_.Get(),p.muted);
        }
        label(L"Drag: orbit  |  Wheel: zoom  |  Q: shoulder  |  R: recenter  |  O: cinematic",
              box(contentX+29,height-103,contentWidth-50,30),small_font_.Get(),p.muted);
        std::wstring perf=L"CPU "+std::to_wstring(static_cast<unsigned>(last_performance_.process_cpu_percent))+
            L"%  |  RAM "+std::to_wstring(last_performance_.process_ram_mb)+L"MB";
        if(last_performance_.gpu_readable)
            perf+=L"  |  DX12 VRAM "+std::to_wstring(last_performance_.gpu_vram_used_mb)+
                  L"/"+std::to_wstring(last_performance_.gpu_vram_budget_mb)+L"MB";
        label(perf,box(contentX+28,height-136,contentWidth-48,24),small_font_.Get(),p.accent);
        label(L"Procedural test scene only. SL region and avatar transport: not connected.",
              box(contentX+29,height-71,contentWidth-50,30),small_font_.Get(),p.muted);
        return;
    }

    if(page_==2 || page_==3 || page_==4) {
        round_rect(box(contentX,90,contentWidth,height-178),29.f,p.elevated);
        label(kPages[page_],box(contentX+28,111,contentWidth-55,42),
              title_font_.Get(),p.text);
        label(L"ORIGINAL KAMI MODULE READINESS / HONEST OFFLINE STATUS",
              box(contentX+28,151,contentWidth-50,28),small_font_.Get(),p.accent);
        constexpr viewer::Feature avatar[]={
            viewer::Feature::AvatarAppearance,viewer::Feature::Gestures,
            viewer::Feature::OutfitDrafts,viewer::Feature::Camera
        };
        constexpr viewer::Feature inventory[]={
            viewer::Feature::Inventory,viewer::Feature::SmartSort,
            viewer::Feature::LandmarkDrafts,viewer::Feature::Assets
        };
        constexpr viewer::Feature messages[]={
            viewer::Feature::NearbyChat,viewer::Feature::DirectMessages,
            viewer::Feature::Friends,viewer::Feature::Groups
        };
        const viewer::Feature* selected=page_==2?avatar:page_==3?inventory:messages;
        const auto& catalog=viewer::feature_catalog();
        for(unsigned i=0;i<4;++i) {
            const float y=206.f+73.f*static_cast<float>(i);
            round_rect(box(contentX+24,y,contentWidth-48,65),14.f,p.surface);
            const auto f=selected[i];
            for(const auto& entry:catalog) {
                if(entry.feature!=f)continue;
                label(entry.label,box(contentX+42,y+8,contentWidth-85,28),
                      body_font_.Get(),p.text);
                const bool local=entry.availability==viewer::Availability::OriginalLocalReady;
                label(local?L"Local code available / no SL server changes":
                      entry.availability==viewer::Availability::AssetPipelineRequired?
                      L"Requires live assets + avatar decoder":
                      L"Requires authenticated Second Life network adapter",
                      box(contentX+42,y+36,contentWidth-86,24),
                      small_font_.Get(),local?p.accent:p.muted);
                break;
            }
        }
        label(L"Account login, remote chat, editing and inventory actions are not connected.",
              box(contentX+28,height-105,contentWidth-48,31),
              small_font_.Get(),p.muted);
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
    if(messenger_edit_brush_)DeleteObject(messenger_edit_brush_);
    messenger_edit_brush_=CreateSolidBrush(
        light_?RGB(247,250,255):RGB(27,40,64));
    if(messenger_edit_)InvalidateRect(messenger_edit_,nullptr,TRUE);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Window::select_page(unsigned index) {
    if (index >= kPages.size()) return;
    const bool was_world=(page_==1 || page_==6 || page_==7);
    const bool next_world=(index==1 || index==6 || index==7);
    if(was_world && !next_world && high_resolution_timer_) {
        (void)timeEndPeriod(1);
        high_resolution_timer_=false;
    }
    page_ = index;
    if(index==1 && world_renderer_.creator_preview_active()) {
        world_renderer_.clear_creator_preview();
        creator_preview_={};creator_file_.clear();
        creator_message_=L"Choose a local OBJ file to preview.";
    }
    auto look=photo_settings_;
    if(index!=7)look.enabled=false;
    world_renderer_.set_photo_settings(look);
    if(next_world && !high_resolution_timer_)
        high_resolution_timer_=timeBeginPeriod(1)==0;
    fps_frames_=0;
    fps_display_=0;
    fps_epoch_=std::chrono::steady_clock::now();
    next_frame_={};
    update_viewport();
    update_messenger_composer();
    InvalidateRect(hwnd_, nullptr, FALSE);
}

bool Window::wants_realtime_render() const noexcept {
    return (page_ == 1 || page_ == 6 || page_ == 7) && world_ready_ && hwnd_ &&
           IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
}

void Window::tick() {
    if(!wants_realtime_render()) return;
    using Clock=std::chrono::steady_clock;
    const auto now=Clock::now();
    if(graphics_.fps_limit!=0 && now<next_frame_) return;
    world_renderer_.draw();
    if(page_==7 && world_renderer_.capture_status()!=prior_capture_) {
        prior_capture_=world_renderer_.capture_status();
        if(prior_capture_==world::CaptureStatus::Saved)
            photo_message_=L"PNG saved to selected location.";
        else if(prior_capture_==world::CaptureStatus::Failed)
            photo_message_=L"Capture failed. Check storage and DX12 availability.";
        InvalidateRect(hwnd_,nullptr,FALSE);
    }
    ++fps_frames_;
    if(fps_epoch_==Clock::time_point{}) fps_epoch_=now;
    const auto elapsed=std::chrono::duration<double>(now-fps_epoch_).count();
    if(elapsed>=1.0) {
        fps_display_=static_cast<unsigned>(static_cast<double>(fps_frames_)/elapsed+0.5);
        fps_frames_=0;
        fps_epoch_=now;
        last_performance_=performance_monitor_.sample(world_renderer_.native_device());
        if(adaptive_.sample(graphics_,static_cast<float>(fps_display_),
                            static_cast<float>(elapsed))) {
            world_renderer_.set_graphics(graphics_);
            world_ready_=world_renderer_.ready();
            next_frame_={};
        }
        InvalidateRect(hwnd_,nullptr,FALSE);
    }
    if(graphics_.fps_limit!=0) {
        const auto duration=std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(1.0/static_cast<double>(graphics_.fps_limit)));
        next_frame_=now+duration;
    }
}

void Window::advance_graphics(unsigned row) {
    switch(row) {
    case 0:
        graphics_.preset=static_cast<graphics::Preset>(
            (static_cast<unsigned>(graphics_.preset)+1U)%4U);
        if(graphics_.preset==graphics::Preset::Potato) {
            graphics_.render_scale_percent=50;
            graphics_.pbr=false;graphics_.shadows=false;
            graphics_.contact_ambient=false;
            graphics_.atmosphere_haze=false;
            graphics_.diamond_glints=false;
        } else if(graphics_.preset==graphics::Preset::Quality) {
            graphics_.render_scale_percent=100;
            graphics_.pbr=true;graphics_.shadows=true;
            graphics_.contact_ambient=true;
            graphics_.diamond_glints=true;
        } else if(graphics_.preset==graphics::Preset::Performance) {
            graphics_.render_scale_percent=75;
            graphics_.shadows=false;graphics_.atmosphere_haze=false;
        } else {
            graphics_.render_scale_percent=100;
        }
        adaptive_.reset();
        break;
    case 1:
        graphics_.render_scale_percent=
            graphics_.render_scale_percent==100?50:
            graphics_.render_scale_percent==50?75:100;
        break;
    case 2:
        switch(graphics_.fps_limit) {
        case 30: graphics_.fps_limit=60; break;
        case 60: graphics_.fps_limit=120; break;
        case 120: graphics_.fps_limit=144; break;
        case 144: graphics_.fps_limit=0; break;
        default: graphics_.fps_limit=30; break;
        }
        break;
    case 3: graphics_.vsync=!graphics_.vsync; break;
    case 4: graphics_.pbr=!graphics_.pbr; break;
    case 5: graphics_.animate_avatar=!graphics_.animate_avatar; break;
    case 6: graphics_.shadows=!graphics_.shadows; break;
    default: return;
    }
    graphics_=graphics::normalize(graphics_);
    graphics::save(graphics_);
    world_renderer_.set_graphics(graphics_);
    world_ready_=!world_initialized_ || world_renderer_.ready();
    next_frame_={};
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::advance_graphics_effects(unsigned row) {
    switch(row) {
    case 0: graphics_.auto_quality=!graphics_.auto_quality; adaptive_.reset(); break;
    case 1:
        graphics_.target_fps=graphics_.target_fps==30?60:
            graphics_.target_fps==60?90:
            graphics_.target_fps==90?120:
            graphics_.target_fps==120?144:30;
        adaptive_.reset();
        break;
    case 2: graphics_.atmosphere_haze=!graphics_.atmosphere_haze; break;
    case 3: graphics_.contact_ambient=!graphics_.contact_ambient; break;
    case 4: graphics_.diamond_glints=!graphics_.diamond_glints; break;
    default: return;
    }
    graphics_=graphics::normalize(graphics_);
    graphics::save(graphics_);
    world_renderer_.set_graphics(graphics_);
    world_ready_=!world_initialized_ || world_renderer_.ready();
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::advance_camera(unsigned row) {
    switch(row) {
    case 0:
        camera_settings_.mouse_sensitivity=
            camera_settings_.mouse_sensitivity>=100?25:
            camera_settings_.mouse_sensitivity+25;
        break;
    case 1:
        camera_settings_.smoothing=
            camera_settings_.smoothing==0?15:
            camera_settings_.smoothing==15?30:
            camera_settings_.smoothing==30?50:
            camera_settings_.smoothing==50?75:0;
        break;
    case 2:
        camera_settings_.follow_distance_cm=
            camera_settings_.follow_distance_cm==200?300:
            camera_settings_.follow_distance_cm==300?400:
            camera_settings_.follow_distance_cm==400?600:
            camera_settings_.follow_distance_cm==600?800:200;
        break;
    case 3: camera_settings_.collision=!camera_settings_.collision; break;
    case 4: camera_settings_.auto_recenter=!camera_settings_.auto_recenter; break;
    case 5: camera_settings_.head_tracking=!camera_settings_.head_tracking; break;
    case 6: camera_settings_.camera_shake=!camera_settings_.camera_shake; break;
    default: return;
    }
    camera_settings_=camera::normalize(camera_settings_);
    camera::save(camera_settings_);
    world_renderer_.set_camera_settings(camera_settings_);
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::advance_photo(unsigned row) {
    switch(row) {
    case 0:
        photo_settings_=photo::for_preset(static_cast<photo::Preset>(
            (static_cast<unsigned>(photo_settings_.preset)+1U)%4U));
        break;
    case 1:
        photo_settings_.exposure_tenths=
            photo_settings_.exposure_tenths>=10?-10:
            photo_settings_.exposure_tenths+2;
        break;
    case 2:
        photo_settings_.contrast_percent=
            photo_settings_.contrast_percent>=160?60:
            photo_settings_.contrast_percent+10;
        break;
    case 3:
        photo_settings_.saturation_percent=
            photo_settings_.saturation_percent>=140?0:
            photo_settings_.saturation_percent+20;
        break;
    case 4:
        photo_settings_.vignette_percent=
            photo_settings_.vignette_percent>=80?0:
            photo_settings_.vignette_percent+10;
        break;
    case 5: photo_settings_.enabled=!photo_settings_.enabled;break;
    case 6:
        photo_shot_=(photo_shot_+1U)%4U;
        world_renderer_.apply_studio_shot(
            static_cast<camera::StudioShot>(photo_shot_));
        break;
    default: return;
    }
    photo_settings_=photo::normalize(photo_settings_);
    photo::save(photo_settings_);
    world_renderer_.set_photo_settings(photo_settings_);
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::open_creator_obj() {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=hwnd_;
    dialog.lpstrFile=path;
    dialog.nMaxFile=static_cast<DWORD>(std::size(path));
    dialog.lpstrFilter=L"Wavefront OBJ (*.obj)\0*.obj\0All files (*.*)\0*.*\0\0";
    dialog.nFilterIndex=1;
    dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(!GetOpenFileNameW(&dialog))return;
    creator::Preview imported;
    std::string problem;
    if(!creator::import_obj_file(path,imported,problem)) {
        creator_message_=L"Import failed: "+
            std::wstring(problem.begin(),problem.end());
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(!world_renderer_.set_creator_preview(imported.scene)) {
        creator_message_=L"DX12 could not allocate preview geometry.";
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    creator_preview_=std::move(imported);
    creator_file_=path;
    creator_message_=L"OBJ loaded into KAMI DX12 preview.";
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::save_photo_png() {
    wchar_t path[32768]=L"KAMI-Photo.png";
    OPENFILENAMEW dialog{};
    dialog.lStructSize=sizeof(dialog);
    dialog.hwndOwner=hwnd_;
    dialog.lpstrFile=path;
    dialog.nMaxFile=static_cast<DWORD>(std::size(path));
    dialog.lpstrFilter=L"PNG Image (*.png)\0*.png\0\0";
    dialog.lpstrDefExt=L"png";
    dialog.nFilterIndex=1;
    dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_EXPLORER;
    if(!GetSaveFileNameW(&dialog))return;
    if(world_renderer_.capture_png(path)) {
        photo_message_=L"DX12 capture queued; PNG output will be written.";
        prior_capture_=world::CaptureStatus::Pending;
    } else photo_message_=L"DX12 renderer not ready for capture.";
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::click(float x, float y) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float width = static_cast<float>(client.right);
    if (x >= width-195.f && x <= width-32.f && y >= 23.f && y < 67.f) {
        toggle_appearance();
        return;
    }
    if(page_==6 && x>=width-341.f && x<=width-51.f) {
        if(y>=249.f && y<292.f) { open_creator_obj(); return; }
        if(y>=301.f && y<343.f) {
            world_renderer_.clear_creator_preview();
            creator_preview_={};creator_file_.clear();
            creator_message_=L"Original scene restored.";
            InvalidateRect(hwnd_,nullptr,FALSE);
            return;
        }
    }
    if(page_==7 && x>=width-341.f && x<=width-51.f) {
        if(y>=245.f && y<288.f) { save_photo_png(); return; }
        for(unsigned i=0;i<7;++i) {
            const float top=300.f+32.f*static_cast<float>(i);
            if(y>=top && y<top+30.f) { advance_photo(i); return; }
        }
    }
    if(page_==5 && x>=width-405.f && x<width-239.f && y>=106.f && y<154.f) {
        graphics_effects_page_=!graphics_effects_page_;
        camera_settings_page_=false;
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(page_==5 && x>=width-220.f && x<width-44.f && y>=106.f && y<154.f) {
        camera_settings_page_=!camera_settings_page_;
        graphics_effects_page_=false;
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(page_==5 && x>=303.f && x<width-49.f) {
        for(unsigned i=0;i<7;++i) {
            const float top=186.f+55.f*static_cast<float>(i);
            if(y>=top && y<top+51.f) {
                if(camera_settings_page_) advance_camera(i);
                else if(graphics_effects_page_) advance_graphics_effects(i);
                else advance_graphics(i);
                return;
            }
        }
    }
    if (x >= 30.f && x <= 242.f) {
        for (unsigned index = 0; index < kPages.size(); ++index) {
            const float top = 151.f + 50.f*static_cast<float>(index);
            if (y >= top && y < top+44.f) {
                select_page(index);
                return;
            }
        }
    }
}
} // namespace kami::app
