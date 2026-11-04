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
    unsigned http_status=0;
    std::wstring event_queue_url; // secret bearer capability; do not log
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
// These parse only the bounded LLSD XML envelope, never execute events.
[[nodiscard]] bool parse_seed_llsd(std::string_view xml,
                                   std::wstring& event_queue_url);
[[nodiscard]] bool parse_event_queue_llsd(std::string_view xml,
                                          unsigned& event_count);
// Full bounded event-queue envelope; bodies are not applied. Successful
// observation is acknowledged so the next long-poll can proceed.
[[nodiscard]] bool parse_event_queue_frame(
    std::string_view xml,EventQueueFrame& out);
[[nodiscard]] std::string build_event_queue_post(const QueueCursor& previous);
[[nodiscard]] SeedResponse request_seed(std::wstring seed_url);
[[nodiscard]] PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous={});
}
