#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace kami::protocol::caps {
enum class SeedStatus : std::uint8_t {
    NotRequested, InvalidUrl, NetworkError, HttpError,
    InvalidReply, NoEventQueue, Granted
};
struct SeedResult {
    SeedStatus status=SeedStatus::NotRequested;
    unsigned http_status=0;
    unsigned granted_names=0;
    // A signed capability URL is sensitive. Never display or log it.
    std::wstring event_queue_url;
    [[nodiscard]] bool queue_granted() const noexcept {
        return status==SeedStatus::Granted;
    }
    [[nodiscard]] std::wstring diagnostic() const;
};
// Non-authenticating fixture: requests only the capabilities the core can
// currently inspect. No account identifiers or login credentials included.
[[nodiscard]] std::string seed_request_xml();
// MSXML6 LLSD response validation; call from a COM-initialized thread.
// Only authenticated simulator seed responses may be supplied at runtime.
[[nodiscard]] SeedResult parse_seed_reply(std::string_view xml);
// HTTPS POST to a server-provided, signed Second Life seed capability.
// Synchronous; invoke only from a worker, never the rendering thread.
[[nodiscard]] SeedResult request_seed_capabilities(
    std::wstring_view seed_url);
} // namespace kami::protocol::caps
