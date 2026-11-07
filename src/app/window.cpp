#include "kami/window.hpp"
#include "kami/appearance.hpp"
#include "kami/features/viewer_foundation.hpp"
#include "kami/features/inventory.hpp"
#include "kami/diagnostics/seven_systems.hpp"
#include "kami/world/scene_diff.hpp"

#include <windowsx.h>
#include <wincred.h>
#include <objbase.h>
#include <shellapi.h>
#include <future>
#include <thread>
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
#include <cmath>

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
    L"KAMI viewer workspace. Open WORLD to enter the live DX12 scene.",
    L"Live Second Life region streaming through the DX12 world renderer.",
    L"Native avatar locomotion, gait and terrain contact. Live avatar assets pending.",
    L"Inventory is not connected. No inventory items can be changed.",
    L"Messages and delivery receipts are not implemented yet.",
    L"Change appearance with the button above or press T.",
    L"Local OBJ mesh preview and geometry validation.",
    L"Native DX12 live grading and PNG exports."
};
constexpr std::array<std::wstring_view, 8> kExplanations = {
    L"Written for KAMI  /  Native Direct2D + DirectWrite",
    L"DX12 region terrain, decoded meshes, texture layers and PBR materials; incomplete assets are reported.",
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
    sl_circuit_.set_maximum_world_detail(graphics_.preset==graphics::Preset::Quality);
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
    SetWindowLongPtrW(messenger_edit_,GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(this));
    messenger_edit_original_=reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(messenger_edit_,GWLP_WNDPROC,
                           reinterpret_cast<LONG_PTR>(&Window::messenger_edit_dispatch)));
    if(!messenger_edit_original_)return false;
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

