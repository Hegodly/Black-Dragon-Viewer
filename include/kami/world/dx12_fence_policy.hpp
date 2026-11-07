#pragma once

namespace kami::world::detail {

// A resource mutation is safe only when no queue can still reference the
// resource, or when the caller has completed a fence ordered after that work.
// Keep this policy pure so failure and no-queue behavior can be regression tested.
template<class Wait>
[[nodiscard]] inline bool resource_mutation_fence_ready(
    bool queue_available,Wait&& wait) noexcept(noexcept(wait())) {
    return !queue_available||static_cast<bool>(wait());
}

} // namespace kami::world::detail
