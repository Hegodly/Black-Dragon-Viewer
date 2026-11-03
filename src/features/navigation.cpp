#include "kami/features/navigation.hpp"
#include <cmath>
#include <utility>
namespace kami::navigation {
void Planner::track(AvatarPosition target) noexcept {
    if(!std::isfinite(target.x)||!std::isfinite(target.y)||
       !std::isfinite(target.z)||target.resident_id.empty()) {
        target_={};return;
    }
    target_=std::move(target);
}
Action Planner::follow() const noexcept {
    if(!connected_)return Action::NotConnected;
    if(!target_.present)return Action::TargetUnavailable;
    if(!target_.permission_to_follow)return Action::PermissionRequired;
    return Action::RequestReady;
}
Action Planner::teleport_to() const noexcept {
    if(!connected_)return Action::NotConnected;
    if(!target_.present)return Action::TargetUnavailable;
    // Teleport permissions must be verified by the simulator later.
    if(!target_.permission_to_follow)return Action::PermissionRequired;
    return Action::RequestReady;
}
} // namespace kami::navigation
