#include "kami/window.hpp"
#include <windows.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    if(FAILED(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED))) {
        MessageBoxW(nullptr,L"KAMI could not initialize the Windows UI.",
                    L"KAMI",MB_ICONERROR);
        return 1;
    }
    int code=1;
    {
        kami::app::Window window;
        if(window.create(instance,show)) {
            MSG message{};
            for(;;) {
                const int result=GetMessageW(&message,nullptr,0,0);
                if(result==-1) { code=1; break; }
                if(result==0) { code=static_cast<int>(message.wParam); break; }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        } else {
            MessageBoxW(nullptr,L"Unable to create KAMI's native window.",
                        L"KAMI",MB_ICONERROR);
        }
    }
    CoUninitialize();
    return code;
}
