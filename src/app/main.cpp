#include "kami/window.hpp"
#include <windows.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    SetProcessDPIAware();
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) {
        MessageBoxW(nullptr, L"Unable to initialize the KAMI Windows interface.",
                    L"KAMI", MB_OK | MB_ICONERROR);
        return 1;
    }
    int exit_code = 1;
    {
        kami::app::Window app;
        if (app.create(instance, show)) {
            MSG message{};
            while (true) {
                const BOOL result = GetMessageW(&message, nullptr, 0, 0);
                if (result == -1) { exit_code = 1; break; }
                if (result == 0) { exit_code = static_cast<int>(message.wParam); break; }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        } else {
            MessageBoxW(nullptr, L"Could not create the KAMI application window.",
                        L"KAMI", MB_OK | MB_ICONERROR);
        }
    }
    CoUninitialize();
    return exit_code;
}
