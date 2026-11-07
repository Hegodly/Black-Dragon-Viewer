#include "kami/protocol/volume_params.hpp"
#include <algorithm>
#include <cmath>

namespace kami::protocol::volume {
namespace {
float cut(std::uint8_t lo,std::uint8_t hi) noexcept {
    const auto wire=static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(lo)|
        (static_cast<std::uint16_t>(hi)<<8U));
    return std::min(1.f,static_cast<float>(wire)*0.00002f);
}
}
bool decode(std::span<const std::uint8_t> b,bool full_layout,
            Parameters& out) noexcept {
    if(b.size()!=23U)return false;
    const unsigned scale_offset=full_layout?6U:5U;
    if(b[scale_offset]>200U||b[scale_offset+1U]>200U)return false;
    // Full High 12 has ProfileCurve at byte 1 and PathScaleX/Y at 6/7.
    // Packed High 13 starts the path cut fields at byte 1 and puts its
    // profile curve at byte 16. Keep these layouts explicitly separate.
    const unsigned path_begin=full_layout?2U:1U;
    const unsigned scale_x=full_layout?6U:5U;
    const unsigned profile_curve=full_layout?1U:16U;
    const unsigned profile_begin=full_layout?17U:17U;
    Parameters p{};
    p.full_layout=full_layout;
    p.path_curve=b[0];
    p.profile_curve=b[profile_curve];
    p.path_begin=cut(b[path_begin],b[path_begin+1U]);
    p.path_end=cut(b[path_begin+2U],b[path_begin+3U]);
    p.scale_x=static_cast<float>(200U-std::min<unsigned>(b[scale_x],200U))*.01f;
    p.scale_y=static_cast<float>(200U-std::min<unsigned>(b[scale_x+1U],200U))*.01f;
    const unsigned extras=full_layout?8U:7U;
    p.shear_x=b[extras];p.shear_y=b[extras+1U];
    p.twist_begin=b[extras+2U];p.twist_end=b[extras+3U];
    p.radius_offset=b[extras+4U];p.taper_x=b[extras+5U];
    p.taper_y=b[extras+6U];p.revolutions=b[extras+7U];p.skew=b[extras+8U];
    p.profile_begin=cut(b[profile_begin],b[profile_begin+1U]);
    p.profile_end=cut(b[profile_begin+2U],b[profile_begin+3U]);
    p.hollow=cut(b[profile_begin+4U],b[profile_begin+5U]);
    if(p.path_begin>p.path_end||p.profile_begin>p.profile_end||
       p.path_end-p.path_begin<.0002f||
       p.profile_end-p.profile_begin<.0002f||
       !std::isfinite(p.scale_x)||!std::isfinite(p.scale_y))
        return false;
    out=p;
    return true;
}
} // namespace kami::protocol::volume
