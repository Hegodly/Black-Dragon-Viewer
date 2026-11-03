#pragma once
#include <string>
#include <string_view>
#include <cstdint>

namespace kami::protocol::caps {
// This is the *first* CAPS transport milestone. Event payloads are never
// dispatched or acknowledged until safe event handlers are implemented.
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
    // Never retain untrusted event bodies until a verified dispatcher exists.
};
struct PollResponse {
    bool structurally_valid=false;
    bool idle=false;
    bool safe_to_ack=false;
    unsigned http_status=0;
    unsigned event_count=0;
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
// Full bounded event queue metadata; bodies are NOT dispatched or acked.
[[nodiscard]] bool parse_event_queue_frame(
    std::string_view xml,EventQueueFrame& out);
[[nodiscard]] std::string build_event_queue_post(const QueueCursor& previous);
[[nodiscard]] SeedResponse request_seed(std::wstring seed_url);
[[nodiscard]] PollResponse probe_event_queue_once(
    std::wstring queue_url,const QueueCursor& previous={});
}
