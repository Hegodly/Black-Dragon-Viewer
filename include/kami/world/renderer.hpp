#pragma once
#include "kami/world/device.hpp"
#include "kami/graphics/settings.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <cstddef>
#include <string>
#include <windows.h>

namespace kami::world {

// Independent DX12 viewport renderer. It draws an original offline scene.
// It is NOT an SL simulator or asset renderer.
class Renderer final {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    [[nodiscard]] bool initialize(HWND window, UINT width, UINT height);
    void resize(UINT width, UINT height);
    void set_graphics(graphics::Settings settings);
    void draw();
    void orbit(float yaw_delta, float elevation_delta);
    void zoom(float distance_delta);
    void shutdown() noexcept;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] const std::wstring& adapter_name() const noexcept {
        return device_.adapter_name();
    }
    [[nodiscard]] const std::wstring& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t vertex_count() const noexcept {
        return terrain_count_ + avatar_count_;
    }

private:
    static constexpr UINT kBuffers = 2;
    [[nodiscard]] bool make_pipeline();
    [[nodiscard]] bool make_geometry();
    [[nodiscard]] bool make_targets();
    [[nodiscard]] bool make_depth();
    void wait_for_gpu() noexcept;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle(UINT index) const noexcept;

    Device device_;
    HWND window_ = nullptr;
    UINT width_ = 0, height_ = 0;
    UINT client_width_ = 0, client_height_ = 0;
    bool allow_tearing_ = false;
    graphics::Settings graphics_{};
    bool ready_ = false;
    float yaw_ = .65f;
    float elevation_ = 6.8f;
    float distance_ = 17.f;
    std::wstring error_;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBuffers> buffers_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    Microsoft::WRL::ComPtr<ID3D12Resource> geometry_;
    D3D12_VERTEX_BUFFER_VIEW vertex_view_{};
    std::size_t terrain_count_ = 0;
    std::size_t avatar_count_ = 0;
    UINT rtv_step_ = 0;
    UINT64 fence_counter_ = 0;
    HANDLE fence_event_ = nullptr;
};
} // namespace kami::world
