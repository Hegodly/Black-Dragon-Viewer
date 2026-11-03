#pragma once
#include "kami/world/device.hpp"
#include "kami/graphics/settings.hpp"
#include "kami/camera/system.hpp"
#include "kami/features/physics.hpp"
#include "kami/features/avatar_motion.hpp"
#include "kami/features/body_physics.hpp"
#include "kami/features/rig.hpp"
#include "kami/features/rig_physics.hpp"
#include "kami/features/photo_studio.hpp"
#include "kami/world/scene.hpp"
#include <optional>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <string>
#include <windows.h>

namespace kami::world {

// Independent DX12 viewport renderer. It draws an original offline scene.
// It is NOT an SL simulator or asset renderer.
enum class CaptureStatus { Idle, Pending, Saved, Failed };
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
    void set_avatar_controls(avatar::Controls controls) noexcept {
        avatar_.set_controls(controls);
    }
    [[nodiscard]] const avatar::State& avatar_state() const noexcept {
        return avatar_.state();
    }
    void reset_avatar() noexcept;
    void set_body_physics_profile(avatar::BodyProfile profile) noexcept {
        rig_physics_.set_profile(profile);
    }
    [[nodiscard]] avatar::BodyProfile body_physics_profile() const noexcept {
        return rig_physics_.profile();
    }
    [[nodiscard]] avatar::PhysicsOffsets body_physics_offsets() const noexcept {
        const auto state=rig_physics_.output();
        return {state.at(rigphysics::Axis::ChestVertical),
                state.at(rigphysics::Axis::BellyVertical),
                state.at(rigphysics::Axis::ButtVertical)};
    }
    [[nodiscard]] rigphysics::Output detailed_body_physics_offsets() const noexcept {
        return rig_physics_.output();
    }
    [[nodiscard]] bool set_decoded_physics_wearable(
        std::span<const rigphysics::NamedParameter> values) noexcept {
        rigphysics::Wearable wearable{};
        if(!rigphysics::decode_named_parameters(values,wearable))return false;
        rig_physics_.set_wearable(wearable);
        return true;
    }
    [[nodiscard]] bool set_creator_preview(SceneMesh mesh);
    void clear_creator_preview();
    [[nodiscard]] bool creator_preview_active() const noexcept {return preview_mesh_.has_value();}
    void set_photo_settings(photo::Settings value) noexcept { photo_=photo::normalize(value); }
    [[nodiscard]] bool capture_png(std::wstring destination);
    [[nodiscard]] CaptureStatus capture_status() const noexcept {return capture_status_;}
    [[nodiscard]] const std::wstring& capture_path() const noexcept {return capture_path_;}

    void orbit(float yaw_delta, float elevation_delta);
    void zoom(float distance_delta);
    void set_camera_settings(camera::Settings settings) noexcept { camera_.set_settings(settings); }
    void pointer_move(float dx, float dy) noexcept { camera_.pointer_delta(dx, dy); }
    void set_camera_dragging(bool value) noexcept { camera_.set_dragging(value); }
    void cycle_camera_shoulder() noexcept { camera_.cycle_shoulder(); }
    void toggle_camera_precision() noexcept { camera_.toggle_precision(); }
    void toggle_camera_focus() noexcept { camera_.toggle_focus_lock(); }
    void toggle_cinematic_orbit() noexcept { camera_.toggle_cinematic_orbit(); }
    void reset_camera() noexcept { camera_.reset(); }
    void apply_studio_shot(camera::StudioShot shot) noexcept {camera_.apply_studio_shot(shot);}
    [[nodiscard]] const camera::System& camera_system() const noexcept { return camera_; }
    void shutdown() noexcept;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] ID3D12Device* native_device() const noexcept {return device_.native_device();}
    [[nodiscard]] const std::wstring& adapter_name() const noexcept {
        return device_.adapter_name();
    }
    [[nodiscard]] const std::wstring& error() const noexcept { return error_; }
    [[nodiscard]] std::size_t vertex_count() const noexcept {
        return terrain_count_ + avatar_count_;
    }

private:
    static constexpr UINT kBuffers = 2;
    static constexpr UINT kShadowSize = 1024;
    [[nodiscard]] bool make_pipeline();
    [[nodiscard]] bool make_geometry();
    [[nodiscard]] bool make_targets();
    [[nodiscard]] bool make_depth();
    [[nodiscard]] bool make_shadow();
    [[nodiscard]] bool make_material_texture();
    [[nodiscard]] bool make_skin_palette();
    [[nodiscard]] bool update_skin_palette() noexcept;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE shadow_dsv() const noexcept;
    void wait_for_gpu() noexcept;
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle(UINT index) const noexcept;

    Device device_;
    HWND window_ = nullptr;
    UINT width_ = 0, height_ = 0;
    UINT client_width_ = 0, client_height_ = 0;
    bool allow_tearing_ = false;
    graphics::Settings graphics_{};
    bool ready_ = false;
    camera::System camera_{};
    physics::Spring avatar_spring_{};
    avatar::Controller avatar_{};
    rigphysics::Solver rig_physics_{};
    rig::Palette skin_matrices_{};
    std::optional<SceneMesh> preview_mesh_;
    photo::Settings photo_{};
    CaptureStatus capture_status_=CaptureStatus::Idle;
    std::wstring capture_path_;

    std::chrono::steady_clock::time_point last_camera_frame_{};
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    std::wstring error_;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapchain_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadow_srv_heap_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBuffers> buffers_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shadow_map_;
    Microsoft::WRL::ComPtr<ID3D12Resource> material_texture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> material_upload_;
    Microsoft::WRL::ComPtr<ID3D12Resource> skin_palette_buffer_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> signature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadow_pipeline_;
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
