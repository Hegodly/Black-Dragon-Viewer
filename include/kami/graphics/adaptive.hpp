#pragma once
#include "kami/graphics/settings.hpp"

namespace kami::graphics {
// Uses measured application frame throughput (not GPU timestamps).
// Never adjusts quality while capped below the requested target.
class AdaptiveQuality final {
public:
    void reset() noexcept;
    [[nodiscard]] bool sample(Settings& value, float measured_fps,
                              float interval_seconds) noexcept;
private:
    unsigned struggling_ = 0;
    unsigned comfortable_ = 0;
    float cooldown_seconds_ = 0.f;
};
} // namespace kami::graphics
