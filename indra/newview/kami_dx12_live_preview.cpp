#include "llviewerprecompiledheaders.h"

#include "kami_dx12_live_preview.h"
#include "kami_dx12_scene_bridge.h"
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llagent.h"

#include <chrono>
#include <memory>

namespace {
constexpr wchar_t kPreviewWindowClass[] = L"KamiNativeDX12ScenePreview";
bool sPreviewCloseRequested = false;

LRESULT CALLBACK previewWindowProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message)
    {
    case WM_CLOSE:
        sPreviewCloseRequested = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1; // DXGI clears the full window using native DX12 commands.
    default:
        return DefWindowProcW(hwnd, message, wp, lp);
    }
}

class KamiDX12LivePreview final
{
public:
    ~KamiDX12LivePreview() { stop(); }

    void tick(const LLViewerCamera& camera)
    {
        if (!gSavedSettings.getBOOL("KamiDX12LivePreview"))
        {
            stop();
            return;
        }
        if (sPreviewCloseRequested)
        {
            gSavedSettings.setBOOL("KamiDX12LivePreview", false);
            stop();
            return;
        }
        // Never create the diagnostic preview on the login/loading screen.
        if (!gAgent.getRegion()) return;
        if (!mWindow && !start()) return;

        if (IsIconic(mWindow)) return;
        RECT rect{};
        if (!GetClientRect(mWindow, &rect)) return;
        const LONG width = rect.right - rect.left;
        const LONG height = rect.bottom - rect.top;
        if (width < 16 || height < 16) return;

        // Off by default, capped to 10 FPS to keep existing world movement,
        // texture streaming and CPU/FPS budgets usable during experiments.
        const auto now = std::chrono::steady_clock::now();
        if (now - mLastFrame < std::chrono::milliseconds(100)) return;
        if (!mRenderer.worldFrameAvailable()) return;

        if ((UINT)width != mWidth || (UINT)height != mHeight)
        {
            if (!mRenderer.resize((UINT)width, (UINT)height))
            {
                failAndDisable("ResizeBuffers");
                return;
            }
            mWidth = (UINT)width;
            mHeight = (UINT)height;
        }

        // Real decoded nearby prim/mesh and terrain geometry, not a sample
        // triangle. No GL readbacks, shader injections, or fake API switches.
        kami::dx12::SceneCaptureStats stats{};
        const auto scene = kami::dx12::captureLiveVolumeWorld(camera, &stats);
        if (!mRenderer.renderWorldFrame(scene, false))
        {
            failAndDisable("renderWorldFrame");
            return;
        }
        // Cool down after work completes, even when an update was slow.
        mLastFrame = std::chrono::steady_clock::now();
    }

private:
    bool start()
    {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW cls{};
        cls.lpfnWndProc = previewWindowProc;
        cls.hInstance = instance;
        cls.lpszClassName = kPreviewWindowClass;
        cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&cls) &&
            GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            failAndDisable("RegisterClassW");
            return false;
        }
        mRegistered = true;

        sPreviewCloseRequested = false;
        mWindow = CreateWindowExW(
            0, kPreviewWindowClass,
            L"Kami DX12 Scene Preview - geometry only (experimental)",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            880, 530, nullptr, nullptr, instance, nullptr);
        if (!mWindow)
        {
            failAndDisable("CreateWindowExW");
            return false;
        }
        ShowWindow(mWindow, SW_SHOW);
        RECT client{};
        GetClientRect(mWindow, &client);
        mWidth = (UINT)(client.right - client.left);
        mHeight = (UINT)(client.bottom - client.top);

        // Hardware DX12 adapter only. WARP is reserved for CI self-tests.
        if (!mRenderer.initialize(false) ||
            !mRenderer.attachWindow(mWindow, mWidth, mHeight))
        {
            failAndDisable("native DX12 initialization / swap chain");
            return false;
        }
        mLastFrame = std::chrono::steady_clock::now() -
                     std::chrono::milliseconds(200);
        LL_INFOS("KamiDX12") << "Live DX12 geometry preview started: "
                             << "native device and independent DXGI window"
                             << LL_ENDL;
        return true;
    }

    void failAndDisable(const char* action)
    {
        LL_WARNS("KamiDX12") << "DX12 scene preview " << action
                              << " failed: " << mRenderer.error()
                              << " (normal viewer remains usable)"
                              << LL_ENDL;
        gSavedSettings.setBOOL("KamiDX12LivePreview", false);
        stop();
    }

    void stop() noexcept
    {
        if (!mWindow && !mRegistered) return;
        mRenderer.shutdown();
        if (mWindow && IsWindow(mWindow)) DestroyWindow(mWindow);
        mWindow = nullptr;
        mWidth = mHeight = 0;
        if (mRegistered)
        {
            UnregisterClassW(kPreviewWindowClass, GetModuleHandleW(nullptr));
            mRegistered = false;
        }
    }

    kami::dx12::Renderer mRenderer;
    HWND mWindow = nullptr;
    UINT mWidth = 0, mHeight = 0;
    bool mRegistered = false;
    std::chrono::steady_clock::time_point mLastFrame{};
};
} // namespace

void kamiDX12LivePreviewTick(const LLViewerCamera& camera)
{
    static KamiDX12LivePreview preview;
    preview.tick(camera);
}
