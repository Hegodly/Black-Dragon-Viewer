#pragma once
#include <cstdint>
#include <string>
namespace kami::navigation {
struct AvatarPosition {
    std::string resident_id;
    float x=0.f,y=0.f,z=0.f;
    bool present=false;
    bool permission_to_follow=false;
};
enum class Action : std::uint8_t {
    NotConnected, TargetUnavailable, PermissionRequired,
    RequestReady
};
class Planner final {
public:
    void set_connected(bool connected) noexcept {connected_=connected;}
    void track(AvatarPosition target) noexcept;
    [[nodiscard]] Action follow() const noexcept;
    [[nodiscard]] Action teleport_to() const noexcept;
    [[nodiscard]] bool has_target() const noexcept {return target_.present;}
private:
    bool connected_ = false;
    AvatarPosition target_{};
};
// Action::RequestReady means the network layer may REQUEST an action;
// it never implies simulator approval, completed teleport, or movement.
} // namespace kami::navigation
