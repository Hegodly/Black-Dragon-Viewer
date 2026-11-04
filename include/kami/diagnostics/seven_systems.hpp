#pragma once
#include <array>
#include <cstdint>

namespace kami::diagnostics {
// A single, read-only overview of SEVEN development workstreams.
// Observing one packet or a local preview NEVER means complete live SL parity.
enum class Stage : std::uint8_t { Waiting, Partial, LocalOnly };
struct Evidence {
    std::uint32_t object_bounds_drawn=0;
    bool live_terrain_uploaded=false;
    bool local_avatar_visible=false;
    std::uint32_t movement_packets_sent=0;
    bool authenticated_simulator=false;
    bool dx12_ready=false;
};
struct Check {
    const wchar_t* title;
    const wchar_t* details;
    Stage stage;
};
[[nodiscard]] inline std::array<Check,7> evaluate(
    const Evidence& e) noexcept {
    return {{
       {L"1  OBJECTS AND BUILDINGS",
        e.object_bounds_drawn>0U?
        L"Live SL root bounds drawn; prim/mesh NOT implemented":
        L"No verified root bounds drawn; real meshes pending",
        e.object_bounds_drawn>0U?Stage::Partial:Stage::Waiting},
       {L"2  TERRAIN AND TEXTURES",
        e.live_terrain_uploaded?
        L"Live SL terrain in DX12; textures/materials NOT loaded":
        L"Awaiting live DX12 land; texture pipeline pending",
        e.live_terrain_uploaded?Stage::Partial:Stage::Waiting},
       {L"3  REAL AVATAR APPEARANCE",
        e.local_avatar_visible?
        L"Blue local placeholder; real body/outfit assets pending":
        L"Avatar not visible; SL appearance/assets pending",
        e.local_avatar_visible?Stage::LocalOnly:Stage::Waiting},
       {L"4  MOVEMENT AND INTERACTIONS",
        e.movement_packets_sent>0U && e.authenticated_simulator?
        L"Movement packets sent; live pose sync/teleports pending":
        L"Awaiting authenticated movement control test",
        e.movement_packets_sent>0U&&e.authenticated_simulator?
        Stage::Partial:Stage::Waiting},
       {L"5  MESSAGING AND INVENTORY",
        L"Local UI/previews only; live chat/inventory NOT synced",
        Stage::LocalOnly},
       {L"6  SECURITY AND PERFORMANCE",
        e.dx12_ready?
        L"DX12 initialized; security/performance validation incomplete":
        L"DX12 unavailable; full security/performance tests pending",
        e.dx12_ready?Stage::Partial:Stage::Waiting},
       {L"7  KAMI GRAPHICS AND INTERFACE",
        L"Native interface/controls ready; updater and parity pending",
        Stage::LocalOnly}
    }};
}
[[nodiscard]] inline const wchar_t* stage_label(Stage stage) noexcept {
    switch(stage){
        case Stage::Waiting:return L"WAITING";
        case Stage::Partial:return L"PARTIAL";
        case Stage::LocalOnly:return L"LOCAL ONLY";
    }
    return L"UNKNOWN";
}
} // namespace kami::diagnostics
