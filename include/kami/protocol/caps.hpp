#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <span>

namespace kami::protocol::caps {
// CAPS event intake: bounded observations plus a narrow, explicitly
// supported teleport-failure notification. All other message bodies
// remain unsupported; this does NOT implement region transitions.
// Only retry transient seed failures with a strict per-login bound.
// Permanent security/parse rejections must never spin or reuse bearer URLs.
[[nodiscard]] inline bool retry_seed_response(unsigned status,
                                             unsigned attempts) noexcept {
    return attempts<3U&&
        (status==0U||status==408U||status==429U||
         status==500U||status==502U||status==503U||status==504U);
}
struct SeedResponse {
    bool granted=false;
    bool event_queue_granted=false;
    bool mesh_granted=false;
    bool texture_granted=false;
    bool inventory_descendents_granted=false;
    bool inventory_items_granted=false;
    bool mesh_named=false;  // grant mentioned but absent or invalid
    bool mesh_rejected=false; // grant violates URL allowlist
    unsigned http_status=0;
    std::wstring event_queue_url; // secret bearer capability; do not log
    std::wstring mesh_capability_url; // ViewerAsset or GetMesh, never logged
    std::wstring texture_capability_url; // ViewerAsset only, never logged
    std::wstring inventory_descendents_url; // FetchInventoryDescendents2 bearer URL
    std::wstring inventory_items_url; // FetchInventory2 bearer URL
    std::wstring diagnostic;      // contains no URLs or response content
};
struct QueueCursor {
    std::string token; // server-provided event-queue acknowledgement ID
    bool integer=true; // preserve LLSD XML type across polling
};
struct EventQueueFrame {
    QueueCursor id;
    unsigned event_count=0;
    std::vector<std::string> event_types; // sanitized names, at most 8
    unsigned handled_events=0; // bounded user-visible notifications only
    std::vector<std::string> notifications; // sanitized, no bearer URLs
    // No other raw event body or simulator credentials are retained.
};
struct PollResponse {
    bool structurally_valid=false;
    bool idle=false;
    bool transient=false; // retry bounded transient HTTP/WinHTTP faults
    bool safe_to_ack=false; // UI must commit observation before next ACK
    unsigned http_status=0;
    unsigned event_count=0;
    unsigned unsupported_events=0; // explicitly observed but NOT applied
    unsigned handled_events=0; // TeleportFailed notifications only
    std::vector<std::string> notifications; // safe and bounded text
    std::vector<std::string> event_types; // no sensitive LLSD bodies
    QueueCursor next_ack; // only valid when safe_to_ack=true
    std::wstring diagnostic; // no event bodies, credentials or bearer URLs
};
// Strict Linden-owned URL check for EventQueue and seed.
[[nodiscard]] bool trusted_caps_url(std::wstring_view url) noexcept;
// Mesh only: authenticated seed grants may use a public HTTPS CDN host.
// Legacy HTTP is accepted solely for the public root of Linden-owned
// asset-cdn.*.lindenlab.com on port 80, with no credential query/path.
// No redirects, IP literals, private/reserved hosts or arbitrary HTTP.
[[nodiscard]] bool trusted_mesh_caps_url(std::wstring_view url) noexcept;
// Do not expose bearer URLs in diagnostics. A server may grant one CAPS
// service without the other. Reject malformed/unsafe URLs individually.
struct SeedParseReport {
    bool xml_map=false;
    bool event_named=false,mesh_named=false;
    bool event_rejected=false,mesh_rejected=false;
    std::wstring mesh_issue; // safe category only; no hostname or bearer path
};
// These parse only the bounded LLSD XML envelope, never execute events.
// Returns true when at least one supported, trusted capability is present.
[[nodiscard]] bool parse_seed_llsd(std::string_view xml,
                                   std::wstring& event_queue_url,
    std::wstring* mesh_capability_url=nullptr,
    SeedParseReport* report=nullptr,
    std::wstring* texture_capability_url=nullptr,
    std::wstring* inventory_descendents_url=nullptr,
    std::wstring* inventory_items_url=nullptr);
[[nodiscard]] bool parse_event_queue_llsd(std::string_view xml,
                                          unsigned& event_count);
// Full bounded event queue: parses a sanitized TeleportFailed reason only.
 // All other event bodies are ignored. Successful committed observation can
 // be acknowledged so the next long-poll can proceed.
[[nodiscard]] bool parse_event_queue_frame(
    std::string_view xml,EventQueueFrame& out);
[[nodiscard]] std::string build_event_queue_post(const QueueCursor& previous);
// Official FetchInventoryDescendents2 request body. UUIDs are validated
// before serialization; callers supply only authenticated agent/folder IDs.
[[nodiscard]] std::string build_inventory_folder_post(
    std::string_view folder_id,std::string_view owner_id,
    bool fetch_folders=true,bool fetch_items=true);
// Deterministic, offline-testable version of the live HTTP envelope observer.
// Observation can advance a typed cursor but never implies event actions ran.
[[nodiscard]] PollResponse observe_event_queue_response(std::string_view xml);
// A malformed WinHTTP response (12152) may be a long-poll edge response,
// but is not proof of a healthy event queue. Retry without advancing ACK.
[[nodiscard]] bool retryable_event_queue_http_error(unsigned code) noexcept;
[[nodiscard]] bool possible_idle_event_queue_http_error(unsigned code,
                                                       bool held_long) noexcept;
struct InventoryEntry {
    std::string id,parent_id;
    std::wstring name;
    bool folder=false;
};
struct InventoryFolderReply {
    bool valid=false;
    std::string folder_id;
    unsigned version=0;
    std::vector<InventoryEntry> entries;
};
// Parse one bounded FetchInventoryDescendents2 LLSD response. Unknown
// fields are ignored; malformed IDs, duplicate IDs, or wrong folder IDs fail.
[[nodiscard]] bool parse_inventory_folder_llsd(
    std::string_view xml,std::string_view expected_folder_id,
    InventoryFolderReply& out);
struct InventoryFetchResult {
    bool fetched=false;
    unsigned http_status=0;
    unsigned transport_error=0;
    InventoryFolderReply folder;
};
// Authenticated HTTPS fetch; bearer URL is consumed and securely erased.
[[nodiscard]] InventoryFetchResult fetch_inventory_folder(
    std::wstring capability_url,std::string_view folder_id,
    std::string_view owner_id);
struct InventoryTreeFetchResult {
    bool fetched=false;
    bool limit_reached=false;
    unsigned folders_fetched=0;
    unsigned http_status=0;
    unsigned transport_error=0;
    std::vector<InventoryEntry> entries;
};
// Bounded breadth-first folder descent. The capability URL is copied per
// request and never exposed in diagnostics.
[[nodiscard]] InventoryTreeFetchResult fetch_inventory_tree(
    const std::wstring& capability_url,std::string_view root_folder_id,
    std::string_view owner_id);
struct MeshReply {
    unsigned status=0;
    bool fetched=false; // only HTTP 200 with bounded, nonempty bytes
    std::vector<std::uint8_t> bytes;
};
// Authenticated simulator CAPS GET; no redirect, no off-grid fallback.
// Runs on a separate std::async worker, never inside the DX12 frame loop.
[[nodiscard]] MeshReply fetch_mesh_asset(std::wstring capability_url,
                                         std::string mesh_id);
// JPEG2000 texture codestream fetched via authenticated ViewerAsset
// ?texture_id= UUID query, separately from mesh_id. No decode or GPU
// upload is claimed by HTTP success.
[[nodiscard]] bool jpeg2000_codestream_header(
    std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] MeshReply fetch_texture_asset(std::wstring capability_url,
                                            std::string texture_id);
[[nodiscard]] MeshReply fetch_material_asset(std::wstring capability_url,std::string material_id);
[[nodiscard]] SeedResponse request_seed(std::wstring seed_url);
[[nodiscard]] PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous={});
}

