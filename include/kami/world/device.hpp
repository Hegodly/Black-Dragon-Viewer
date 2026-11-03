#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>

namespace kami::world {

// KAMI's 3D world will use this original DX12 backend; the native 2D UI
// remains separate. A valid device is NOT proof of world rendering support.
enum class DeviceStatus {
    NotInitialized,
    ReadyHardware,
    ReadySoftware,
    Unavailable
};

class Device final {
public:
    [[nodiscard]] DeviceStatus initialize(bool allow_software_fallback = true) noexcept;
    [[nodiscard]] DeviceStatus status() const noexcept { return status_; }
    [[nodiscard]] bool is_ready() const noexcept {
        return status_ == DeviceStatus::ReadyHardware ||
               status_ == DeviceStatus::ReadySoftware;
    }
    [[nodiscard]] ID3D12Device* native_device() const noexcept { return device_.Get(); }
    [[nodiscard]] ID3D12CommandQueue* graphics_queue() const noexcept {
        return graphics_queue_.Get();
    }
    [[nodiscard]] const std::wstring& adapter_name() const noexcept {
        return adapter_name_;
    }
private:
    bool make_device(IDXGIAdapter1* adapter, bool software) noexcept;
    DeviceStatus status_ = DeviceStatus::NotInitialized;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> graphics_queue_;
    std::wstring adapter_name_;
};
} // namespace kami::world
