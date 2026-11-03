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
struct PollResponse {
    bool structurally_valid=false;
    bool idle=false;
    unsigned http_status=0;
    unsigned event_count=0;
    std::wstring diagnostic;      // contains no events or signed URLs
};
// Pure URL checks, useful for offline tests. Only Linden-owned HTTPS
// capability hosts are accepted; redirects are prohibited.
[[nodiscard]] bool trusted_caps_url(std::wstring_view url) noexcept;
// These parse only the bounded LLSD XML envelope, never execute events.
[[nodiscard]] bool parse_seed_llsd(std::string_view xml,
                                   std::wstring& event_queue_url);
[[nodiscard]] bool parse_event_queue_llsd(std::string_view xml,
                                          unsigned& event_count);
[[nodiscard]] SeedResponse request_seed(std::wstring seed_url);
[[nodiscard]] PollResponse probe_event_queue_once(std::wstring queue_url);
}