LRESULT CALLBACK Window::messenger_edit_dispatch(HWND hwnd,UINT message,
                                                   WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(self && message==WM_KEYDOWN && wp==VK_RETURN) {
        self->messenger_send();
        return 0;
    }
    if(self && message==WM_CHAR && wp==L'\r')return 0;
    return self && self->messenger_edit_original_?
        CallWindowProcW(self->messenger_edit_original_,hwnd,message,wp,lp):
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
                InvalidateRect(hwnd_,nullptr,FALSE);
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
        if(wp==203) {
            update_sl_connection();
            return 0;
        }
        if (wp == 202) {
            const auto state=shader_warmup_.snapshot();
            if (page_ == 0) InvalidateRect(hwnd_, nullptr, FALSE);
            if (state.result != world::WarmupResult::Idle &&
                state.result != world::WarmupResult::Optimizing)
                (void)KillTimer(hwnd_, 202);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if ((page_ == 1 || page_ == 2 || page_ == 6 || page_ == 7) && world_ready_) {
            world_renderer_.zoom(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -0.7f : 0.7f);
            return 0;
        }
        break;
    case WM_CHAR:
        if(page_==3&&inventory_search_focused_){
            if(wp==8){if(!inventory_search_.empty())inventory_search_.pop_back();}
            else if(wp>=32&&wp<127&&inventory_search_.size()<64)inventory_search_.push_back(static_cast<wchar_t>(wp));
            InvalidateRect(hwnd_,nullptr,FALSE);return 0;
        }
        break;
    case WM_KEYDOWN:
        if(page_==3){
            if(wp=='R'){start_inventory_refresh();return 0;}
            if(wp=='G'){inventory_group_creators_=!inventory_group_creators_;if(inventory_group_creators_)inventory_sort_mode_=1U;InvalidateRect(hwnd_,nullptr,FALSE);return 0;}
            if(wp==VK_ESCAPE){inventory_search_focused_=false;inventory_search_.clear();InvalidateRect(hwnd_,nullptr,FALSE);return 0;}
            if(wp==VK_BACK&&!inventory_search_focused_){
                const auto root=sl_circuit_.inventory_root_id();
                if(inventory_current_folder_!=root){
                    for(const auto& entry:inventory_root_snapshot_.entries)
                        if(entry.folder&&entry.id==inventory_current_folder_){inventory_current_folder_=entry.parent_id;break;}
                    inventory_selected_entry_.clear();InvalidateRect(hwnd_,nullptr,FALSE);
                }
                return 0;
            }
            if(wp==VK_RETURN)inventory_search_focused_=false;
        }
        if(page_==1&&world_ready_&&wp=='V'){
            // Ignore auto-repeat: only one scenic switch per physical press.
            if((static_cast<unsigned long long>(lp)&(1ULL<<30))==0ULL)
                world_renderer_.toggle_landscape_view();
            InvalidateRect(hwnd_,nullptr,FALSE);
            return 0;
        }
        if(wp==VK_F9){
            const bool open=!seven_systems_open_;
            select_page(0);
            seven_systems_open_=open;
            InvalidateRect(hwnd_,nullptr,FALSE);
            return 0;
        }
        if(page_==0 && !seven_systems_open_ && wp=='L') {
            begin_sl_login();
            return 0;
        }
        if(page_==0 && !seven_systems_open_ && wp=='N') {
            check_sl_tls_connection();
            return 0;
        }
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
        if(page_==2 && wp=='B') {
            cycle_body_physics();
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
        if ((page_ == 1 || page_ == 2) && world_ready_) {
            // Movement keys are sampled continuously while the viewport runs;
            // they must not also orbit or zoom the camera on key-repeat.
            switch(wp) {
            case 'W':case 'A':case 'S':case 'D':
            case 'Z':case 'C':case VK_UP:case VK_DOWN:
            case VK_LEFT:case VK_RIGHT:case VK_SPACE:case VK_SHIFT:
                return 0;
            }
        }
        if ((page_ == 1 || page_ == 2 || page_ == 6 || page_ == 7) && world_ready_) {
            switch (wp) {
            case VK_LEFT: case 'A': world_renderer_.orbit(-.11f, 0.f); return 0;
            case VK_RIGHT: case 'D': world_renderer_.orbit(.11f, 0.f); return 0;
            case VK_UP: world_renderer_.orbit(0.f, .35f); return 0;
            case VK_DOWN: world_renderer_.orbit(0.f, -.35f); return 0;
            case 'W': world_renderer_.zoom(-.7f); return 0;
            case 'S': world_renderer_.zoom(.7f); return 0;
            case 'H':
                if(page_==1||page_==2) {
                    world_renderer_.reset_avatar();
                    InvalidateRect(hwnd_,nullptr,FALSE);
                }
                return 0;
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
        (void)KillTimer(hwnd_,203);
        sl_circuit_.stop();
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
    if (!hwnd_ || (page_ != 1 && page_ != 2 && page_ != 6 && page_ != 7)) {
        if (viewport_) ShowWindow(viewport_, SW_HIDE);
        return;
    }
    RECT client{};
    GetClientRect(hwnd_, &client);
    const int x=302, y=188;
    const int width=std::max<int>(128, client.right-
        ((page_==2)?700:((page_==6||page_==7)?690:350)));
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
    label(L"DEVELOPER TOOLS [F9]",box(41,height-93,200,25),
          small_font_.Get(),p.accent);
    label(L"KAMI VIEWER / DX12",box(42,height-65,189,25),small_font_.Get(),p.muted);

    const float contentX = 279.f;
    const float contentWidth = width - 305.f;
    label(L"KAMI  /  SECOND LIFE VIEWER",box(contentX,28,620,24),
          small_font_.Get(),p.muted);
    round_rect(box(width-195,23,163,44),22.f,p.surface);
    label(light_?L"LIGHT MODE   [T]":L"DARK MODE   [T]",
          box(width-176,36,145,24),small_font_.Get(),p.accent);

    if(seven_systems_open_){
        round_rect(box(contentX,90,contentWidth,height-154),27.f,p.elevated);
        label(L"KAMI / SEVEN-IN-ONE TEST",
              box(contentX+26,108,contentWidth-52,37),
              title_font_.Get(),p.text);
        label(L"F9 closes | Real evidence, local previews, and missing SL systems",
              box(contentX+28,147,contentWidth-55,26),
              small_font_.Get(),p.accent);
        const diagnostics::Evidence ev{
            static_cast<std::uint32_t>(
                world_renderer_.uploaded_root_bounds()+
                world_renderer_.uploaded_root_primitive_shapes()+
                world_renderer_.uploaded_linked_primitive_shapes()),
            world_renderer_.region_geometry_active(),
            world_renderer_.live_avatar_placeholder_active(),
            sl_circuit_.movement_control_packets_sent(),
            sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed,
            world_renderer_.ready()
        };
        const auto checks=diagnostics::evaluate(ev);
        const float rowHeight=std::clamp((height-270.f)/7.f,47.f,70.f);
        for(unsigned i=0;i<checks.size();++i){
            const float y=180.f+rowHeight*static_cast<float>(i);
            round_rect(box(contentX+23,y,contentWidth-47,rowHeight-4.f),
                       12.f,p.surface);
            label(checks[i].title,
                  box(contentX+36,y+3,contentWidth-190,22),
                  small_font_.Get(),p.text);
            label((i==4U &&
                   sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed)?
                  L"PARTIAL":diagnostics::stage_label(checks[i].stage),
                  box(contentX+contentWidth-153,y+3,128,22),
                  small_font_.Get(),p.accent);
            std::wstring detail=checks[i].details;
            if(i==0U){
                // Decoded frames can contain ZERO eligible root objects.
                // Never imply Frame OK equals visible object count.
                detail=L"Root "+std::to_wstring(
                    sl_circuit_.decoded_root_count())+
                    L" | Linked "+std::to_wstring(
                    sl_circuit_.retained_linked_objects())+
                    L" | AMBER "+std::to_wstring(
                    world_renderer_.uploaded_root_bounds())+
                    L" | SOLID "+std::to_wstring(
                    world_renderer_.uploaded_root_primitive_shapes()+
                    world_renderer_.uploaded_linked_primitive_shapes()+
                    world_renderer_.uploaded_live_meshes())+
                    L" | Mesh DRAW "+std::to_wstring(
                        world_renderer_.uploaded_live_meshes())+
                    L" | Missing "+std::to_wstring(
                    sl_circuit_.pending_object_requests())+
                    L"/"+std::to_wstring(protocol::sim::Circuit::kMaxMissingObjectRequests)+
                    L" | Requested "+std::to_wstring(
                    sl_circuit_.requested_object_ids());
            }
            if(i==1U){
                detail=L"Live terrain | textures "+std::to_wstring(world_renderer_.terrain_texture_layers())+
                    L" / PBR "+std::to_wstring(world_renderer_.terrain_pbr_layers())+
                    L" | IBL "+std::to_wstring(world_renderer_.environment_updates())+
                    L" | material HTTP "+std::to_wstring(sl_circuit_.material_last_http())+
                    L" / "+std::to_wstring(sl_circuit_.material_last_bytes())+L"B / "+protocol::pbr::asset_format_label(sl_circuit_.material_last_format())+L" / "+
                    (sl_circuit_.material_last_http()==200U&&sl_circuit_.material_last_bytes()>0U?
                        protocol::pbr::decode_error_label(sl_circuit_.material_last_decode_error()):L"not decoded")+
                    L" | failed HTTP "+std::to_wstring(sl_circuit_.material_http_failures())+
                    L" / decode "+std::to_wstring(sl_circuit_.material_decode_failures());
            }
            if(i==3U && sl_circuit_.state()==
               protocol::sim::CircuitState::MovementConfirmed){
                detail=L"AgentUpdate packets "+
                    std::to_wstring(sl_circuit_.agent_updates_sent())+
                    L" | control packets "+
                    std::to_wstring(sl_circuit_.movement_control_packets_sent())+
                    L" | full teleport/live pose sync pending";
            }
            if(i==4U && sl_circuit_.state()==
               protocol::sim::CircuitState::MovementConfirmed){
                detail=L"Nearby chat live UDP | TX packets "+
                    std::to_wstring(sl_circuit_.sent_chat_packets())+
                    L" | group/IM/inventory pending";
            }
            if(i==2U){
                detail+=L" | AvatarAppearance packets "+
                    std::to_wstring(sl_circuit_.avatar_appearance_packets())+
                    L" | Own visual params "+
                    std::to_wstring(sl_circuit_.own_avatar_visual_params());
                if(const auto* appearance=sl_circuit_.own_avatar_appearance()){
                    detail+=L" | TE bytes "+
                        std::to_wstring(appearance->texture_entry_size)+
                        L" | appearance payload retained";
                }else{
                    detail+=L" | waiting for own appearance payload";
                }
                detail+=L" | Bento body/rig render still pending";
            }
            label(detail,
                  box(contentX+36,y+25,contentWidth-76,20),
                  small_font_.Get(),p.muted);
            if(i==0U && rowHeight>=62.f){
                // R66: classify mesh instance losses ahead of all raw
                // frame counts. This row previously overflowed the panel,
                // hiding the reason hundreds of mesh references could
                // produce only a few DX12-visible objects.
                const auto mesh_stats=sl_circuit_.mesh_asset_stats();
                const std::wstring reasons=
                    L"MeshID "+std::to_wstring(sl_circuit_.mesh_object_count())+
                    L" | LOD "+std::to_wstring(sl_circuit_.decoded_mesh_assets().size())+
                    L" | Near "+std::to_wstring(world_renderer_.mesh_eligible_instances())+
                    L" | NoLOD "+std::to_wstring(world_renderer_.mesh_missing_lod_instances())+
                    L" | Parent "+std::to_wstring(world_renderer_.mesh_waiting_for_parent())+
                    L" | Far "+std::to_wstring(world_renderer_.mesh_out_of_range_instances())+
                    L" | BadPose "+std::to_wstring(world_renderer_.mesh_invalid_pose_instances())+
                    L" | Undrawn "+std::to_wstring(world_renderer_.mesh_undrawn_instances())+
                    L" | Q "+std::to_wstring(mesh_stats.queued)+
                    L" DL "+std::to_wstring(mesh_stats.downloading)+
                    L" Decode "+std::to_wstring(mesh_stats.decoding)+
                    L" DecodedReady "+std::to_wstring(mesh_stats.ready_gpu)+
                    L" SchedulerResident "+std::to_wstring(mesh_stats.resident)+
                    L" Fail "+std::to_wstring(sl_circuit_.mesh_failure_count());
                label(reasons,box(contentX+36,y+43,
                      contentWidth-76,19),small_font_.Get(),p.accent);
            }
            if(i==5U && rowHeight>=62.f){
                // R69: the previous Parent counter included genuine missing
                // roots, avatar attachments, cycles and invalid hierarchies.
                // Distinguish these before changing recovery networking.
                const std::wstring causes=
                    L"Mesh parents M:"+std::to_wstring(world_renderer_.mesh_parent_missing_instances())+
                    L" A:"+std::to_wstring(world_renderer_.mesh_parent_avatar_instances())+
                    L" C:"+std::to_wstring(world_renderer_.mesh_parent_cycle_instances())+
                    L" I:"+std::to_wstring(world_renderer_.mesh_parent_invalid_instances())+
                    L" D:"+std::to_wstring(world_renderer_.mesh_parent_deep_instances())+
                    L" | Undrawn Alpha:"+std::to_wstring(world_renderer_.mesh_undrawn_transparent_instances())+
                    L" Geom:"+std::to_wstring(world_renderer_.mesh_undrawn_bad_geometry_instances())+
                    L" Budget:"+std::to_wstring(world_renderer_.mesh_undrawn_budget_instances())+
                    L" Global:"+std::to_wstring(world_renderer_.mesh_undrawn_global_limit())+
                    L" PerObj:"+std::to_wstring(world_renderer_.mesh_undrawn_object_limit())+
                    L" DrawDrop:"+std::to_wstring(world_renderer_.mesh_draw_count_drop())+
                    L" GPUcache:"+std::to_wstring(world_renderer_.gpu_mesh_cache_entries())+
                    L" GPUmemMB:"+std::to_wstring(world_renderer_.gpu_mesh_cache_bytes()/(1024U*1024U));
                label(causes,box(contentX+36,y+43,contentWidth-76,19),
                      small_font_.Get(),p.accent);
            }
        }
        // GPU atlas != CPU JPEG2000 cache. Keep the numbers separate
        // so test screenshots reveal when decoded images still cannot bind.
        label(L"J2C CPU "+std::to_wstring(
                  sl_circuit_.decoded_texture_count())+
              L" | DX12 GPU "+std::to_wstring(
                  world_renderer_.bound_sl_texture_assets())+
              L"/"+std::to_wstring(protocol::texture::kGPUImageLayers)+
              L" | textured triangles "+std::to_wstring(
                  world_renderer_.uploaded_textured_triangles())+
              L" | PBR assets "+std::to_wstring(sl_circuit_.decoded_materials().size())+
              L" faces "+std::to_wstring(world_renderer_.bound_pbr_faces())+
              L" failures "+std::to_wstring(sl_circuit_.material_failures())+
              L" | IBL "+std::to_wstring(world_renderer_.environment_updates())+
              L" | land "+std::to_wstring(world_renderer_.terrain_texture_layers())+
              L"/"+std::to_wstring(world_renderer_.terrain_pbr_layers())+
              L" | avatar partial",
              box(contentX+25,height-89,contentWidth-54,24),
              small_font_.Get(),p.muted);
        return;
    }

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
                if(height>=720.f &&
                   creator_preview_.skipped_degenerate_triangles>0) {
                    label(L"Skipped zero-area triangles: "+
                          std::to_wstring(creator_preview_.skipped_degenerate_triangles),
                          box(panel+19,522,290,21),small_font_.Get(),p.muted);
                }
                if(height>=720.f) {
                    // Clearly labelled offline estimate, not an SL fee quote.
                    const auto estimate=viewer::estimate_scene_cost(
                        2048,2048,2,creator_preview_.audit.triangles,512);
                    const float metricsY=
                        creator_preview_.skipped_degenerate_triangles>0?
                        547.f:532.f;
                    label(L"2 x 2K textures + mesh estimate:",
                          box(panel+19,metricsY,290,21),small_font_.Get(),p.muted);
                    label(std::to_wstring(estimate.total_mb)+L" MiB uncompressed",
                          box(panel+19,metricsY+22.f,290,21),small_font_.Get(),p.accent);
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
        // R86: visible mesh coverage, NOT a claim that the entire SL scene
        // has loaded. The simulator can discover additional objects later.
        const auto drawn=world_renderer_.uploaded_live_meshes();
        const auto pending=world_renderer_.mesh_undrawn_instances()+
                           world_renderer_.mesh_missing_lod_instances();
        const auto total=drawn+pending;
        const unsigned coverage=total?
            static_cast<unsigned>(100U*drawn/total):0U;
        const float bar_width=std::max(0.f,contentWidth-320.f);
        round_rect(box(contentX+30,184,bar_width,9),4.f,p.muted);
        if(coverage>0U)
            round_rect(box(contentX+30,184,
                bar_width*static_cast<float>(coverage)/100.f,9),
                4.f,p.accent);
        label(L"Visible mesh coverage ~"+std::to_wstring(coverage)+
              L"% (known objects)",
              box(contentX+38+bar_width,175,280,26),
              small_font_.Get(),p.accent);

        round_rect(box(contentX,90,contentWidth,height-178),30.f,p.elevated);
        label(L"WORLD  /  DIRECTX 12",box(contentX+28,105,contentWidth-55,45),
              title_font_.Get(),p.text);
        std::wstring heading_status=world_renderer_.region_geometry_active()?
            L"SL LIVE LAND / KAMI DX12 SKY + BIOMES + WATER":
            L"KAMI scene lab / awaiting live terrain";
        if(world_renderer_.live_avatar_placeholder_active()){
            heading_status+=L"  |  Local avatar facing "+
                std::to_wstring(world_renderer_.live_avatar_heading_degrees())+
                L" deg";
        }
        if(world_renderer_.landscape_view_active())
            heading_status+=L"  |  LANDSCAPE CAMERA [V]";
        else if(world_renderer_.region_geometry_active())
            heading_status+=L"  |  V: VIEW LAND";
        label(heading_status,box(contentX+30,151,contentWidth-50,30),
              small_font_.Get(),p.accent);
        const std::wstring fps_label=L"FPS: "+std::to_wstring(fps_display_);
        label(fps_label,box(contentX+contentWidth-130,116,112,28),
              small_font_.Get(),p.accent);
        if (!world_ready_) {
            label(world_initialized_?world_renderer_.error():L"Loading DX12 viewport...",
                  box(contentX+35,234,contentWidth-70,55),body_font_.Get(),p.muted);
        }
        label(L"WASD: walk/turn | V: LANDSCAPE / AVATAR | Shift: run | Space: jump",
              box(contentX+29,height-103,contentWidth-50,30),small_font_.Get(),p.muted);
        std::wstring perf=L"CPU "+std::to_wstring(static_cast<unsigned>(last_performance_.process_cpu_percent))+
            L"%  |  RAM "+std::to_wstring(last_performance_.process_ram_mb)+L"MB";
        if(last_performance_.gpu_readable)
            perf+=L"  |  DX12 VRAM "+std::to_wstring(last_performance_.gpu_vram_used_mb)+
                  L"/"+std::to_wstring(last_performance_.gpu_vram_budget_mb)+L"MB";
        label(perf,box(contentX+28,height-136,contentWidth-48,24),small_font_.Get(),p.accent);
        std::wstring world_status;
        if(sl_circuit_.movement_seen()&&
           sl_circuit_.last_controls_sent()!=0U){
            world_status=L"SL controls TX (experimental) | ";
        }
        if(world_renderer_.region_geometry_active()){
            world_status+=L"SL land vertices "+
                std::to_wstring(world_renderer_.uploaded_live_land_vertices())+
                L" | patches "+
                std::to_wstring(sl_circuit_.decoded_land_patches().size())+
                L" | SKIPPED SPIKES "+
                std::to_wstring(world_renderer_.skipped_terrain_discontinuities())+
                L" | AMBER BOUNDS "+
                std::to_wstring(world_renderer_.uploaded_root_bounds())+
                L" | SOLID ROOTS "+
                std::to_wstring(world_renderer_.uploaded_root_primitive_shapes())+
                L" | SOLID LINKED "+
                std::to_wstring(world_renderer_.uploaded_linked_primitive_shapes())+
                L" | MESH "+std::to_wstring(
                    world_renderer_.uploaded_live_meshes())+
                (world_renderer_.live_water_surface_active()?
                    L" | WATER BELOW TERRAIN":L" | NO WET LAND IN VIEW")+
                L" | avatar LOCAL";
        }else if(sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed){
            world_status+=L"Connected to region | land patches "+
                std::to_wstring(sl_circuit_.decoded_land_patches().size())+
                L" | "+(world_renderer_.error().empty()?
                         L"DX12 terrain upload pending":world_renderer_.error());
        }else {
            world_status=L"Offline procedural preview | connect to SL to receive live land";
        }
        label(world_status,box(contentX+29,height-71,contentWidth-50,30),
              small_font_.Get(),p.muted);
        return;
    }

    if(page_==4) {
        const float railX=contentX+18.f;
        const float panelX=contentX+255.f;
        const float panelW=std::max(280.f,contentWidth-277.f);
        const float cardHeight=height-178.f;
        round_rect(box(contentX,90,contentWidth,cardHeight),29.f,p.elevated);
        // Separate conversation rail: readable but unmistakably offline.
        round_rect(box(railX,109,220.f,cardHeight-38.f),22.f,p.surface);
        label(L"KAMI",box(railX+16,126,188,30),
              messenger_centered_font_.Get(),p.text);
        label(L"M E S S E N G E R",box(railX+15,154,190,22),
              messenger_centered_small_.Get(),p.accent);
        const wchar_t* tabs[]={L"DESIGN PREVIEW",L"NEARBY CHAT",L"GROUPS"};
        const bool chat_online=sl_circuit_.state()==
            protocol::sim::CircuitState::MovementConfirmed;
        const wchar_t* subtitles[]={
            L"Local demo",chat_online?L"Simulator-connected":L"Not connected",
            L"Not connected"};
        for(unsigned i=0;i<3;++i) {
            const float top=211.f+76.f*static_cast<float>(i);
            const bool selected=static_cast<unsigned>(messenger_.active())==i;
            round_rect(box(railX+13,top,194,67),16.f,
                       selected?p.accent_soft:p.elevated);
            label(tabs[i],box(railX+25,top+9,172,28),
                  messenger_centered_font_.Get(),selected?p.text:p.muted);
            label(subtitles[i],box(railX+25,top+39,172,20),
                  messenger_centered_small_.Get(),selected?p.accent:p.muted);
        }
        if(messenger_.can_send()) {
            const float replyTop=std::min(474.f,height-196.f);
            round_rect(box(railX+14,replyTop,192,38),13.f,p.accent_soft);
            label(L"ADD DEMO REPLY",box(railX+23,replyTop+4,174,29),
                  messenger_centered_small_.Get(),p.accent);
        }
        label(chat_online?L"CONNECTION: LIVE UDP":L"CONNECTION: OFFLINE",
              box(railX+20,height-153,181,25),
              messenger_centered_small_.Get(),p.muted);
        label(chat_online?
              (std::to_wstring(sl_circuit_.nearby_chat().size())+
               L" nearby messages"):L"No resident data",
              box(railX+20,height-131,181,21),
              messenger_centered_small_.Get(),p.muted);

        label(messenger::channel_title(messenger_.active()),
              box(panelX+12,115,panelW-24,37),
              messenger_centered_font_.Get(),p.text);
        label(messenger::channel_subtitle(messenger_.active()),
              box(panelX+15,150,panelW-30,22),
              messenger_centered_small_.Get(),p.muted);
        label(chat_online?L"NEARBY CHAT / SIMULATOR UDP":
              L"NOT CONNECTED TO SECOND LIFE",
              box(panelX+15,176,panelW-30,24),
              messenger_centered_small_.Get(),p.accent);

        if(messenger_.can_send()) {
            // Paint the latest entries backward so the newest are always visible.
            // Center text *within* bubbles; keep incoming/outgoing sides distinct.
            const auto& chat=messenger_.messages();
            float cursor=height-197.f;
            for(std::size_t offset=0;offset<chat.size();++offset) {
                const auto& msg=chat[chat.size()-1-offset];
                const bool system=msg.direction==messenger::Direction::System;
                const float bubblesW=std::min(395.f,panelW-45.f);
                const float bubbleH=system?33.f:
                    (msg.text.size()>96?83.f:msg.text.size()>49?66.f:49.f);
                const float needed=bubbleH+(system?13.f:25.f);
                cursor-=needed;
                if(cursor<207.f)break;
                if(system) {
                    label(msg.text,box(panelX+20,cursor,panelW-40,bubbleH),
                          messenger_centered_small_.Get(),p.muted);
                    continue;
                }
                const float px=msg.direction==messenger::Direction::Outgoing?
                    panelX+panelW-bubblesW-22.f:panelX+22.f;
                round_rect(box(px,cursor,bubblesW,bubbleH),18.f,
                           msg.direction==messenger::Direction::Outgoing?
                           p.accent_soft:p.surface);
                label(msg.text,box(px+14,cursor+5,bubblesW-28,bubbleH-10),
                      messenger_centered_font_.Get(),p.text);
                const unsigned hh=(msg.clock_minutes/60U)%24U;
                const unsigned mm=msg.clock_minutes%60U;
                wchar_t timestamp[48]{};
                (void)swprintf_s(timestamp,L"LOCAL DEMO  %02u:%02u",hh,mm);
                label(timestamp,box(px+10,cursor+bubbleH+3,bubblesW-20,18),
                      messenger_centered_small_.Get(),p.muted);
            }
            round_rect(box(panelX+11,height-164.f,panelW-23.f,58.f),
                       19.f,p.surface);
            const bool hasText=messenger_.draft().find_first_not_of(L" \t")!=
                               std::wstring::npos;
            round_rect(box(panelX+panelW-115.f,height-158.f,103.f,46.f),
                       15.f,hasText?p.accent_soft:p.elevated);
            label(L"PREVIEW",box(panelX+panelW-109.f,height-152.f,91.f,34.f),
                  messenger_centered_small_.Get(),hasText?p.accent:p.muted);
            label(L"Text stays local  /  No deliveries or read receipts",
                  box(panelX+16,height-96.f,panelW-31.f,25.f),
                  messenger_centered_small_.Get(),p.muted);
        } else if(messenger_.active()==messenger::Channel::Nearby &&
                  chat_online) {
            const auto& lines=sl_circuit_.nearby_chat();
            if(lines.empty())
                label(L"No nearby chat received yet",
                      box(panelX+28,285,panelW-56,36),
                      messenger_centered_font_.Get(),p.text);
            const auto shown=std::min<std::size_t>(lines.size(),6U);
            for(std::size_t i=0;i<shown;++i){
                const auto& msg=lines[lines.size()-shown+i];
                const float y=230.f+static_cast<float>(i)*72.f;
                if(y>height-205.f)break;
                round_rect(box(panelX+22,y,panelW-44,65),14.f,p.surface);
                std::wstring display=msg.sender+L": "+msg.message;
                if(display.size()>160U)display.resize(160U);
                label(display,box(panelX+33,y+8,panelW-66,49.f),
                      messenger_centered_small_.Get(),p.text);
            }
            round_rect(box(panelX+11,height-164.f,panelW-23.f,58.f),
                       19.f,p.surface);
            round_rect(box(panelX+panelW-115.f,height-158.f,103.f,46.f),
                       15.f,p.accent_soft);
            label(L"SEND",box(panelX+panelW-109.f,height-152.f,91.f,34.f),
                  messenger_centered_small_.Get(),p.accent);
            label(L"Packet sent != delivered/read. Only channel 0 supported.",
                  box(panelX+16,height-96.f,panelW-31.f,25.f),
                  messenger_centered_small_.Get(),p.muted);
        } else {
            label(L"NO LIVE MESSAGES AVAILABLE",
                  box(panelX+28,285,panelW-56,36),
                  messenger_centered_font_.Get(),p.text);
            label(L"This channel requires KAMI's authenticated network layer.",
                  box(panelX+30,323,panelW-60,70),
                  messenger_centered_small_.Get(),p.muted);
            label(L"No messages have been fetched or sent.",
                  box(panelX+25,401,panelW-50,30),
                  messenger_centered_small_.Get(),p.accent);
        }
        return;
    }

    if(page_==2) {
        round_rect(box(contentX,90,contentWidth,height-178),29.f,p.elevated);
        label(L"KAMI AVATAR  /  MOTION + BODY PHYSICS",
              box(contentX+28,106,contentWidth-55,42),
              title_font_.Get(),p.text);
        label(L"DX12 weighted fitted-mesh preview. Live SL wearables are not connected.",
              box(contentX+28,152,contentWidth-55,28),
              small_font_.Get(),p.accent);
        const float panelX=width-352.f;
        round_rect(box(panelX,190,324.f,height-280.f),22.f,p.surface);
        label(L"LOCAL AVATAR TELEMETRY",
              box(panelX+15,202,292,24),
              messenger_centered_small_.Get(),p.accent);
        label(rig::compatibility_label(world_renderer_.skin_compatibility().status),
              box(panelX+15,222,292,18),
              messenger_centered_small_.Get(),p.muted);
        const auto& state=world_renderer_.avatar_state();
        const std::wstring values[]={
            avatar::motion_label(state.motion),
            std::to_wstring(static_cast<int>(std::lround(state.horizontal_speed*100.f)))+
                L" cm/s",
            L"X "+std::to_wstring(static_cast<int>(std::lround(state.x*100.f)))+
                L"cm  /  Z "+std::to_wstring(static_cast<int>(std::lround(state.z*100.f)))+L"cm",
            state.grounded?L"TERRAIN CONTACT":L"AIRBORNE",
            std::to_wstring(static_cast<int>(std::lround(state.heading*57.29578f)))+
                L" degrees"
        };
        const wchar_t* titles[]={
            L"ANIMATION STATE",L"GROUND SPEED",L"TEST WORLD POSITION",
            L"GROUND PHYSICS",L"HEADING"
        };
        const unsigned rows=height>=700.f?5U:4U;
        for(unsigned i=0;i<rows;++i) {
            const float y=241.f+std::min(45.f,(height-480.f)/6.f)*static_cast<float>(i);
            round_rect(box(panelX+16,y,291.f,58.f),12.f,p.elevated);
            label(titles[i],box(panelX+24,y+6,271.f,22.f),
                  messenger_centered_small_.Get(),p.muted);
            label(values[i],box(panelX+24,y+27,271.f,27.f),
                  messenger_centered_font_.Get(),p.text);
        }
        const auto offsets=world_renderer_.detailed_body_physics_offsets();
        const auto millimeters=[](float value) {
            return std::to_wstring(static_cast<int>(std::lround(value*1000.f)));
        };
        round_rect(box(panelX+16,height-269.f,291.f,76.f),12.f,p.elevated);
        label(L"EIGHT-AXIS RIG PHYSICS (LOCAL)",
              box(panelX+21,height-263.f,280.f,19.f),
              messenger_centered_small_.Get(),p.muted);
        const auto twist_degrees=std::to_wstring(static_cast<int>(
            std::lround(offsets.at(rigphysics::Axis::ChestTwist)*57.29578f)));
        const float open_close=offsets.at(rigphysics::Axis::ButtOpenClose);
        label(L"CHEST UP "+millimeters(offsets.at(rigphysics::Axis::ChestVertical))+
              L" SIDE "+millimeters(offsets.at(rigphysics::Axis::ChestLateral))+
              L" TW "+twist_degrees+L" deg",
              box(panelX+21,height-242.f,280.f,21.f),
              messenger_centered_small_.Get(),p.accent);
        label(L"BUTT UP "+millimeters(offsets.at(rigphysics::Axis::ButtVertical))+
              L" SIDE "+millimeters(offsets.at(rigphysics::Axis::ButtLateral))+
              (open_close<0.f?L" CLOSE ":L" OPEN ")+millimeters(open_close),
              box(panelX+21,height-224.f,280.f,21.f),
              messenger_centered_small_.Get(),p.accent);
        label(L"CHEST DEPTH "+millimeters(offsets.at(rigphysics::Axis::ChestDepth))+
              L"  BELLY "+millimeters(offsets.at(rigphysics::Axis::BellyVertical))+
              L"  (mm)",
              box(panelX+21,height-205.f,280.f,18.f),
              messenger_centered_small_.Get(),p.accent);
        round_rect(box(panelX+16,height-186.f,291.f,34.f),12.f,p.accent_soft);
        label(L"BODY PHYSICS [B]: "+
              std::wstring(avatar::profile_label(world_renderer_.body_physics_profile())),
              box(panelX+23,height-181.f,278.f,26.f),
              messenger_centered_small_.Get(),p.accent);
        round_rect(box(panelX+18,height-144.f,287.f,38.f),12.f,p.accent_soft);
        label(L"RESET AVATAR POSITION  [H]",
              box(panelX+25,height-138.f,270.f,28.f),
              messenger_centered_small_.Get(),p.accent);
        label(L"W/S: forward/back  |  A/D: turn  |  Z/C: strafe",
              box(contentX+29,height-105.f,contentWidth-54,25.f),
              small_font_.Get(),p.text);
        label(L"Arrows: move/turn  |  Shift: run  |  Space: jump  |  B: physics",
              box(contentX+29,height-78.f,contentWidth-54,25.f),
              small_font_.Get(),p.muted);
        if(!world_ready_)label(world_initialized_?world_renderer_.error():
             L"Loading avatar preview...",box(contentX+30,218,510,32),
             small_font_.Get(),p.muted);
        return;
    }

    if(page_==3) {
        round_rect(box(contentX,90,contentWidth,height-178),25.f,p.elevated);
        label(L"INVENTORY",box(contentX+24,105,contentWidth-48,36),title_font_.Get(),p.text);
        label(L"Authenticated Second Life inventory / local organization preview only",box(contentX+24,141,contentWidth-48,24),small_font_.Get(),p.muted);
        const float controlsY=174.f;
        round_rect(box(contentX+22,controlsY,contentWidth-44,42),10.f,p.surface);
        label(L"Search: "+inventory_search_+(inventory_search_focused_?L"|":L""),box(contentX+34,controlsY+7,contentWidth-310,28),body_font_.Get(),inventory_search_focused_?p.accent:p.text);
        label(L"REFRESH [R]",box(contentX+contentWidth-264,controlsY+7,105,28),small_font_.Get(),p.accent);
        const wchar_t* sort_names[]={L"NAME",L"CREATOR",L"TYPE"};
        label(std::wstring(L"SORT: ")+sort_names[inventory_sort_mode_%3],box(contentX+contentWidth-151,controlsY+7,120,28),small_font_.Get(),p.accent);
        const auto& entries=inventory_root_snapshot_.entries;
        const auto root=sl_circuit_.inventory_root_id();
        if(inventory_current_folder_.empty()&&!root.empty())inventory_current_folder_=root;
        const auto visible=inventory::filter_entries(entries,inventory_current_folder_,inventory_search_,static_cast<inventory::SortMode>(inventory_sort_mode_%3));
        const auto groups=inventory::group_entries_by_creator(entries,visible);
        std::vector<inventory::Item> visible_items;
        for(const auto index:visible)if(!entries[index].folder)
            visible_items.push_back({entries[index].name,std::wstring(entries[index].creator_id.begin(),entries[index].creator_id.end()),false});
        const auto demo_matches=inventory::preview_demo_deletions(visible_items);
        label(L"BACK [BACKSPACE] / "+std::to_wstring(visible.size())+L" entries / "+std::to_wstring(groups.size())+L" creators / "+std::to_wstring(demo_matches.size())+L" demo-trial matches",box(contentX+24,222,contentWidth-48,25),small_font_.Get(),p.muted);
        const float rowTop=251.f,rowHeight=31.f;
        const float detailTop=std::max(rowTop+80.f,height-188.f);
        const auto maxRows=static_cast<unsigned>(std::max(1.f,(detailTop-rowTop)/rowHeight));
        for(unsigned row=0;row<maxRows&&row<visible.size();++row){
            const auto& e=entries[visible[row]];
            const float y=rowTop+rowHeight*static_cast<float>(row);
            round_rect(box(contentX+20,y,contentWidth-40,28),7.f,e.id==inventory_selected_entry_?p.accent_soft:p.surface);
            std::wstring line=e.folder?L"[FOLDER] ":L"[ITEM] ";line+=e.name;
            if(!e.folder&&inventory_group_creators_){line+=L"  ·  ";line+=e.creator_id.empty()?L"Unknown creator":std::wstring(e.creator_id.begin(),e.creator_id.end());}
            label(line,box(contentX+32,y+3,contentWidth-64,23),small_font_.Get(),e.id==inventory_selected_entry_?p.accent:p.text);
        }
        if(visible.empty())label(inventory_search_.empty()?L"No entries in this folder. Use Refresh after login.":L"No inventory entries match this search.",box(contentX+28,rowTop+18,contentWidth-56,30),body_font_.Get(),p.muted);
        round_rect(box(contentX+20,detailTop,contentWidth-40,height-detailTop-18),12.f,p.surface);
        const protocol::caps::InventoryEntry* selected=nullptr;
        for(const auto& e:entries)if(e.id==inventory_selected_entry_){selected=&e;break;}
        if(inventory_selected_details_valid_&&
           inventory_selected_details_.id==inventory_selected_entry_)
            selected=&inventory_selected_details_;
        if(selected){
            label(selected->name,box(contentX+34,detailTop+8,contentWidth-68,24),body_font_.Get(),p.text);
            std::wstring detail=selected->folder?L"Folder ID: ":L"Creator: ";
            if(selected->folder)detail+=std::wstring(selected->id.begin(),selected->id.end());
            else detail+=selected->creator_id.empty()?L"Unknown":std::wstring(selected->creator_id.begin(),selected->creator_id.end());
            label(detail,box(contentX+34,detailTop+34,contentWidth-68,22),small_font_.Get(),p.muted);
            if(!selected->folder){
                const auto meta=L"Asset type: "+std::to_wstring(selected->asset_type)+L" / Inventory type: "+std::to_wstring(selected->inventory_type);
                label(meta,box(contentX+34,detailTop+56,contentWidth-68,22),small_font_.Get(),p.muted);
                const std::wstring asset=selected->asset_id.empty()?L"Asset ID: unavailable":L"Asset ID: "+std::wstring(selected->asset_id.begin(),selected->asset_id.end());
                label(asset,box(contentX+34,detailTop+78,contentWidth-68,22),small_font_.Get(),p.muted);
                label(selected->description,box(contentX+34,detailTop+100,contentWidth-68,22),small_font_.Get(),p.muted);
                const inventory::Item sort_item{selected->name,std::wstring(selected->creator_id.begin(),selected->creator_id.end()),false};
                label(L"Smart Sort suggestion: "+inventory::folder_for(sort_item),box(contentX+34,detailTop+122,contentWidth-68,22),small_font_.Get(),p.accent);
            }
        }else label(L"Select an item to inspect metadata, or select a folder to open it.",box(contentX+34,detailTop+12,contentWidth-68,28),small_font_.Get(),p.muted);
        label(inventory_status_+L" / Smart Sort preview only; no server items moved or deleted.",box(contentX+24,height-35,contentWidth-48,22),small_font_.Get(),p.accent);
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
    const wchar_t* connection_state=
        sl_login_pending_?L"AUTHENTICATING":
        sl_circuit_.movement_seen()?L"SIMULATOR HANDSHAKE":
        sl_login_status_==protocol::LoginStatus::Accepted?
            L"LOGIN ACCEPTED":L"OFFLINE";
    label(connection_state,box(contentX+25,412,cardWidth-50,44),
          title_font_.Get(),p.text);
    std::wstring check;
    switch(sl_tls_probe_.status) {
    case protocol::ProbeStatus::NotChecked:
        check=L"TLS NOT TESTED";break;
    case protocol::ProbeStatus::TlsEndpointReached:
        check=L"TLS REACHED  /  HTTP "+std::to_wstring(sl_tls_probe_.http_status);break;
    case protocol::ProbeStatus::NetworkUnavailable:
        check=L"SL NETWORK UNREACHABLE";break;
    case protocol::ProbeStatus::TlsError:
        check=L"TLS VALIDATION FAILED";break;
    case protocol::ProbeStatus::BadResponse:
        check=L"SL LOGIN HTTP ERROR";break;
    }
    label(check,box(contentX+25,468,cardWidth-50,24),
          small_font_.Get(),p.muted);
    if(page_==0) {
        round_rect(box(contentX+22,493,cardWidth-44,33),11.f,p.accent_soft);
        label(L"CHECK CONNECTION [N]",
              box(contentX+31,500,cardWidth-62,22),
              small_font_.Get(),p.accent);
    }

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
    if(page_==0) {
        // Allocate enough vertical room for the *whole* HTTP diagnostic.
        const float rowH=height>=715.f?87.f:76.f;
        const float y=height>=715.f?558.f:std::max(360.f,height-135.f);
        round_rect(box(contentX,y,contentWidth,rowH),20.f,p.surface);
        round_rect(box(contentX+18,y+19,216.f,42.f),13.f,p.accent_soft);
        label(sl_login_pending_?L"AUTHENTICATING...":
              L"CONNECT TO SECOND LIFE [L]",
              box(contentX+27,y+29,202.f,24.f),
              small_font_.Get(),p.accent);
        if(!sl_login_pending_ &&
           sl_login_status_==protocol::LoginStatus::HttpError){
            // Never clip the category onto the end of a long status string.
            label(L"HTTP "+std::to_wstring(sl_login_http_status_)+
                  L" / KAMI DIAG R5",
                  box(contentX+252,y+3,contentWidth-269,23.f),
                  small_font_.Get(),p.text);
            const auto category=sl_login_response_kind_.empty()?
                std::wstring(L"unavailable (no response details)") :
                sl_login_response_kind_;
            label(L"Response: "+category,
                  box(contentX+252,y+28,contentWidth-269,22.f),
                  small_font_.Get(),p.muted);
            label(L"Login not accepted; DX12 shaders remain ready",
                  box(contentX+252,y+51,contentWidth-269,22.f),
                  small_font_.Get(),p.muted);
        }else if(sl_login_status_==protocol::LoginStatus::Accepted&&
                 sl_circuit_.movement_seen()){
            label(L"SIMULATOR MOVEMENT CONFIRMED",
                  box(contentX+252,y+3,contentWidth-269,20.f),
                  small_font_.Get(),p.text);
            const auto& region=sl_circuit_.region_metadata();
            if(region.verified) {
                std::wstring name(region.name.begin(),region.name.end());
                // Server-derived name, bounded and sanitized by the
                // RegionHandshake decoder. Not offline procedurally generated.
                if(name.size()>32)name.resize(32);
                label(L"Region: "+name+L" (live simulator)",
                      box(contentX+252,y+28,contentWidth-269,21.f),
                      small_font_.Get(),p.text);
            } else {
                label(L"Region metadata: waiting for verified handshake",
                      box(contentX+252,y+28,contentWidth-269,21.f),
                      small_font_.Get(),p.muted);
            }
            label(caps_status_.empty()?
                  L"CAPS negotiation not started":caps_status_,
                  box(contentX+252,y+54,contentWidth-269,24.f),
                  small_font_.Get(),p.muted);
            label(!caps_last_notice_.empty()?caps_last_notice_:
                  (caps_seed_summary_.empty()?
                   L"Seed: waiting for CAPS response":
                   L"Seed: "+caps_seed_summary_),
                  box(contentX+252,y+78,contentWidth-269,23.f),
                  small_font_.Get(),p.muted);
        }else{
            const auto status=sl_login_status_==protocol::LoginStatus::Accepted&&
                sl_circuit_.state()!=protocol::sim::CircuitState::Closed?
                sl_circuit_.diagnostic():sl_login_message_;
            label(status,box(contentX+252,y+8,contentWidth-269,rowH-12.f),
                  small_font_.Get(),p.muted);
        }
    }
    if(sl_login_status_==protocol::LoginStatus::Accepted&&
       sl_circuit_.movement_seen()){
        // Live counts originate from validated LLUDP packets, NOT the
        // DX12 offline renderer. Framing != decoded world geometry.
        label(L"R69 LIVE | Land patches "+std::to_wstring(sl_circuit_.decoded_land_patches().size())+
              L" | meshes drawn "+std::to_wstring(world_renderer_.uploaded_live_meshes())+
              L" | primitives drawn "+std::to_wstring(world_renderer_.uploaded_root_primitive_shapes()+
                  world_renderer_.uploaded_linked_primitive_shapes())+
              L" | PBR faces "+std::to_wstring(world_renderer_.bound_pbr_faces()),
              box(contentX,height-35,contentWidth,23),
              small_font_.Get(),p.muted);
    }else{
        label(L"KAMI DX12 viewer / Connect to Second Life to stream region content.",
              box(contentX,height-35,contentWidth,23),
              small_font_.Get(),p.muted);
    }
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
    seven_systems_open_=false;
    const bool was_world=(page_==1 || page_==2 || page_==6 || page_==7);
    const bool next_world=(index==1 || index==2 || index==6 || index==7);
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
    return (page_ == 1 || page_ == 2 || page_ == 6 || page_ == 7) && world_ready_ && hwnd_ &&
           IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
}

void Window::tick() {
    if(!wants_realtime_render()) return;
    using Clock=std::chrono::steady_clock;
    const auto now=Clock::now();
    if(graphics_.fps_limit!=0 && now<next_frame_) return;
    avatar::Controls controls{};
    const bool focused=GetForegroundWindow()==hwnd_ ||
                       IsChild(hwnd_,GetForegroundWindow())!=FALSE;
    if((page_==1||page_==2)&&focused) {
        const auto down=[](int key) noexcept {
            return (GetAsyncKeyState(key)&0x8000)!=0;
        };
        controls.forward=static_cast<float>(down('W')||down(VK_UP))-
                         static_cast<float>(down('S')||down(VK_DOWN));
        controls.turn=static_cast<float>(down('D')||down(VK_RIGHT))-
                      static_cast<float>(down('A')||down(VK_LEFT));
        controls.strafe=static_cast<float>(down('C'))-
                        static_cast<float>(down('Z'));
        controls.run=down(VK_SHIFT);
        controls.jump=down(VK_SPACE);
    }
    world_renderer_.set_avatar_controls(controls);
    if(sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed){
        const auto wanted=protocol::sim::movement_flags(
            controls.forward>0.f,controls.forward<0.f,
            controls.strafe>0.f,controls.strafe<0.f,
            controls.turn>0.f,controls.turn<0.f,
            controls.run,controls.jump);
        sl_circuit_.set_requested_controls(
            (page_==1||page_==2)&&focused?wanted:0U);
    }
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
    sl_circuit_.set_maximum_world_detail(graphics_.preset==graphics::Preset::Quality);
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
        case 60: graphics_.fps_limit=90; break;
        case 90: graphics_.fps_limit=120; break;
        case 120: graphics_.fps_limit=144; break;
        case 144: graphics_.fps_limit=165; break;
        case 165: graphics_.fps_limit=240; break;
        case 240: graphics_.fps_limit=0; break;
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
    // The cached DX12 live scene uses the *previous* preset's object
    // count until a simulator revision arrives. Rebuild immediately on
    // the next coalesced live-world tick when the user switches presets.
    if(row==0U)live_spatial_uploaded_revision_=~std::uint64_t{0};
    sl_circuit_.set_maximum_world_detail(graphics_.preset==graphics::Preset::Quality);
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
            graphics_.target_fps==120?144:
            graphics_.target_fps==144?165:
            graphics_.target_fps==165?240:30;
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
    sl_circuit_.set_maximum_world_detail(graphics_.preset==graphics::Preset::Quality);
    world_ready_=!world_initialized_ || world_renderer_.ready();
    InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::cycle_body_physics() {
    const auto current=static_cast<unsigned>(world_renderer_.body_physics_profile());
    const auto next=static_cast<avatar::BodyProfile>((current+1U)%4U);
    world_renderer_.set_body_physics_profile(next);
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
    // Transfer the potentially large CPU triangle buffer once; do not
    // duplicate up to 64 MiB just to display source audit counts.
    if(!world_renderer_.set_creator_preview(std::move(imported.scene))) {
        creator_message_=world_renderer_.error().empty()?
            L"KAMI OBJ preview rejected this geometry":
            world_renderer_.error();
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    creator_preview_=std::move(imported);
    creator_file_=path;
    creator_message_=imported.skipped_degenerate_triangles==0?
        L"OBJ loaded into KAMI DX12 preview.":
        L"OBJ preview loaded; skipped "+
            std::to_wstring(imported.skipped_degenerate_triangles)+
            L" zero-area triangles.";
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

void Window::update_messenger_composer() {
    if(!messenger_edit_||!hwnd_)return;
    const bool chat_online=messenger_.active()==messenger::Channel::Nearby&&
        sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed;
    if(page_!=4||(!messenger_.can_send()&&!chat_online)) {
        ShowWindow(messenger_edit_,SW_HIDE);
        return;
    }
    RECT rect{};GetClientRect(hwnd_,&rect);
    const float contentWidth=static_cast<float>(rect.right)-305.f;
    const float panelX=279.f+255.f;
    const float panelW=std::max(280.f,contentWidth-277.f);
    const int x=static_cast<int>(panelX+30.f);
    const int y=static_cast<int>(rect.bottom-144);
    const int width=std::max(145,static_cast<int>(panelW-158.f));
    MoveWindow(messenger_edit_,x,y,width,26,TRUE);
    messenger_edit_sync_=true;
    SetWindowTextW(messenger_edit_,messenger_.draft().c_str());
    messenger_edit_sync_=false;
    ShowWindow(messenger_edit_,SW_SHOW);
}
void Window::messenger_select(messenger::Channel channel) {
    messenger_.select(channel);
    update_messenger_composer();
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::messenger_send() {
    if(!messenger_edit_)return;
    const bool live_nearby=messenger_.active()==messenger::Channel::Nearby&&
       sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed;
    if(!messenger_.can_send()&&!live_nearby)return;
    const int len=GetWindowTextLengthW(messenger_edit_);
    if(len<0||len>512)return;
    std::wstring text(static_cast<std::size_t>(len)+1,L'\0');
    const int count=GetWindowTextW(messenger_edit_,text.data(),len+1);
    text.resize(static_cast<std::size_t>(std::max(count,0)));
    if(!messenger_.set_draft(std::move(text)))return;
    SYSTEMTIME now{};GetLocalTime(&now);
    const unsigned minutes=static_cast<unsigned>(now.wHour)*60U+now.wMinute;
    if(live_nearby){
        if(sl_circuit_.send_nearby_chat(messenger_.draft())){
            (void)messenger_.set_draft(L"");
            messenger_edit_sync_=true;
            SetWindowTextW(messenger_edit_,L"");
            messenger_edit_sync_=false;
        }
        SetFocus(messenger_edit_);
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(messenger_.send_local(minutes)) {
        messenger_edit_sync_=true;
        SetWindowTextW(messenger_edit_,L"");
        messenger_edit_sync_=false;
    }
    SetFocus(messenger_edit_);
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::messenger_demo_reply() {
    SYSTEMTIME now{};GetLocalTime(&now);
    const unsigned minutes=static_cast<unsigned>(now.wHour)*60U+now.wMinute;
    if(messenger_.add_demo_reply(minutes))
        InvalidateRect(hwnd_,nullptr,FALSE);
}

void Window::check_sl_tls_connection() {
    // User initiated: HEAD HTTPS only; never collects or submits credentials.
    sl_tls_probe_=protocol::probe_login_tls(protocol::Grid::Agni);
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::begin_sl_login() {
    if(sl_login_pending_)return;
    // One active region CAPS request at a time. A new login must not
    // overwrite an outstanding async request or reuse bearer URLs.
    if((caps_seed_future_.valid()&&
        caps_seed_future_.wait_for(std::chrono::seconds(0))!=
            std::future_status::ready)||
       (caps_poll_future_.valid()&&
        caps_poll_future_.wait_for(std::chrono::seconds(0))!=
            std::future_status::ready)){
        caps_status_=L"CAPS request in progress; retry after it completes";
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    // No hidden account collection, saved secrets, or identity spoofing.
    // Explicit acceptance and Windows native credential input only.
    const int choice=MessageBoxW(hwnd_,
        L"KAMI experimental Second Life login (not yet a complete viewer).\n\n"
        L"Please review the official Second Life Terms of Service:\n"
        L"https://secondlife.com/corporate/tos\n\n"
        L"YES: I have read and accept the current Terms, and want KAMI\n"
        L"to send credentials to the official SL login server over HTTPS.\n"
        L"NO: Open the official Terms in my browser.\n"
        L"CANCEL: Do not sign in.\n\n"
        L"Windows can remember credentials if Save is checked. MFA is not yet supported.",
        L"KAMI Beyond Reality / Experimental Login",
        MB_YESNOCANCEL|MB_ICONINFORMATION|MB_DEFBUTTON3);
    if(choice==IDNO){
        (void)ShellExecuteW(hwnd_,L"open",
            L"https://secondlife.com/corporate/tos",
            nullptr,nullptr,SW_SHOWNORMAL);
        return;
    }
    if(choice!=IDYES)return;

    CREDUI_INFOW prompt{};
    prompt.cbSize=sizeof(prompt);
    prompt.hwndParent=hwnd_;
    prompt.pszCaptionText=L"KAMI Beyond Reality / Second Life";
    prompt.pszMessageText=
        L"Experimental login for login.agni.lindenlab.com only. "
        L"Check Save to remember login securely in Windows Credential Manager.";
    std::array<wchar_t,CREDUI_MAX_USERNAME_LENGTH+1> account{};
    std::array<wchar_t,CREDUI_MAX_PASSWORD_LENGTH+1> secret{};
    BOOL save=FALSE;
    const DWORD status=CredUIPromptForCredentialsW(
        &prompt,L"login.agni.lindenlab.com",nullptr,0,
        account.data(),static_cast<ULONG>(account.size()),
        secret.data(),static_cast<ULONG>(secret.size()),&save,
        CREDUI_FLAGS_GENERIC_CREDENTIALS|
        CREDUI_FLAGS_ALWAYS_SHOW_UI|CREDUI_FLAGS_SHOW_SAVE_CHECK_BOX|
        CREDUI_FLAGS_EXPECT_CONFIRMATION);
    if(status!=NO_ERROR){
        SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));
        if(status!=ERROR_CANCELLED){
            sl_login_message_=L"Windows credential prompt unavailable";
            InvalidateRect(hwnd_,nullptr,FALSE);
        }
        return;
    }
    protocol::LoginRequest request{};
    request.grid=protocol::Grid::Agni;
    request.account=account.data();
    request.terms_accepted=true;
    std::wstring password=secret.data();
    SecureZeroMemory(secret.data(),secret.size()*sizeof(wchar_t));
    SecureZeroMemory(account.data(),account.size()*sizeof(wchar_t));
    // Only end the existing CAPS session AFTER the user has accepted
    // the Terms and supplied replacement credentials. Cancelling either
    // dialog must never destroy a working Second Life session.
    if(caps_seed_future_.valid()){
        try{
            auto previous=caps_seed_future_.get();
            if(!previous.mesh_capability_url.empty())
                SecureZeroMemory(previous.mesh_capability_url.data(),
                     previous.mesh_capability_url.size()*sizeof(wchar_t));
            if(!previous.event_queue_url.empty())
                SecureZeroMemory(previous.event_queue_url.data(),
                     previous.event_queue_url.size()*sizeof(wchar_t));
        }catch(...){
            // A failed previous CAPS worker cannot crash a new login.
            caps_status_=L"Previous CAPS seed worker failed";
        }
    }
    if(caps_poll_future_.valid()){
        try{(void)caps_poll_future_.get();}catch(...){}
    }
    if(!caps_seed_url_.empty())
        SecureZeroMemory(caps_seed_url_.data(),
                          caps_seed_url_.size()*sizeof(wchar_t));
    if(!caps_event_queue_url_.empty())
        SecureZeroMemory(caps_event_queue_url_.data(),
                         caps_event_queue_url_.size()*sizeof(wchar_t));
    if(!caps_ack_.token.empty())
        SecureZeroMemory(caps_ack_.token.data(),caps_ack_.token.size());
    caps_seed_url_.clear();
    caps_event_queue_url_.clear();
    caps_ack_={};
    caps_completed_polls_=0;
    caps_events_observed_=caps_events_unsupported_=0;
    caps_notifications_handled_=0;
    caps_last_notice_.clear();
    caps_consecutive_failures_=0;
    caps_dispatch_blocked_=false;
    caps_next_poll_={};
    caps_seed_retry_after_={};
    caps_seed_attempts_=0U;
    caps_status_.clear();
    caps_seed_summary_.clear();
    caps_recent_types_.clear();
    caps_seed_started_=caps_poll_started_=false;
    sl_circuit_.stop();
    world_renderer_.clear_decoded_region_geometry();
    world_renderer_.clear_live_water_preview();
    live_land_uploaded_revision_=0;
    live_spatial_uploaded_revision_=0;
    live_land_last_upload_={};
    live_water_attempted_=false;
    live_water_status_.clear();
    sl_login_status_=protocol::LoginStatus::NotAttempted;
    sl_login_http_status_=0;
    sl_login_response_kind_.clear();
    sl_login_message_=L"Authenticating with Second Life over HTTPS...";
    sl_login_pending_=true;
    try{
        sl_login_future_=std::async(std::launch::async,
        [req=std::move(request),password=std::move(password)]() mutable {
            const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            protocol::LoginResult answer{};
            if(SUCCEEDED(init)){
                answer=protocol::login_to_simulator(req,password);
                CoUninitialize();
            }else{
                answer.status=protocol::LoginStatus::MalformedReply;
            }
            if(!password.empty())
                SecureZeroMemory(password.data(),
                                 password.size()*sizeof(wchar_t));
            return answer;
        });
    }catch(...){
        // Creating a worker can fail when Windows is out of threads or
        // address space. Never leave the account stuck in LOGIN PENDING.
        sl_login_pending_=false;
        sl_login_status_=protocol::LoginStatus::NetworkError;
        sl_login_message_=L"Windows could not start the SL login worker";
    }
    (void)SetTimer(hwnd_,203,100,nullptr);
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::start_caps_poll() {
    if(!caps_poll_started_||caps_dispatch_blocked_||
       caps_event_queue_url_.empty()||caps_poll_future_.valid()||
       sl_login_status_!=protocol::LoginStatus::Accepted||
       sl_circuit_.state()!=protocol::sim::CircuitState::MovementConfirmed||
       std::chrono::steady_clock::now()<caps_next_poll_)
        return;
    auto url=caps_event_queue_url_; // keep trusted URL only for this session
    auto ack=caps_ack_;              // preserve server-provided LLSD type
    caps_status_=L"R9 CAPS polling: "+
        std::to_wstring(caps_events_observed_)+
        L" observed / "+std::to_wstring(caps_events_unsupported_)+
        L" unsupported / "+std::to_wstring(caps_notifications_handled_)+
        L" notices delivered";
    if(!caps_recent_types_.empty())
        caps_status_+=L" ("+caps_recent_types_+L")";
    try{
        caps_poll_future_=std::async(std::launch::async,
        [url=std::move(url),ack=std::move(ack)]() mutable {
            const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            protocol::caps::PollResponse response{};
            if(SUCCEEDED(init)){
                response=protocol::caps::probe_event_queue_once(url,ack);
                CoUninitialize();
            }else response.diagnostic=L"EventQueue COM init failed";
            if(!url.empty())
                SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
            if(!ack.token.empty())
                SecureZeroMemory(ack.token.data(),ack.token.size());
            return response;
        });
    }catch(...){
        // No future was created. Preserve the authoritative EventQueue URL
        // and ACK cursor, then retry at a restrained interval.
        caps_next_poll_=std::chrono::steady_clock::now()+
            std::chrono::seconds(2);
        caps_status_=L"CAPS: Windows worker unavailable; retrying poll";
    }
}
void Window::start_inventory_refresh(){
    if(sl_circuit_.state()!=protocol::sim::CircuitState::MovementConfirmed||
       !sl_circuit_.inventory_capabilities_ready()){
        inventory_status_=L"Log in and complete region handshake before refreshing";
        InvalidateRect(hwnd_,nullptr,FALSE);return;
    }
    if(inventory_root_future_.valid()){
        if(inventory_root_future_.wait_for(std::chrono::seconds(0))!=std::future_status::ready){
            inventory_status_=L"Inventory request already running";
            InvalidateRect(hwnd_,nullptr,FALSE);return;
        }
        try{(void)inventory_root_future_.get();}catch(...){}
    }
    const auto root=sl_circuit_.inventory_root_id();
    const auto owner=sl_circuit_.inventory_owner_id();
    auto url=sl_circuit_.inventory_descendents_capability();
    if(!protocol::valid_uuid(root)||!protocol::valid_uuid(owner)||url.empty()){
        inventory_status_=L"Inventory root or capability unavailable";
        InvalidateRect(hwnd_,nullptr,FALSE);return;
    }
    inventory_current_folder_=root;
    inventory_selected_entry_.clear();
    inventory_search_.clear();
    inventory_status_=L"Refreshing full inventory tree...";
    inventory_root_future_=std::async(std::launch::async,
        [url=std::move(url),root,owner]() mutable {
            const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            protocol::caps::InventoryTreeFetchResult result{};
            if(SUCCEEDED(init)){
                result=protocol::caps::fetch_inventory_tree(url,root,owner);
                CoUninitialize();
                if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
            }else{
                result.transport_error=static_cast<unsigned>(init);
                if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
            }
            return result;
        });
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::start_inventory_item_details(std::string item_id){
    if(!protocol::valid_uuid(item_id)||!sl_circuit_.inventory_capabilities_ready())return;
    if(inventory_item_future_.valid()){
        if(inventory_item_future_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try{(void)inventory_item_future_.get();}catch(...){}
    }
    const auto agent=sl_circuit_.inventory_owner_id();
    const auto owner=sl_circuit_.inventory_owner_id();
    auto url=sl_circuit_.inventory_items_capability();
    if(!protocol::valid_uuid(agent)||!protocol::valid_uuid(owner)||url.empty())return;
    inventory_selected_details_valid_=false;
    inventory_item_future_=std::async(std::launch::async,
        [url=std::move(url),agent,owner,item_id=std::move(item_id)]() mutable {
            const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
            protocol::caps::InventoryItemsFetchResult result{};
            if(SUCCEEDED(init)){
                result=protocol::caps::fetch_inventory_items(url,agent,owner,{item_id});
                CoUninitialize();
                if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
            }else{
                result.transport_error=static_cast<unsigned>(init);
                if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
            }
            return result;
        });
}
void Window::update_sl_connection() {
    if(sl_login_pending_&&sl_login_future_.valid()&&
       sl_login_future_.wait_for(std::chrono::seconds(0))==
           std::future_status::ready) {
        protocol::LoginResult result{};
        try{
            result=sl_login_future_.get();
        } catch(...) {
            result.status=protocol::LoginStatus::NetworkError;
        }
        sl_login_pending_=false;
        // Confirm persistence only after the SL server accepts the login.
        (void)CredUIConfirmCredentialsW(L"login.agni.lindenlab.com",
            result.accepted()?TRUE:FALSE);
        sl_login_status_=result.status;
        sl_login_http_status_=result.http_status;
        sl_login_response_kind_=result.http_response_kind;
        sl_login_message_=result.message.empty()?
            protocol::login_status_label(result.status):result.message;
        if(result.accepted()) {
            if(sl_circuit_.start(result)){
                // Seed URL was authenticated as part of the login handoff.
                // Do not request CAPS until the simulator movement message.
                caps_seed_url_=result.handoff.seed_capability;
                caps_status_=L"CAPS seed queued; waiting for simulator";
                sl_login_message_=L"Login accepted; contacting simulator...";
            }else{
                caps_status_=L"Simulator UDP failed; CAPS not started";
                sl_login_message_=L"SL accepted login, but simulator UDP failed";
            }
        }
        if(!result.handoff.seed_capability.empty())
            SecureZeroMemory(result.handoff.seed_capability.data(),
                result.handoff.seed_capability.size()*sizeof(wchar_t));
        result.handoff={};
    }
    if(sl_circuit_.state()!=protocol::sim::CircuitState::Closed &&
       sl_circuit_.state()!=protocol::sim::CircuitState::Failed)
        sl_circuit_.poll();
    // A login may complete while MESSAGES is already open. Refresh the
    // nearby-chat composer only on connectivity transitions, not on every
    // timer tick (which would erase text the resident is typing).
    const bool nearby_online = sl_circuit_.state()==
        protocol::sim::CircuitState::MovementConfirmed;
    if(nearby_online != messenger_nearby_online_) {
        messenger_nearby_online_ = nearby_online;
        if(page_ == 4) {
            update_messenger_composer();
            InvalidateRect(hwnd_, nullptr, FALSE);
        }
    }
    // R9 first server-sourced DX12 pixels: only a partial planar
    // water-height preview, never fabricated terrain or avatar meshes.
    if(!live_water_attempted_&&
       sl_login_status_==protocol::LoginStatus::Accepted&&
       sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed&&
       sl_circuit_.region_metadata().verified&&
       sl_circuit_.movement_position().verified){
        live_water_attempted_=true;
        if(world_renderer_.set_verified_live_water(sl_circuit_))
            live_water_status_=L"R9 verified water-only surface active";
        else
            live_water_status_=L"R9 water DX12 upload unavailable";
    }
    const auto land_revision=sl_circuit_.land_revision();
    // Local avatar full/terse pose updates are not world-geometry edits.
    // Rebase from an authenticated movement position after 12m instead.
    const auto spatial_revision=sl_circuit_.spatial_render_revision();
    const auto& scene_agent=sl_circuit_.movement_position();
    const bool scene_rebase=scene_agent.verified&&
        world::world_scene_rebase_due(
            {scene_agent.x,scene_agent.y,scene_agent.z},
            live_scene_uploaded_origin_,live_scene_origin_valid_);
    const auto now=std::chrono::steady_clock::now();
    // Coalesce bursts of verified live-world updates. Objects may arrive
    // before the first land layer; do not make terrain a prerequisite for
    // uploading already-decoded spatial/mesh content to the DX12 scene.
    const bool has_live_world_data=land_revision>0U||spatial_revision>0U||
        !sl_circuit_.decoded_mesh_assets().empty()||
        sl_circuit_.decoded_texture_count()>0U||
        !sl_circuit_.decoded_materials().empty();
    if(sl_login_status_==protocol::LoginStatus::Accepted&&
       sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed&&
       world_renderer_.ready()&&has_live_world_data&&
       (land_revision!=live_land_uploaded_revision_||
        spatial_revision!=live_spatial_uploaded_revision_||
        sl_circuit_.decoded_texture_count()!=live_uploaded_texture_count_||
        sl_circuit_.decoded_mesh_assets().size()!=live_uploaded_mesh_count_||
        sl_circuit_.decoded_materials().size()!=live_uploaded_material_count_||
        scene_rebase||world_renderer_.texture_upload_pending())&&
       (live_land_last_upload_==std::chrono::steady_clock::time_point{}||
        now-live_land_last_upload_>=std::chrono::milliseconds(800))){
        live_land_last_upload_=now;
        if(world_renderer_.set_verified_live_terrain(sl_circuit_)){
            live_land_uploaded_revision_=land_revision;
            live_spatial_uploaded_revision_=spatial_revision;
            live_uploaded_texture_count_=sl_circuit_.decoded_texture_count();
            live_uploaded_mesh_count_=sl_circuit_.decoded_mesh_assets().size();
            live_uploaded_material_count_=sl_circuit_.decoded_materials().size();
            live_scene_uploaded_origin_={
                scene_agent.x,scene_agent.y,scene_agent.z};
            live_scene_origin_valid_=scene_agent.verified;
            live_water_status_=world_renderer_.texture_upload_pending()?
                L"R52 DX12 atlas update deferred until GPU copy completes":
                L"R52 DX12 coalesced land and mesh world";
        }else{
            live_water_status_=L"R12 verified live-world geometry could not be uploaded";
        }
    }
    if(sl_circuit_.state()==protocol::sim::CircuitState::Closed||
       sl_circuit_.state()==protocol::sim::CircuitState::Failed){
        if(world_renderer_.region_geometry_active())
            world_renderer_.clear_decoded_region_geometry();
        if(world_renderer_.live_water_preview_active())
            world_renderer_.clear_live_water_preview();
        live_land_uploaded_revision_=0;
        live_spatial_uploaded_revision_=0;
        live_uploaded_texture_count_=0;
        live_uploaded_mesh_count_=0;
        live_uploaded_material_count_=0;
        live_land_last_upload_={};
        live_scene_uploaded_origin_={};
        live_scene_origin_valid_=false;
        live_water_attempted_=false;
        live_water_status_.clear();
    }
    if(sl_login_status_==protocol::LoginStatus::Accepted&&
       // movement_seen alone can precede RegionHandshake. Starting the
       // one-shot seed fetch then could discard a successful grant before
       // Circuit reaches MovementConfirmed, leaving CAP NO indefinitely.
       sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed&&
       !caps_seed_started_&&!caps_seed_url_.empty()&&
       std::chrono::steady_clock::now()>=caps_seed_retry_after_){
        caps_status_=L"CAPS: requesting region seed over HTTPS...";
        // Preserve the trusted seed URL until worker launch succeeds.
        // Thread creation may fail; dropping the only seed URL then
        // permanently leaves the session on CAP NO.
        auto url=caps_seed_url_;
        try{
            caps_seed_future_=std::async(std::launch::async,
                [url=std::move(url)]()mutable {
                    const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
                    protocol::caps::SeedResponse response{};
                    if(SUCCEEDED(init)){
                        response=protocol::caps::request_seed(url);
                        CoUninitialize();
                    }else response.diagnostic=L"CAPS COM initialization failed";
                    if(!url.empty())
                        SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                    return response;
                });
            caps_seed_started_=true;
            ++caps_seed_attempts_;
            // Retain the trusted URL for a bounded retry if the HTTPS
            // worker returns a transient failure. Clear it on success or
            // permanent rejection, not immediately after thread launch.
        }catch(...){
            caps_seed_started_=false;
            caps_seed_retry_after_=std::chrono::steady_clock::now()+
                std::chrono::seconds(3);
            caps_status_=L"CAPS: Windows worker unavailable; retrying seed";
        }
    }
    if(caps_seed_started_&&caps_seed_future_.valid()&&
       caps_seed_future_.wait_for(std::chrono::seconds(0))==
           std::future_status::ready){
        protocol::caps::SeedResponse result{};
        try{result=caps_seed_future_.get();}
        catch(...){result.diagnostic=L"CAPS seed worker failed";}
        caps_status_=result.diagnostic;
        caps_seed_summary_=result.diagnostic;
        if(result.granted&&
           sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed){
            // CAPS services are independent. Mesh assets may be granted
            // without EventQueueGet. Never fabricate a queue URL.
            if(result.mesh_granted)
                (void)sl_circuit_.set_mesh_capability(
                    std::move(result.mesh_capability_url));
            if(result.texture_granted)
                (void)sl_circuit_.set_texture_capability(
                    std::move(result.texture_capability_url));
            if(result.inventory_descendents_granted&&
               result.inventory_items_granted&&
               sl_circuit_.set_inventory_capabilities(
                    std::move(result.inventory_descendents_url),
                    std::move(result.inventory_items_url))){
                const auto root=sl_circuit_.inventory_root_id();
                const auto owner=sl_circuit_.inventory_owner_id();
                auto url=sl_circuit_.inventory_descendents_capability();
                if(protocol::valid_uuid(root)&&protocol::valid_uuid(owner)&&
                   !inventory_root_future_.valid()){
                    inventory_status_=L"Fetching inventory root...";
                    inventory_root_future_=std::async(std::launch::async,
                    [url=std::move(url),root,owner]() mutable {
                        const HRESULT init=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
                        protocol::caps::InventoryTreeFetchResult fetched{};
                        if(SUCCEEDED(init)){
                            fetched=protocol::caps::fetch_inventory_tree(
                                url,root,owner);
                            CoUninitialize();
                            if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                        }else{
                            fetched.transport_error=static_cast<unsigned>(init);
                            if(!url.empty())
                                SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                        }
                        return fetched;
                    });
                }else{
                    inventory_status_=L"Inventory root unavailable in login reply";
                    if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                }
            }
            if(result.event_queue_granted){
                caps_poll_started_=true;
                caps_event_queue_url_=std::move(result.event_queue_url);
                caps_status_=result.mesh_granted?
                    L"CAPS seed OK: EventQueue + mesh granted":
                    L"CAPS seed OK: EventQueue granted; mesh unavailable";
                start_caps_poll();
            }
        }
        if(!result.mesh_capability_url.empty())
            SecureZeroMemory(result.mesh_capability_url.data(),
                 result.mesh_capability_url.size()*sizeof(wchar_t));
        if(!result.texture_capability_url.empty())
            SecureZeroMemory(result.texture_capability_url.data(),
                result.texture_capability_url.size()*sizeof(wchar_t));
        if(!result.event_queue_url.empty())
            SecureZeroMemory(result.event_queue_url.data(),
                result.event_queue_url.size()*sizeof(wchar_t));
        if(!result.granted&&
           sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed&&
           protocol::caps::retry_seed_response(
               result.http_status,caps_seed_attempts_)&&
           !caps_seed_url_.empty()){
            caps_seed_started_=false;
            caps_seed_retry_after_=std::chrono::steady_clock::now()+
                std::chrono::seconds(1U<<caps_seed_attempts_);
            caps_status_+=L"; retrying seed";
        }else{
            if(!caps_seed_url_.empty())
                SecureZeroMemory(caps_seed_url_.data(),
                    caps_seed_url_.size()*sizeof(wchar_t));
            caps_seed_url_.clear();
        }
    }
    if(inventory_root_future_.valid()&&
       inventory_root_future_.wait_for(std::chrono::seconds(0))==
           std::future_status::ready){
        try{
            auto fetched=inventory_root_future_.get();
            if(fetched.fetched){
                inventory_root_snapshot_.valid=true;
                inventory_root_snapshot_.entries=std::move(fetched.entries);
                inventory_status_=L"Loaded "+
                    std::to_wstring(inventory_root_snapshot_.entries.size())+
                    L" inventory entries across "+
                    std::to_wstring(fetched.folders_fetched)+L" folders";
            }else{
                inventory_status_=fetched.limit_reached?
                    L"Inventory safety limit reached; partial data not marked complete":
                    L"Inventory fetch failed / HTTP "+
                    std::to_wstring(fetched.http_status);
                inventory_root_snapshot_.entries=std::move(fetched.entries);
            }
        }catch(...){
            inventory_status_=L"Inventory response worker failed";
        }
    }
    if(inventory_item_future_.valid()&&
       inventory_item_future_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{
            auto fetched=inventory_item_future_.get();
            if(fetched.fetched&&fetched.items.size()==1&&
               fetched.items.front().id==inventory_selected_entry_){
                inventory_selected_details_=std::move(fetched.items.front());
                inventory_selected_details_valid_=true;
            }
        }catch(...){inventory_selected_details_valid_=false;}
        if(page_==3)InvalidateRect(hwnd_,nullptr,FALSE);
    }
    if(caps_poll_started_&&caps_poll_future_.valid()&&
       caps_poll_future_.wait_for(std::chrono::seconds(0))==
          std::future_status::ready){
        try{
            auto result=caps_poll_future_.get();
            if(result.structurally_valid&&result.safe_to_ack){
                // Commit bounded event diagnostics on the main thread,
                // and only THEN advance the server's typed ACK cursor.
                // R9 fixes the R8/R9 infinite "no ACK" standstill.
                ++caps_completed_polls_;
                caps_consecutive_failures_=0;
                caps_events_observed_+=result.event_count;
                caps_events_unsupported_+=result.unsupported_events;
                caps_notifications_handled_+=result.handled_events;
                // Only the bounded TeleportFailed notice has a defined
                // UI handler. Never show arbitrary CAPS event bodies.
                if(!result.notifications.empty()){
                    const auto& safe=result.notifications.back();
                    caps_last_notice_.assign(safe.begin(),safe.end());
                }
                if(!caps_ack_.token.empty())
                    SecureZeroMemory(caps_ack_.token.data(),
                                     caps_ack_.token.size());
                caps_ack_=std::move(result.next_ack);
                if(result.event_count>0){
                    caps_status_=L"R9 CAPS LIVE: "+
                        std::to_wstring(caps_events_observed_)+
                        L" observed; "+
                        std::to_wstring(caps_events_unsupported_)+
                        L" unsupported; "+std::to_wstring(
                            caps_notifications_handled_)+L" notices";
                    caps_recent_types_.clear();
                    if(!result.event_types.empty()){
                        for(std::size_t i=0;i<result.event_types.size()&&i<2;++i){
                            if(i)caps_recent_types_+=L", ";
                            const auto& type=result.event_types[i];
                            caps_recent_types_+=
                                std::wstring(type.begin(),type.end());
                        }
                        caps_status_+=L" ("+caps_recent_types_+L")";
                    }
                }else{
                    caps_status_=L"R9 CAPS LIVE: "+
                        std::to_wstring(caps_completed_polls_)+
                        L" polls; "+
                        std::to_wstring(caps_events_observed_)+
                        L" events observed (not applied)";
                }
                caps_next_poll_=std::chrono::steady_clock::now()+
                                std::chrono::milliseconds(500);
            }else if(result.idle){
                caps_consecutive_failures_=0;
                // Preserve the warning for an unparseable idle response.
                caps_status_=result.diagnostic;
                caps_next_poll_=std::chrono::steady_clock::now()+
                                std::chrono::milliseconds(500);
            }else if(result.transient){
                // Retry recoverable endpoint/long-poll failures without
                // spinning on every UI tick or stalling permanently.
                caps_consecutive_failures_=
                    std::min(caps_consecutive_failures_+1U,6U);
                const unsigned backoff=std::min(
                    1U<<caps_consecutive_failures_,30U);
                caps_next_poll_=std::chrono::steady_clock::now()+
                    std::chrono::seconds(backoff);
                caps_status_=result.diagnostic+L"; retrying in "+
                    std::to_wstring(backoff)+L"s";
            }else{
                // Invalid LLSD, expired CAPS or non-recoverable failures
                // must be visible rather than blindly advancing an ACK.
                caps_dispatch_blocked_=true;
                caps_status_=result.diagnostic;
            }
        }catch(...){
            caps_dispatch_blocked_=true;
            caps_status_=L"EventQueue worker failed";
        }
    }
    // Exactly one outstanding request at a time, same simulator session.
    // Never acknowledge events until their handlers commit state.
    start_caps_poll();
    if(!sl_login_pending_&&
       (sl_circuit_.state()==protocol::sim::CircuitState::Closed||
        sl_circuit_.state()==protocol::sim::CircuitState::Failed))
        (void)KillTimer(hwnd_,203);
    if(page_==0)InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::click(float x, float y) {
    RECT client{};
    GetClientRect(hwnd_, &client);
    const float width = static_cast<float>(client.right);
    if(x>=30.f&&x<=245.f &&
       y>=static_cast<float>(client.bottom)-98.f &&
       y<static_cast<float>(client.bottom)-62.f){
        const bool open=!seven_systems_open_;
        select_page(0);
        seven_systems_open_=open;
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(seven_systems_open_)return;
    if (x >= width-195.f && x <= width-32.f && y >= 23.f && y < 67.f) {
        toggle_appearance();
        return;
    }
    if(page_==0 && x>=301.f &&
       x<301.f+(width-305.f-18.f)/2.f-44.f &&
       y>=493.f && y<526.f) {
        check_sl_tls_connection();
        return;
    }
    if(page_==0 && x>=297.f && x<=530.f &&
       y>=(client.bottom>=715?558.f:536.f)+17.f &&
       y<(client.bottom>=715?558.f:536.f)+64.f) {
        begin_sl_login();
        return;
    }
    if(page_==2 && y>=static_cast<float>(client.bottom)-186.f &&
       y<static_cast<float>(client.bottom)-152.f &&
       x>=width-336.f && x<=width-45.f) {
        cycle_body_physics();
        return;
    }
    if(page_==2 && y>=static_cast<float>(client.bottom)-144.f &&
       y<static_cast<float>(client.bottom)-106.f &&
       x>=width-334.f && x<=width-47.f) {
        world_renderer_.reset_avatar();
        InvalidateRect(hwnd_,nullptr,FALSE);
        return;
    }
    if(page_==3){
        const float contentX=534.f,contentWidth=width-contentX-30.f;
        if(y>=174.f&&y<216.f){
            if(x>=contentX+contentWidth-270.f&&x<contentX+contentWidth-154.f){start_inventory_refresh();return;}
            if(x>=contentX+contentWidth-154.f&&x<contentX+contentWidth-20.f){inventory_sort_mode_=(inventory_sort_mode_+1U)%3U;InvalidateRect(hwnd_,nullptr,FALSE);return;}
            inventory_search_focused_=true;SetFocus(hwnd_);return;
        }
        if(y>=222.f&&y<248.f){
            const auto root=sl_circuit_.inventory_root_id();
            if(inventory_current_folder_!=root){
                for(const auto& entry:inventory_root_snapshot_.entries)
                    if(entry.folder&&entry.id==inventory_current_folder_){inventory_current_folder_=entry.parent_id;break;}
            }
            inventory_selected_entry_.clear();inventory_search_.clear();
            InvalidateRect(hwnd_,nullptr,FALSE);return;
        }
        const auto visible=inventory::filter_entries(inventory_root_snapshot_.entries,inventory_current_folder_,inventory_search_,static_cast<inventory::SortMode>(inventory_sort_mode_%3));
        const float rowTop=251.f,rowHeight=31.f;
        const float detailTop=std::max(rowTop+80.f,static_cast<float>(client.bottom)-188.f);
        if(y>=rowTop&&y<detailTop){
            const auto row=static_cast<std::size_t>((y-rowTop)/rowHeight);
            if(row<visible.size()){
                const auto& e=inventory_root_snapshot_.entries[visible[row]];
                inventory_selected_entry_=e.id;
                inventory_selected_details_valid_=false;
                if(e.folder){inventory_current_folder_=e.id;inventory_search_.clear();}
                else start_inventory_item_details(e.id);
                InvalidateRect(hwnd_,nullptr,FALSE);return;
            }
        }
        return;
    }
    if(page_==4) {
        const float contentWidth=width-305.f;
        const float panelX=279.f+255.f;
        const float panelW=std::max(280.f,contentWidth-277.f);
        if(x>=310.f && x<=504.f) {
            for(unsigned i=0;i<3;++i) {
                const float top=211.f+76.f*static_cast<float>(i);
                if(y>=top && y<top+67.f) {
                    messenger_select(static_cast<messenger::Channel>(i));
                    return;
                }
            }
            const float replyTop=std::min(474.f,
                static_cast<float>(client.bottom)-196.f);
            if(messenger_.can_send()&&y>=replyTop&&y<replyTop+38.f){
                messenger_demo_reply();return;
            }
        }
        if((messenger_.can_send()||
            (messenger_.active()==messenger::Channel::Nearby &&
             sl_circuit_.state()==protocol::sim::CircuitState::MovementConfirmed)) &&
           x>=panelX+panelW-115.f && x<panelX+panelW-12.f &&
           y>=static_cast<float>(client.bottom)-158.f &&
           y<static_cast<float>(client.bottom)-112.f) {
            messenger_send();return;
        }
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

