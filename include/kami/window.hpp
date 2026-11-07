#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <string_view>
#include <array>
#include <chrono>
#include <future>
#include <cstdint>
#include <string>
#include "kami/graphics/settings.hpp"
#include "kami/graphics/adaptive.hpp"
#include "kami/features/telemetry.hpp"
#include "kami/features/creator_studio.hpp"
#include "kami/features/photo_studio.hpp"
#include "kami/features/messenger.hpp"

#include "kami/palette.hpp"
#include "kami/session.hpp"
#include "kami/protocol/transport.hpp"
#include "kami/protocol/login.hpp"
#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/caps.hpp"
#include "kami/world/renderer.hpp"
#include "kami/world/warmup.hpp"

namespace kami::app {
class Window final {
public:
    [[nodiscard]] bool create(HINSTANCE instance, int show);
    [[nodiscard]] bool wants_realtime_render() const noexcept;
    void tick();
private:
    static LRESULT CALLBACK dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK viewport_dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK messenger_edit_dispatch(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT message, WPARAM wp, LPARAM lp);
    LRESULT viewport_handle(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    [[nodiscard]] bool create_resources();
    void paint();
    void resize(UINT width, UINT height);
    void update_viewport();
    void select_page(unsigned page);
    void toggle_appearance();
    void click(float x, float y);
    void advance_graphics(unsigned row);
    void advance_graphics_effects(unsigned row);
    void advance_camera(unsigned row);
    void cycle_body_physics();
    void advance_photo(unsigned row);
    void open_creator_obj();
    void save_photo_png();
    void update_messenger_composer();
    void messenger_send();
    void messenger_demo_reply();
    void messenger_select(messenger::Channel channel);
    void check_sl_tls_connection();
    void begin_sl_login();
    void start_caps_poll();
    void update_sl_connection();
    void start_inventory_refresh();

    void round_rect(D2D1_RECT_F bounds, float radius, D2D1_COLOR_F color);
    void label(std::wstring_view text, D2D1_RECT_F bounds, IDWriteTextFormat* font, D2D1_COLOR_F color);
    void draw_interface(float width, float height, const ui::Palette& palette);

    HWND hwnd_ = nullptr;
    HWND viewport_ = nullptr;
    HWND messenger_edit_ = nullptr;
    WNDPROC messenger_edit_original_ = nullptr;
    HBRUSH messenger_edit_brush_ = nullptr;
    bool messenger_edit_sync_ = false;
    // Track simulator chat availability so the composer follows live connection changes.
    bool messenger_nearby_online_ = false;
    bool world_initialized_ = false;
    bool world_ready_ = false;
    world::Renderer world_renderer_;
    world::ShaderWarmup shader_warmup_;
    graphics::Settings graphics_{};
    graphics::AdaptiveQuality adaptive_{};
    telemetry::Monitor performance_monitor_{};
    telemetry::Snapshot last_performance_{};
    bool graphics_effects_page_ = false;
    camera::Settings camera_settings_{};
    bool camera_settings_page_ = false;
    bool camera_dragging_ = false;
    int pointer_x_ = 0, pointer_y_ = 0;
    bool high_resolution_timer_ = false;
    std::chrono::steady_clock::time_point next_frame_{};
    std::chrono::steady_clock::time_point fps_epoch_{};
    unsigned fps_frames_ = 0;
    unsigned fps_display_ = 0;
    creator::Preview creator_preview_{};
    std::wstring creator_file_;
    std::wstring creator_message_ = L"Choose a local OBJ file to preview.";
    photo::Settings photo_settings_{};
    unsigned photo_shot_ = 0;
    std::wstring photo_message_ = L"DX12 live preview - capture PNG to save.";
    world::CaptureStatus prior_capture_=world::CaptureStatus::Idle;
    messenger::Model messenger_{};
    bool light_ = false;
    bool seven_systems_open_ = false;
    unsigned page_ = 0;
    protocol::Session session_;
    protocol::ProbeResult sl_tls_probe_{};
    protocol::LoginStatus sl_login_status_=protocol::LoginStatus::NotAttempted;
    std::wstring sl_login_message_=L"SL login not attempted";
    unsigned sl_login_http_status_=0;
    std::wstring sl_login_response_kind_;
    bool sl_login_pending_=false;
    std::future<protocol::LoginResult> sl_login_future_;
    protocol::sim::Circuit sl_circuit_;
    bool live_water_attempted_=false;
    std::wstring live_water_status_;
    std::uint64_t live_land_uploaded_revision_=0;
    std::uint64_t live_spatial_uploaded_revision_=0;
    // Asset completion can occur without a simulator spatial packet.
    std::size_t live_uploaded_texture_count_=0;
    std::size_t live_uploaded_mesh_count_=0;
    std::size_t live_uploaded_material_count_=0;
    std::chrono::steady_clock::time_point live_land_last_upload_{};
    // Last verified scene origin. Local avatar movement must not force
    // a full terrain/mesh rebase every 800 ms; 12m travel still does.
    std::array<float,3> live_scene_uploaded_origin_{};
    bool live_scene_origin_valid_=false;
    // Bearer CAPS URLs are retained only for the authenticated region
    // connection. Never display signed URLs or event payloads.
    std::wstring caps_seed_url_;
    std::wstring caps_event_queue_url_; // secret signed bearer URL, never shown
    protocol::caps::QueueCursor caps_ack_{};
    std::wstring caps_status_;
    // Persistent, URL-free seed outcome; polling must not overwrite it.
    std::wstring caps_seed_summary_;
    std::wstring caps_recent_types_; // sanitized names only, no LLSD bodies
    unsigned caps_completed_polls_=0;
    std::uint64_t caps_events_observed_=0;
    std::uint64_t caps_events_unsupported_=0;
    std::uint64_t caps_notifications_handled_=0;
    std::wstring caps_last_notice_; // sanitized TeleportFailed only
    unsigned caps_consecutive_failures_=0;
    bool caps_dispatch_blocked_=false;
    std::chrono::steady_clock::time_point caps_next_poll_{};
    std::chrono::steady_clock::time_point caps_seed_retry_after_{};
    unsigned caps_seed_attempts_=0U;
    bool caps_seed_started_=false;
    bool caps_poll_started_=false;
    std::future<protocol::caps::SeedResponse> caps_seed_future_;
    std::future<protocol::caps::PollResponse> caps_poll_future_;
    std::future<protocol::caps::InventoryTreeFetchResult> inventory_root_future_;
    protocol::caps::InventoryFolderReply inventory_root_snapshot_{};
    std::wstring inventory_status_=L"Waiting for authenticated inventory";
    std::wstring inventory_search_;
    std::string inventory_current_folder_;
    std::string inventory_selected_entry_;
    unsigned inventory_sort_mode_=0;
    bool inventory_group_creators_=false;
    bool inventory_search_focused_=false;
    std::wstring inventory_search_;
    std::string inventory_current_folder_;
    std::string inventory_selected_entry_;
    unsigned inventory_sort_mode_=0;
    bool inventory_group_creators_=false;
    bool inventory_search_focused_=false;

    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<IDWriteFactory> text_factory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> hero_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> title_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> body_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> small_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> messenger_centered_font_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> messenger_centered_small_;
};
}
