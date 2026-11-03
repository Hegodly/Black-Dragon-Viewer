#include "kami/world/device.hpp"

namespace kami::world {

bool Device::make_device(IDXGIAdapter1* adapter, bool software) noexcept {
    Microsoft::WRL::ComPtr<ID3D12Device> candidate;
    if (FAILED(D3D12CreateDevice(
            adapter, D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(candidate.GetAddressOf()))))
        return false;

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(candidate->CreateCommandQueue(
            &queue_desc, IID_PPV_ARGS(queue.GetAddressOf()))))
        return false;

    DXGI_ADAPTER_DESC1 info{};
    if (FAILED(adapter->GetDesc1(&info)))
        return false;
    device_ = std::move(candidate);
    graphics_queue_ = std::move(queue);
    adapter_name_ = info.Description;
    status_ = software ? DeviceStatus::ReadySoftware : DeviceStatus::ReadyHardware;
    return true;
}

DeviceStatus Device::initialize(bool allow_software_fallback) noexcept {
    if (is_ready()) return status_;
    status_ = DeviceStatus::Unavailable;
    device_.Reset();
    graphics_queue_.Reset();
    adapter_name_.clear();

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(factory.GetAddressOf()))))
        return status_;

    // Discover hardware adapters through DXGI instead of vendor-specific code.
    // Do not silently initialize an OpenGL or D3D11 world renderer.
    for (UINT index = 0; ; ++index) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        const HRESULT result = factory->EnumAdapterByGpuPreference(
            index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            IID_PPV_ARGS(adapter.GetAddressOf()));
        if (result == DXGI_ERROR_NOT_FOUND) break;
        if (FAILED(result)) break;

        DXGI_ADAPTER_DESC1 info{};
        if (FAILED(adapter->GetDesc1(&info))) continue;
        if (info.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (make_device(adapter.Get(), false)) return status_;
    }

    // WARP is an explicit DX12-only diagnostic compatibility fallback. It
    // is not a promise of playable FPS on unsupported integrated graphics.
    if (allow_software_fallback) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> warp;
        if (SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(warp.GetAddressOf())))
            && make_device(warp.Get(), true))
            return status_;
    }
    return status_;
}
} // namespace kami::world
