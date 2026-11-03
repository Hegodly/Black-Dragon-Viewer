#pragma once
#include <string_view>
namespace kami::protocol {
enum class SessionState { Offline, Connecting, Online };
enum class ConnectionResult { NotImplemented };
class Session final {
public:
    [[nodiscard]] SessionState state() const noexcept { return SessionState::Offline; }
    [[nodiscard]] ConnectionResult connect(std::wstring_view, std::wstring_view) const noexcept {
        return ConnectionResult::NotImplemented;
    }
    [[nodiscard]] bool can_enter_world() const noexcept { return false; }
};
}
