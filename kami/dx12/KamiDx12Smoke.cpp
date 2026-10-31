#include "KamiDx12Renderer.h"

#include <iostream>
#include <string>

namespace {
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CLOSE) { DestroyWindow(hwnd); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}
bool failure(const kami::dx12::Renderer& renderer, const char* stage)
{
    std::cerr << "FAILED " << stage << ": " << renderer.error() << '\n';
    return false;
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    bool warp = false;
    bool visibleWindow = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"--warp") warp = true;
        else if (arg == L"--hardware") warp = false;
        else if (arg == L"--window") visibleWindow = true;
        else if (arg == L"--offscreen") visibleWindow = false;
        else
        {
            std::wcerr << L"Unknown option " << arg
                       << L"; use --warp [--offscreen|--window]\n";
            return 2;
        }
    }
    kami::dx12::Renderer renderer;
    if (!renderer.initialize(warp))
    {
        failure(renderer, "native DX12 adapter/device initialization");
        return 1;
    }
    std::wcout << L"Native DX12 device: " << renderer.adapterName() << L'\n';

    for (const UINT dimension : {64u, 128u, 256u})
    {
        if (!renderer.renderOffscreenSmokeTest(dimension, dimension))
        {
            failure(renderer, "D3D12 GPU draw + readback");
            return 3;
        }
        std::wcout << L"PASS hardware-API offscreen GPU triangle/readback "
                   << dimension << L"x" << dimension << L'\n';
        if (!renderer.renderWorldOffscreenTest(dimension, dimension))
        {
            failure(renderer, "native indexed world geometry + depth-buffer readback");
            return 9;
        }
        std::wcout << L"PASS native DX12 indexed 3D meshes, MVP, depth occlusion "
                   << dimension << L"x" << dimension << L'\n';
    }
    if (visibleWindow)
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW klass{};
        klass.lpfnWndProc = windowProc;
        klass.hInstance = instance;
        klass.lpszClassName = L"KamiDx12FoundationSmokeWindow";
        if (!RegisterClassW(&klass)) return 4;
        HWND hwnd = CreateWindowExW(0, klass.lpszClassName, L"Kami DX12 Foundation",
                                    WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                    960, 640, nullptr, nullptr, instance, nullptr);
        if (!hwnd) return 5;
        ShowWindow(hwnd, SW_SHOW);
        if (!renderer.attachWindow(hwnd, 640, 360))
        {
            failure(renderer, "DXGI HWND swap chain");
            DestroyWindow(hwnd);
            return 6;
        }
        for (int frame = 0; frame < 15; ++frame)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!renderer.renderFrame())
            {
                failure(renderer, "DX12 frame presentation");
                DestroyWindow(hwnd);
                return 7;
            }
        }
        if (!renderer.resize(800, 450) || !renderer.renderFrame(false))
        {
            failure(renderer, "DX12 resize/present");
            DestroyWindow(hwnd);
            return 8;
        }
        kami::dx12::WorldFrame world;
        kami::dx12::WorldMesh mesh;
        mesh.vertices = {
            {{-0.75f, -0.75f, 0.3f}, {1.f, 0.2f, 0.1f, 1.f}},
            {{ 0.00f,  0.75f, 0.3f}, {1.f, 0.2f, 0.1f, 1.f}},
            {{ 0.75f, -0.75f, 0.3f}, {1.f, 0.2f, 0.1f, 1.f}}
        };
        mesh.indices = {0, 1, 2};
        world.meshes.push_back(std::move(mesh));
        if (!renderer.renderWorldFrame(world, true))
        {
            failure(renderer, "DX12 indexed world frame presentation");
            DestroyWindow(hwnd);
            return 10;
        }
        renderer.shutdown();
        DestroyWindow(hwnd);
        UnregisterClassW(klass.lpszClassName, instance);
        std::cout << "PASS native DXGI flip-model swap chain and resize\n";
    }
    else
    {
        renderer.shutdown();
    }
    std::cout << "Kami DirectX 12 foundation smoke test: PASS\n";
    return 0;
}
