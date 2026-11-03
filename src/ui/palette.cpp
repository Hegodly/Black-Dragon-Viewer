#include "kami/palette.hpp"
namespace kami::ui {
namespace {
D2D1_COLOR_F color(unsigned rgb,float alpha=1.f) {
    return {
        static_cast<float>((rgb >> 16)&255U)/255.f,
        static_cast<float>((rgb >> 8)&255U)/255.f,
        static_cast<float>(rgb&255U)/255.f,
        alpha
    };
}
}
Palette colors(bool light) noexcept {
    if(light) return {
        color(0xEDF3FC),color(0xFFFFFF,.94f),color(0xF7FAFF,.98f),
        color(0x12243A),color(0x526780),color(0x087BEE),color(0xDCEBFF),
        color(0xBACBE0,.7f)
    };
    return {
        color(0x0B101A),color(0x151F30,.92f),color(0x1B2840,.97f),
        color(0xEDF4FF),color(0x9AAEC7),color(0x56ADFF),color(0x1A406C),
        color(0x38506D,.7f)
    };
}
}
