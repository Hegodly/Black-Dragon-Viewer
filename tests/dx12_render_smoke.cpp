#include "kami/world/renderer.hpp"
#include "kami/graphics/settings.hpp"
#include <windows.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace {
LRESULT CALLBACK smoke_window(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    return DefWindowProcW(window,message,wp,lp);
}
void print_failure(const wchar_t* step,const kami::world::Renderer& renderer) {
    std::wcerr<<L"KAMI DX12 smoke FAILED at "<<step
              <<L": "<<renderer.error()<<L"\n";
}
}
int main() {
    auto instance=GetModuleHandleW(nullptr);
    WNDCLASSEXW desc{};
    desc.cbSize=sizeof(desc);
    desc.hInstance=instance;
    desc.lpfnWndProc=smoke_window;
    desc.lpszClassName=L"KAMI_DX12_Render_Smoke";
    if(!RegisterClassExW(&desc)) {
        std::wcerr<<L"NO WINDOWING: cannot register a test window\n";
        return 77;
    }
    HWND window=CreateWindowExW(0,desc.lpszClassName,L"KAMI DX12 Renderer Smoke",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,680,520,
        nullptr,nullptr,instance,nullptr);
    if(!window) {
        std::wcerr<<L"NO WINDOWING: test window creation unavailable\n";
        return 77;
    }
    ShowWindow(window,SW_SHOWNOACTIVATE);
    int result=0;
    {
        kami::world::Renderer renderer;
        if(!renderer.initialize(window,640,480)) {
            print_failure(L"initialize",renderer);
            // No hardware/display in an automated server VM can prevent
            // an HWND swap chain. Report SKIPPED, not a false GPU pass.
            if(renderer.error().find(L"No DirectX 12 device")!=std::wstring::npos ||
               renderer.error().find(L"swap chain")!=std::wstring::npos)
                result=77;
            else result=1;
        } else {
            if(renderer.vertex_count()==0) {
                print_failure(L"empty scene",renderer);
                result=2;
            } else {
                renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"initial draw",renderer);
                    result=3;
                }
                kami::graphics::Settings quality{};
                quality.preset=kami::graphics::Preset::Quality;
                quality.render_scale_percent=75;
                quality.vsync=false;
                quality.pbr=true;
                quality.shadows=true;
                quality.animate_avatar=true;
                renderer.set_graphics(quality);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"PBR and shadow preset",renderer);
                    result=4;
                }
                quality.shadows=false;
                quality.pbr=false;
                renderer.set_graphics(quality);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"no-PBR no-shadows",renderer);
                    result=5;
                }
                renderer.resize(400,300);
                renderer.orbit(.18f,.20f);
                renderer.zoom(-1.f);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"resize and camera",renderer);
                    result=6;
                }
                if(!result) {
                    auto preview=kami::world::build_test_scene(8);
                    if(!renderer.set_creator_preview(preview)) {
                        print_failure(L"OBJ preview geometry",renderer);result=7;
                    } else {
                        renderer.draw();
                        if(!renderer.ready()){print_failure(L"preview draw",renderer);result=8;}
                        renderer.clear_creator_preview();
                        if(renderer.ready()) renderer.draw();
                        if(!renderer.ready()){print_failure(L"preview restore",renderer);result=9;}
                    }
                }
                if(!result) {
                    wchar_t folder[MAX_PATH]{};
                    if(!GetTempPathW(MAX_PATH,folder))result=10;
                    else {
                        const std::wstring target=std::wstring(folder)+L"KAMI-DX12-Capture-Test.png";
                        if(!renderer.capture_png(target))result=11;
                        else {
                            renderer.draw();
                            if(renderer.capture_status()!=kami::world::CaptureStatus::Saved)
                                result=12;
                            else if(!std::filesystem::exists(target))
                                result=13;
                        }
                        std::error_code ignored;
                        std::filesystem::remove(target,ignored);
                    }
                }
                if(!result) std::wcout
                    <<L"KAMI DX12 SMOKE PASS - adapter: "<<renderer.adapter_name()
                    <<L"; drawn="<<renderer.vertex_count()
                    <<L" vertices; PBR, shadows, texture upload, resize, FPS presentation path\n";
            }
        }
        renderer.shutdown();
    }
    DestroyWindow(window);
    UnregisterClassW(desc.lpszClassName,instance);
    if(result!=0 && result!=77)
        std::wcerr<<L"KAMI DX12 smoke failure stage "<<result<<L"\\n";
    return result;
}
