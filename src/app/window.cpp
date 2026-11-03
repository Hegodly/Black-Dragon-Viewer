#include "kami/window.hpp"
#include "kami/appearance.hpp"
#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <array>
#include <string_view>

namespace kami::app {
namespace {
constexpr wchar_t kClassName[]=L"KAMI_OriginalCore_Window";
constexpr std::array<std::wstring_view,6> kPages = {
    L"HOME",L"WORLD",L"AVATAR",L"INVENTORY",L"MESSAGES",L"SETTINGS"
};
constexpr std::array<std::wstring_view,6> kDescriptions = {
    L"An original native Windows application created for Kami.",
    L"World connection and simulator protocol are not implemented yet.",
    L"Avatar appearance and animation are not implemented yet.",
    L"Inventory synchronization is not implemented yet.",
    L"Messaging and delivery receipts are not implemented yet.",
    L"Appearance settings are live and saved to your Windows profile."
};
constexpr std::array<std::wstring_view,6> kSubtitles = {
    L"Original shell  /  Direct2D interface  /  New independent architecture",
    L"Real world support requires a new transport, asset pipeline and renderer.",
    L"Avatar controls will only appear when they are connected to real data.",
    L"No simulated items or alterations to your Second Life inventory.",
    L"Nothing is sent to Second Life by this initial application.",
    L"F1–F6 changes pages. Press T to switch between light and dark."
};
D2D1_RECT_F rect(float x,float y,float w,float h) {
    return D2D1::RectF(x,y,x+w,y+h);
}
}

bool Window::create(HINSTANCE instance,int show) {
    light_=kami::settings::light_appearance();
    if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
              factory_.ReleaseAndGetAddressOf()))) return false;
    if(FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
              __uuidof(IDWriteFactory),
              reinterpret_cast<IUnknown**>(writing_.ReleaseAndGetAddressOf())))) return false;
    auto font=[this](float size,DWRITE_FONT_WEIGHT weight,
                     Microsoft::WRL::ComPtr<IDWriteTextFormat>& format) {
        return writing_->CreateTextFormat(L"Segoe UI",nullptr,weight,
              DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,
              size,L"en-us",format.ReleaseAndGetAddressOf());
    };
    if(FAILED(font(42.f,DWRITE_FONT_WEIGHT_BOLD,hero_)) ||
       FAILED(font(23.f,DWRITE_FONT_WEIGHT_SEMI_BOLD,title_)) ||
       FAILED(font(17.f,DWRITE_FONT_WEIGHT_NORMAL,body_)) ||
       FAILED(font(13.f,DWRITE_FONT_WEIGHT_NORMAL,caption_))) return false;

    WNDCLASSEXW klass{};
    klass.cbSize=sizeof(klass);
    klass.hInstance=instance;
    klass.lpfnWndProc=&Window::dispatch;
    klass.lpszClassName=kClassName;
    klass.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    klass.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    klass.style=CS_HREDRAW | CS_VREDRAW;
    if(!RegisterClassExW(&klass) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;

    hwnd_=CreateWindowExW(0,kClassName,L"KAMI Beyond Reality - Original Core",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1150,760,
        nullptr,nullptr,instance,this);
    if(!hwnd_) return false;
    const BOOL dark=light_?FALSE:TRUE;
    DwmSetWindowAttribute(hwnd_,20,&dark,sizeof(dark));
    ShowWindow(hwnd_,show);
    UpdateWindow(hwnd_);
    return true;
}

LRESULT CALLBACK Window::dispatch(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    Window* self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(message==WM_NCCREATE) {
        const auto* creation=reinterpret_cast<CREATESTRUCTW*>(lp);
        self=static_cast<Window*>(creation->lpCreateParams);
        self->hwnd_=hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
    }
    return self?self->handle(message,wp,lp):DefWindowProcW(hwnd,message,wp,lp);
}
LRESULT Window::handle(UINT message,WPARAM wp,LPARAM lp) {
    switch(message) {
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd_,&ps);
        paint();
        EndPaint(hwnd_,&ps);
        return 0;
    }
    case WM_SIZE:
        resize(static_cast<UINT>(LOWORD(lp)),static_cast<UINT>(HIWORD(lp)));
        return 0;
    case WM_LBUTTONDOWN:
        SetFocus(hwnd_);
        on_click(static_cast<float>(GET_X_LPARAM(lp)),static_cast<float>(GET_Y_LPARAM(lp)));
        return 0;
    case WM_KEYDOWN:
        if(wp>=VK_F1 && wp<=VK_F6) {
            select_page(static_cast<unsigned>(wp-VK_F1));
            return 0;
        }
        if(wp==static_cast<WPARAM>('T')) { toggle_theme(); return 0; }
        break;
    case WM_GETMINMAXINFO: {
        auto* constraints=reinterpret_cast<MINMAXINFO*>(lp);
        constraints->ptMinTrackSize.x=900;
        constraints->ptMinTrackSize.y=620;
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd_,message,wp,lp);
}

