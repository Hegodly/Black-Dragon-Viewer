#pragma once
#include <cstdint>
#include <string_view>

namespace kami::protocol {
enum class Grid : std::uint8_t { Agni, Aditi };
enum class ProbeStatus : std::uint8_t {
    NotChecked, TlsEndpointReached, NetworkUnavailable, TlsError, BadResponse
};
struct ProbeResult {
    ProbeStatus status=ProbeStatus::NotChecked;
    unsigned http_status=0;
    [[nodiscard]] bool secure_route_reached() const noexcept {
        return status==ProbeStatus::TlsEndpointReached;
    }
};
// A non-authenticating request, restricted to Linden Lab login hosts.
// Never transmits a password, login key, viewer identifier or account data.
[[nodiscard]] ProbeResult probe_login_tls(Grid grid) noexcept;
[[nodiscard]] std::wstring_view login_host(Grid grid) noexcept;
[[nodiscard]] bool valid_capability_url(std::wstring_view url) noexcept;
} // namespace kami::protocol
