#pragma once
#include <algorithm>
#include <cmath>

namespace kami::world::terrainquality {
// Conservative LAST-RESORT visualization guard, not an authenticity test:
// if one 1m grid cell spans over 48m of elevation, omit its triangles
// rather than creating a massive triangle wall. Real heights are neither
// edited nor guessed. This may omit legitimate extreme SL cliff cells;
// the on-screen counter makes this tradeoff observable.
inline constexpr float kMaxCellHeightSpanMeters=48.f;
[[nodiscard]] inline bool renderable_cell(
    float a,float b,float c,float d) noexcept {
    if(!std::isfinite(a)||!std::isfinite(b)||
       !std::isfinite(c)||!std::isfinite(d))return false;
    const float lo=std::min(std::min(a,b),std::min(c,d));
    const float hi=std::max(std::max(a,b),std::max(c,d));
    return hi-lo<=kMaxCellHeightSpanMeters;
}
} // namespace kami::world::terrainquality
