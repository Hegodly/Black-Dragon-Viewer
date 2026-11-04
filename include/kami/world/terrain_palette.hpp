#pragma once
#include <algorithm>
#include <cmath>

namespace kami::world::terraincolor {
struct RGB { float r=0.f,g=0.f,b=0.f; };
[[nodiscard]] inline float clamp01(float v) noexcept {
    return std::clamp(v,0.f,1.f);
}
[[nodiscard]] inline float smooth(float low,float high,float x) noexcept {
    const float t=clamp01((x-low)/(high-low));
    return t*t*(3.f-2.f*t);
}
[[nodiscard]] inline RGB mix(RGB a,RGB b,float t) noexcept {
    t=clamp01(t);
    return {a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};
}
// Procedural visual-only biome. Heights and surface normals originate
// from verified Second Life terrain; these RGB colors DO NOT represent
// the region's texture IDs or the actual SL terrain material weights.
[[nodiscard]] inline RGB sample(float world_x,float world_y,float world_z,
                                float normal_up,float water_z) noexcept {
    if(!std::isfinite(world_x)||!std::isfinite(world_y)||
       !std::isfinite(world_z)||!std::isfinite(normal_up))return {};
    const float slope=clamp01(1.f-std::fabs(normal_up));
    // Continuous deterministic small-scale color variation - stable
    // across patches because coordinates are SL-region-absolute.
    const float micro=std::sin(world_x*.36f)*std::cos(world_z*.39f);
    const float broad=std::sin(world_x*.079f+world_z*.043f)*
                      std::cos(world_z*.062f-world_x*.039f);
    const float variance=(micro*.035f+broad*.07f);
    RGB grass{.19f+variance,.43f+variance*.8f,.21f+variance*.5f};
    const RGB drygrass{.31f,.40f,.21f};
    grass=mix(grass,drygrass,smooth(38.f,95.f,world_y)*.55f);
    const RGB sand{.60f,.51f,.36f};
    const RGB soil{.39f,.31f,.25f};
    const RGB rock{.40f,.43f,.48f};
    RGB result=grass;
    if(std::isfinite(water_z)){
        const float shore=1.f-smooth(0.3f,5.f,std::fabs(world_y-water_z));
        result=mix(result,sand,shore*.88f);
    }
    result=mix(result,soil,smooth(.12f,.34f,slope)*.40f);
    result=mix(result,rock,smooth(.26f,.68f,slope)*.93f);
    return {clamp01(result.r),clamp01(result.g),clamp01(result.b)};
}
} // namespace kami::world::terraincolor
