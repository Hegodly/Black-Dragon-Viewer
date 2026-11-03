#pragma once
#include "kami/protocol/bootstrap.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace kami::protocol {
enum class LoginStatus : std::uint8_t {
    NotAttempted, InvalidInput, CryptoUnavailable, NetworkError,
    TlsFailure, HttpError, MalformedReply, Rejected,
    RequiresUserAction, Accepted
};
struct LoginRequest {
    Grid grid=Grid::Agni;
    std::wstring account; // "First Last" or "Firstname" (Resident)
    std::wstring start=L"last";
    bool terms_accepted=false; // user-confirmed, never auto-checked
};
struct LoginResult {
    LoginStatus status=LoginStatus::NotAttempted;
    unsigned http_status=0;
    std::wstring http_response_kind; // safe response format only, not raw text
    std::wstring message;
    LoginHandoff handoff{};
    std::uint32_t circuit_code=0;
    [[nodiscard]] bool accepted() const noexcept {
        return status==LoginStatus::Accepted;
    }
};
// Hash raw adapter bytes (not a printable colon-delimited MAC).
// This pure helper supports deterministic offline protocol tests.
[[nodiscard]] std::string hash_mac_bytes(std::array<std::uint8_t,6> raw);
// Only a category is returned: never expose untrusted HTTP error bodies.
[[nodiscard]] std::wstring_view classify_login_http_body(
    std::string_view body) noexcept;
[[nodiscard]] std::wstring describe_login_http_response(
    bool read_ok,std::string_view sample,unsigned read_error);
[[nodiscard]] std::wstring_view classify_login_http_content_type(
    std::wstring_view media_type) noexcept;
// No password, seed capability, or account identifier is written to logs.
[[nodiscard]] bool parse_account_name(std::wstring_view input,
                                      std::string& first,
                                      std::string& last) noexcept;
// Pure, offline-safe serializer for a PREHASHED credential ($1$ + md5).
[[nodiscard]] std::string build_login_xmlrpc(std::string_view first,
                                             std::string_view last,
                                             std::string_view hashed_password,
                                             std::string_view mac,
                                             std::string_view volume_id0,
                                             bool terms_accepted);
// Parse an actual XML-RPC response with MSXML6 DTD/external entities disabled.
[[nodiscard]] LoginResult parse_login_xmlrpc(std::string_view xml);
// User-invoked real TLS POST. Password used only in memory and never
// included in output; callers must clear their own password buffer.
[[nodiscard]] LoginResult login_to_simulator(const LoginRequest& request,
                                             std::wstring_view password);
// Return safe diagnostics based on HTTP status only; optional non-secret
// response type classification is appended by login_to_simulator().
 // Never display server response bodies or sensitive response headers.
[[nodiscard]] std::wstring login_http_diagnostic(unsigned status);
[[nodiscard]] const wchar_t* login_status_label(LoginStatus status) noexcept;
} // namespace kami::protocol
