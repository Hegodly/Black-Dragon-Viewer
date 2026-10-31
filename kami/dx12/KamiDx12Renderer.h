#pragma once

#if !defined(_WIN32)
#error "Kami DirectX 12 backend is Windows-only"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <cstdint>
#include <string>

namespace kami::dx12 {

// Phase 1: genuine Direct3D 12 command recording, shader pipeline, swap chain,
// GPU/CPU fencing and offscreen rendering test. This class cannot yet render
// Second Life world geometry, avatars, PBR, UI or the legacy postprocess stack.
// No OpenGL calls, shims, or GL-on-DX translation are used.
class Renderer final
{
public:
    static constexpr UINT kFrameCount = 2;
    static constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Production uses hardware; WARP is expressly a CI/development fallback.
    bool initialize(bool warpForTesting = false);
    bool attachWindow(HWND hwnd, UINT width, UINT height);
    bool resize(UINT width, UINT height);
    bool renderFrame(bool vsync = true);
    bool renderOffscreenSmokeTest(UINT width = 128, UINT height = 128);
    void shutdown() noexcept;

    bool deviceReady() const noexcept { return device_ != nullptr; }
    bool ready() const noexcept { return device_ && swapChain_; }
    const std::string& error() const noexcept { return error_; }
    const std::wstring& adapterName() const noexcept { return adapterName_; }
    bool tearingAvailable() const noexcept { return allowTearing_; }

private:
    bool fail(const char* what, HRESULT result);
    bool makePipeline();
    bool makeSwapchainTargets();
    bool waitForFence(UINT64 value);
    bool waitForFrame(UINT index);
    bool flush();
    bool encodeScene(ID3D12Resource* target, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                     UINT width, UINT height, D3D12_RESOURCE_STATES initial,
                     D3D12_RESOURCE_STATES final);
    UINT64 signal();

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kFrameCount> targets_;
    std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kFrameCount> allocators_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    std::array<UINT64, kFrameCount> frameFenceValues_{};
    UINT64 nextFenceValue_ = 1;
    UINT rtvStride_ = 0;
    UINT width_ = 0, height_ = 0;
    bool allowTearing_ = false;
    std::string error_;
    std::wstring adapterName_;
};

} // namespace kami::dx12
