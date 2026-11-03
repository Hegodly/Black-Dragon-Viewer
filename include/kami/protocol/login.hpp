#pragma once
#include "kami/protocol/bootstrap.hpp"
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
    std::wstring message;
    LoginHandoff handoff{};
    std::uint32_t circuit_code=0;
    [[nodiscard]] bool accepted() const noexcept {
        return status==LoginStatus::Accepted;
    }
};
// No password, seed capability, or account identifier is written to logs.
[[nodiscard]] bool parse_account_name(std::wstring_view input,
                                      std::string& first,
                                      std::string& last) noexcept;
// Pure, offline-safe serializer for a PREHASHED credential ($1$ + md5).
[[nodiscard]] std::string build_login_xmlrpc(std::string_view first,
                                             std::string_view last,
                                             std::string_view hashed_password,
                                             std::string_view mac,
                                             bool terms_accepted);
// Parse an actual XML-RPC response with MSXML6 DTD/external entities disabled.
[[nodiscard]] LoginResult parse_login_xmlrpc(std::string_view xml);
// User-invoked real TLS POST. Password used only in memory and never
// included in output; callers must clear their own password buffer.
[[nodiscard]] LoginResult login_to_simulator(const LoginRequest& request,
                                             std::wstring_view password);
// Return safe diagnostics based ONLY on HTTP status codes. Never display
// server response bodies, redirect Location headers, passwords, or tokens.
[[nodiscard]] std::wstring login_http_diagnostic(unsigned status);
[[nodiscard]] const wchar_t* login_status_label(LoginStatus status) noexcept;
} // namespace kami::protocol
