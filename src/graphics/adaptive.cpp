#include "kami/graphics/adaptive.hpp"
#include <algorithm>
#include <cmath>

namespace kami::graphics {
void AdaptiveQuality::reset() noexcept {
    struggling_=0;
    comfortable_=0;
    cooldown_seconds_=0.f;
}
bool AdaptiveQuality::sample(Settings& value,float fps,float dt) noexcept {
    value=normalize(value);
    if(!value.auto_quality || !std::isfinite(fps) || fps<=0 ||
       !std::isfinite(dt) || dt<=0) {
        reset();
        return false;
    }
    if(value.fps_limit!=0 && value.fps_limit<value.target_fps) {
        reset();  // A software FPS cap cannot be overcome by lowering render scale.
        return false;
    }
    if(cooldown_seconds_>0.f) {
        cooldown_seconds_=std::max(0.f,cooldown_seconds_-dt);
        return false;
    }
    const float target=static_cast<float>(value.target_fps);
    struggling_=fps<target*.84f ? struggling_+1:0;
    comfortable_=fps>target*1.18f ? comfortable_+1:0;
    // Downshift quickly; upshift conservatively to avoid oscillation.
    if(struggling_>=3) {
        struggling_=0;comfortable_=0;cooldown_seconds_=8.f;
        const unsigned next=value.render_scale_percent==100?75:50;
        if(next==value.render_scale_percent) return false;
        value.render_scale_percent=next;
        return true;
    }
    if(comfortable_>=6) {
        struggling_=0;comfortable_=0;cooldown_seconds_=12.f;
        const unsigned next=value.render_scale_percent==50?75:100;
        if(next==value.render_scale_percent) return false;
        value.render_scale_percent=next;
        return true;
    }
    return false;
}
} // namespace kami::graphics
