#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>

namespace kami::protocol::caps {
// CAPS transport observation stage. Validated LLSD event envelopes are
// admitted into bounded metadata telemetry and ACKed to keep polling alive.
// Body-dependent simulator actions are deliberately NOT implemented yet.
// Don't present an observed event as a fully handled SL viewer action.
struct SeedResponse {
    bool granted=false;
    bool event_queue_granted=false;
    bool mesh_granted=false;
    unsigned http_status=0;
    std::wstring event_queue_url; // secret bearer capability; do not log
    std::wstring mesh_capability_url; // ViewerAsset or GetMesh, never logged
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
    // Only sanitized type names are kept. Event bodies are not applied.
};
struct PollResponse {
    bool structurally_valid=false;
    bool idle=false;
    bool transient=false; // retry bounded transient HTTP/WinHTTP faults
    bool safe_to_ack=false; // UI must commit observation before next ACK
    unsigned http_status=0;
    unsigned event_count=0;
    unsigned unsupported_events=0; // explicitly observed but NOT applied
    std::vector<std::string> event_types; // no sensitive LLSD bodies
    QueueCursor next_ack; // only valid when safe_to_ack=true
    std::wstring diagnostic; // no event bodies, credentials or bearer URLs
};
// Pure URL checks, useful for offline tests. Only Linden-owned HTTPS
// capability hosts are accepted; redirects are prohibited.
[[nodiscard]] bool trusted_caps_url(std::wstring_view url) noexcept;
// Do not expose bearer URLs in diagnostics. A server may grant one CAPS
// service without the other. Reject malformed/unsafe URLs individually.
struct SeedParseReport {
    bool xml_map=false;
    bool event_named=false,mesh_named=false;
    bool event_rejected=false,mesh_rejected=false;
};
// These parse only the bounded LLSD XML envelope, never execute events.
// Returns true when at least one supported, trusted capability is present.
[[nodiscard]] bool parse_seed_llsd(std::string_view xml,
                                   std::wstring& event_queue_url,
    std::wstring* mesh_capability_url=nullptr,
    SeedParseReport* report=nullptr);
[[nodiscard]] bool parse_event_queue_llsd(std::string_view xml,
                                          unsigned& event_count);
// Full bounded event-queue envelope; bodies are not applied. Successful
// observation is acknowledged so the next long-poll can proceed.
[[nodiscard]] bool parse_event_queue_frame(
    std::string_view xml,EventQueueFrame& out);
[[nodiscard]] std::string build_event_queue_post(const QueueCursor& previous);
// Deterministic, offline-testable version of the live HTTP envelope observer.
// Observation can advance a typed cursor but never implies event actions ran.
[[nodiscard]] PollResponse observe_event_queue_response(std::string_view xml);
// A malformed WinHTTP response (12152) may be a long-poll edge response,
// but is not proof of a healthy event queue. Retry without advancing ACK.
[[nodiscard]] bool retryable_event_queue_http_error(unsigned code) noexcept;
[[nodiscard]] bool possible_idle_event_queue_http_error(unsigned code,
                                                       bool held_long) noexcept;
struct MeshReply {
    unsigned status=0;
    bool fetched=false; // only HTTP 200 with bounded, nonempty bytes
    std::vector<std::uint8_t> bytes;
};
// Authenticated simulator CAPS GET; no redirect, no off-grid fallback.
// Runs on a separate std::async worker, never inside the DX12 frame loop.
[[nodiscard]] MeshReply fetch_mesh_asset(std::wstring capability_url,
                                         std::string mesh_id);
[[nodiscard]] SeedResponse request_seed(std::wstring seed_url);
[[nodiscard]] PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous={});
}
