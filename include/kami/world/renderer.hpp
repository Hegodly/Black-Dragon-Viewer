#pragma once
#include "kami/world/device.hpp"
#include "kami/graphics/settings.hpp"
#include "kami/camera/system.hpp"
#include "kami/features/physics.hpp"
#include "kami/features/avatar_motion.hpp"
#include "kami/features/body_physics.hpp"
#include "kami/features/rig.hpp"
#include "kami/features/rig_physics.hpp"
#include "kami/features/rig_diagnostics.hpp"
#include "kami/features/photo_studio.hpp"
#include "kami/world/scene.hpp"
#include "kami/protocol/texture_decode.hpp"
#include "kami/world/region_geometry.hpp"
#include "kami/world/live_avatar.hpp"
#include <optional>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <chrono>
#include <map>
#include <cstdint>
#include <cstddef>
#include <string>
#include <windows.h>

namespace kami::protocol::sim { class Circuit; }
namespace kami::world {

// Independent DX12 viewport renderer. It draws an original offline scene.
// It is NOT an SL simulator or asset renderer.
enum class CaptureStatus { Idle, Pending, Saved, Failed };
// A still-in-flight DX12 copy is retryable, not a rendering failure.
enum class TextureUploadStatus { Committed, Deferred, Failed };
class Renderer final {
public:
    Renderer() = default;
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Software DX12 WARP may be enabled explicitly by test hosts only.
    [[nodiscard]] bool initialize(HWND window, UINT width, UINT height,
                                  bool allow_software_dx12=false,
                                  bool force_software_dx12=false);
    void resize(UINT width, UINT height);
    void set_graphics(graphics::Settings settings);
    void draw();
    void set_avatar_controls(avatar::Controls controls) noexcept {
        avatar_.set_controls(controls);
        live_preview_controls_=controls;
    }
    [[nodiscard]] bool live_avatar_placeholder_active() const noexcept {
        return region_mesh_.has_value() && live_avatar_preview_.active();
    }
    void toggle_landscape_view() noexcept;
    [[nodiscard]] bool landscape_view_active() const noexcept {
        return landscape_view_&&landscape_anchor_valid_;
    }
    [[nodiscard]] int live_avatar_heading_degrees() const noexcept {
        constexpr float kToDegrees=57.2957795131f;
        const int deg=static_cast<int>(
            live_avatar_preview_.pose().heading*kToDegrees);
        return (deg%360+360)%360;
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
    [[nodiscard]] const rig::Audit& skin_compatibility() const noexcept {
        return skin_audit_;
    }
    // Takes a validated, decoded STATIC region triangle batch. No OpenGL,
    // D3D11 or alternate renderer. Does not download SL assets by itself.
    [[nodiscard]] bool set_decoded_region_geometry(
        const protocol::Bootstrap& authenticated_region,
        SceneMesh decoded_static_geometry,bool verified_region_data);
    void clear_decoded_region_geometry();
    // Only an authenticated simulator circuit with verified water AND
    // avatar elevations can enable this partial server-height preview.
    // Does not claim to decode terrain, water textures or region objects.
    [[nodiscard]] bool set_verified_live_water(
        const protocol::sim::Circuit& circuit);
    // Render only genuine decoded land heights from the current
    // authenticated region. Materials remain clearly untextured.
    [[nodiscard]] bool set_verified_live_terrain(
        const protocol::sim::Circuit& circuit);
    void clear_live_water_preview();
    [[nodiscard]] bool live_water_preview_active() const noexcept {
        return live_water_mesh_.has_value();
    }
    [[nodiscard]] bool region_geometry_active() const noexcept {
        return region_mesh_.has_value();
    }
    // Geometry successfully accepted into the active DX12 scene buffer,
    // NOT proof it is currently visible in the camera's frustum.
    [[nodiscard]] std::size_t uploaded_live_land_vertices() const noexcept {
        // The scene's terrain pass includes unskinned amber object bounds.
        // Exclude the proxy vertices from the land-only metric.
        return region_mesh_?
            region_mesh_->terrain_count-uploaded_root_bounds_*432U-
            uploaded_primitive_vertices_-uploaded_mesh_vertices_-
            uploaded_water_vertices_:0U;
    }
    [[nodiscard]] std::size_t uploaded_root_bounds() const noexcept {
        return region_mesh_?uploaded_root_bounds_:0U;
    }
    [[nodiscard]] std::size_t uploaded_root_primitive_shapes() const noexcept {
        return region_mesh_?uploaded_root_primitive_shapes_:0U;
    }
    [[nodiscard]] std::size_t uploaded_linked_primitive_shapes() const noexcept {
        return region_mesh_?uploaded_linked_primitive_shapes_:0U;
    }
    [[nodiscard]] std::size_t uploaded_live_meshes() const noexcept {
        return region_mesh_?uploaded_mesh_shapes_:0U;
    }
    [[nodiscard]] std::size_t uploaded_mesh_triangles() const noexcept {
        return region_mesh_?uploaded_mesh_triangles_:0U;
    }
    [[nodiscard]] bool texture_upload_pending() const noexcept {
        return texture_upload_pending_;
    }
    [[nodiscard]] std::size_t bound_sl_texture_assets() const noexcept {
        return region_mesh_?live_texture_layers_.size():0U;
    }
    [[nodiscard]] std::size_t uploaded_textured_triangles() const noexcept {
        return region_mesh_?uploaded_textured_triangles_:0U;
    }
    [[nodiscard]] std::size_t live_primitive_eligible() const noexcept {
        return region_mesh_?primitive_eligible_:0U;
    }
    [[nodiscard]] std::size_t live_primitive_unresolved() const noexcept {
        return region_mesh_?primitive_unresolved_:0U;
    }
    [[nodiscard]] std::size_t live_missing_anchor_shapes() const noexcept {
        return region_mesh_?primitive_missing_anchor_:0U;
    }
    [[nodiscard]] std::size_t live_avatar_attachment_shapes() const noexcept {
        return region_mesh_?primitive_avatar_parent_:0U;
    }
    [[nodiscard]] std::size_t live_cycle_shapes() const noexcept {
        return region_mesh_?primitive_cycle_:0U;
    }
    [[nodiscard]] std::size_t live_unique_missing_parent_ids() const noexcept {
        return region_mesh_?primitive_unique_missing_:0U;
    }
    [[nodiscard]] std::size_t live_primitive_distant() const noexcept {
        return region_mesh_?primitive_distant_:0U;
    }
    [[nodiscard]] std::size_t live_primitive_deferred() const noexcept {
        return region_mesh_?primitive_deferred_:0U;
    }
    [[nodiscard]] bool live_water_surface_active() const noexcept {
        return region_mesh_ && uploaded_water_vertices_>0U;
    }
    // Real decoded terrain cells omitted solely for extreme height
    // discontinuities; never counted as rendered geometry.
    [[nodiscard]] std::size_t skipped_terrain_discontinuities() const noexcept {
        return region_mesh_?skipped_terrain_discontinuities_:0U;
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
    [[nodiscard]] bool software_dx12_active() const noexcept {
        return device_.status()==DeviceStatus::ReadySoftware;
    }
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
    // Layer zero is the original procedural fallback. Other slices are
    // authenticated, CPU-decoded SL default TextureEntry images.
    [[nodiscard]] TextureUploadStatus make_material_texture(
        const std::map<std::string,protocol::texture::DecodedTexture>* images=nullptr,
        const std::map<std::string,std::uint16_t>* previous_layers=nullptr);
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
    rig::Audit skin_audit_{};
    std::optional<SceneMesh> preview_mesh_;
    std::optional<SceneMesh> region_mesh_;
    // Fixed per-region spawn origin: server position changes may select
    // nearer objects, but must never shift old world geometry away from
    // the camera/locally predicted avatar marker.
    std::array<float,3> live_region_origin_{};
    bool live_region_origin_valid_=false;
    std::size_t uploaded_root_bounds_=0;
    std::size_t uploaded_root_primitive_shapes_=0;
    std::size_t uploaded_linked_primitive_shapes_=0;
    std::size_t uploaded_primitive_vertices_=0;
    std::size_t uploaded_mesh_vertices_=0,uploaded_mesh_shapes_=0;
    std::size_t uploaded_mesh_triangles_=0;
    std::size_t uploaded_textured_triangles_=0;
    std::map<std::string,std::uint16_t> live_texture_layers_;
    bool texture_upload_pending_=false;
    static constexpr UINT kLiveTextureLayers=48U;
    static constexpr UINT kLiveTextureSize=256U;
    std::size_t primitive_eligible_=0;
    std::size_t primitive_unresolved_=0;
    std::size_t primitive_missing_anchor_=0,primitive_avatar_parent_=0;
    std::size_t primitive_cycle_=0,primitive_unique_missing_=0;
    std::size_t primitive_distant_=0;
    std::size_t primitive_deferred_=0;
    std::size_t uploaded_water_vertices_=0;
    std::size_t skipped_terrain_discontinuities_=0;
    std::optional<SceneMesh> live_water_mesh_;
    LiveAvatarPreview live_avatar_preview_{};
    bool landscape_view_=false;
    bool landscape_anchor_valid_=false;
    camera::Vec3 landscape_anchor_{};
    avatar::Controls live_preview_controls_{};
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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> sky_pipeline_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commands_;
    // R51: texture copy work must not reset the frame's command allocator.
    // One dedicated copy list is submitted to the same ordered DX12 queue.
    // The upload heap stays alive until its own fence has completed.
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> texture_copy_allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> texture_copy_commands_;
    UINT64 texture_copy_submitted_fence_=0U;
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
