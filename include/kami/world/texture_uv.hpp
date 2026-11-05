#pragma once
#include "kami/protocol/texture_entry.hpp"
#include <array>
#include <cmath>
namespace kami::world {
// SL legacy diffuse TE UV convention: rotation about the center, then
// scale/repeat and offset. The GPU's decoded JPEG2000 V coordinate points
// down, so perform the SL calculation in V-up space and flip it back.
[[nodiscard]] inline std::array<float,2> transform_diffuse_uv(
    float u,float v,const protocol::texture::UVTransform& t) noexcept {
    if(!std::isfinite(u)||!std::isfinite(v)||
       !std::isfinite(t.repeat_u)||!std::isfinite(t.repeat_v)||
       !std::isfinite(t.offset_u)||!std::isfinite(t.offset_v)||
       !std::isfinite(t.rotation))return {0.f,0.f};
    const float x=u-.5f,y=.5f-v;
    const float c=std::cos(t.rotation),s=std::sin(t.rotation);
    const float sx=x*c-y*s,sy=x*s+y*c;
    return {sx*t.repeat_u+t.offset_u+.5f,
            .5f-(sy*t.repeat_v+t.offset_v)};
}
} // namespace kami::world