bool Window::create_resources() {
    if(surface_) return true;
    RECT client{};
    GetClientRect(hwnd_,&client);
    return SUCCEEDED(factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_,D2D1::SizeU(
            static_cast<UINT>(std::max<LONG>(1,client.right)),
            static_cast<UINT>(std::max<LONG>(1,client.bottom)))),
        surface_.ReleaseAndGetAddressOf()));
}
void Window::resize(UINT width,UINT height) {
    if(surface_ && width && height) surface_->Resize(D2D1::SizeU(width,height));
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::rounded(D2D1_RECT_F area,float radius,D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if(SUCCEEDED(surface_->CreateSolidColorBrush(color,brush.GetAddressOf())))
        surface_->FillRoundedRectangle(D2D1::RoundedRect(area,radius,radius),brush.Get());
}
void Window::label(std::wstring_view words,D2D1_RECT_F area,
                   IDWriteTextFormat* font,D2D1_COLOR_F color) {
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush;
    if(SUCCEEDED(surface_->CreateSolidColorBrush(color,brush.GetAddressOf())))
        surface_->DrawTextW(words.data(),static_cast<UINT32>(words.size()),
                            font,area,brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Window::draw(float width,float height,const kami::ui::Palette& color) {
    // Entire UI is KAMI-owned geometry, native text and KAMI-owned callbacks.
    rounded(rect(17,17,236,height-34),29.f,color.surface);
    label(L"K A M I",rect(42,50,180,45),title_.Get(),color.text);
    label(L"BEYOND REALITY",rect(43,104,175,24),caption_.Get(),color.accent);
    for(unsigned i=0;i<static_cast<unsigned>(kPages.size());++i) {
        float y=156.f+56.f*static_cast<float>(i);
        if(page_==i) rounded(rect(29,y,212,43),15.f,color.accent_soft);
        label(kPages[i],rect(49,y+10.f,175,28),body_.Get(),
              page_==i?color.accent:color.muted);
    }
    label(L"KAMI  /  v0.1 original",rect(41,height-66,185,23),
          caption_.Get(),color.muted);

    const float x=278.f;
    const float w=std::max(440.f,width-308.f);
    label(L"KAMI  /  ORIGINAL WINDOWS CORE",rect(x,27,w-190,27),
          caption_.Get(),color.muted);
    rounded(rect(width-193.f,24.f,160.f,45.f),21.f,color.surface);
    label(light_?L"LIGHT APPEARANCE":L"DARK APPEARANCE",
          rect(width-181.f,36.f,140.f,22.f),caption_.Get(),color.accent);

    rounded(rect(x,92,w,243),29.f,color.elevated);
    label(kPages[page_],rect(x+32,120,w-60,60),hero_.Get(),color.text);
    label(kDescriptions[page_],rect(x+33,194,w-66,69),body_.Get(),color.muted);
    label(kSubtitles[page_],rect(x+33,292,w-64,28),caption_.Get(),color.accent);

    const float half=(w-18.f)/2.f;
    rounded(rect(x,354,half,180),22.f,color.surface);
    rounded(rect(x+half+18.f,354,half,180),22.f,color.surface);
    label(L"SECOND LIFE SESSION",rect(x+22,377,half-32,29),
          caption_.Get(),color.muted);
    label(L"OFFLINE",rect(x+22,422,half-32,40),title_.Get(),color.text);
    label(L"Not yet implemented",rect(x+22,491,half-32,25),
          caption_.Get(),color.muted);
    label(L"RENDERING",rect(x+half+40,377,half-50,29),
          caption_.Get(),color.muted);
    label(L"NATIVE UI",rect(x+half+40,422,half-50,40),
          title_.Get(),color.text);
    label(L"3D world renderer pending",rect(x+half+40,491,half-50,28),
          caption_.Get(),color.muted);

    label(L"Independent KAMI code  •  Windows-only  •  F1–F6 pages  •  T appearance",
          rect(x,height-67,w,29),caption_.Get(),color.muted);
}
void Window::paint() {
    if(!create_resources()) return;
    const auto dimensions=surface_->GetSize();
    const auto palette=kami::ui::colors(light_);
    surface_->BeginDraw();
    surface_->Clear(palette.background);
    draw(dimensions.width,dimensions.height,palette);
    if(surface_->EndDraw()==D2DERR_RECREATE_TARGET) surface_.Reset();
}
void Window::toggle_theme() {
    light_=!light_;
    kami::settings::set_light_appearance(light_);
    const BOOL dark=light_?FALSE:TRUE;
    DwmSetWindowAttribute(hwnd_,20,&dark,sizeof(dark));
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::select_page(unsigned index) {
    if(index>=kPages.size()) return;
    page_=index;
    InvalidateRect(hwnd_,nullptr,FALSE);
}
void Window::on_click(float x,float y) {
    RECT client{};
    GetClientRect(hwnd_,&client);
    if(x>static_cast<float>(client.right)-193.f &&
       x<static_cast<float>(client.right)-33.f &&
       y>=24.f && y<69.f) { toggle_theme(); return; }
    if(x>=29.f && x<=241.f) {
        for(unsigned i=0;i<static_cast<unsigned>(kPages.size());++i) {
            const float top=156.f+56.f*static_cast<float>(i);
            if(y>=top && y<top+43.f) { select_page(i); return; }
        }
    }
}
}
