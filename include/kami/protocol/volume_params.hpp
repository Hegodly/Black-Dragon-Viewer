#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace kami::protocol::volume {

// Decoded, bounded subset of Linden Lab LLVolumeParams. The raw bytes remain
// authoritative; normalized values use the quantization in llvolume.h.
struct Parameters {
    std::uint8_t path_curve=0;
    std::uint8_t profile_curve=0;
    float path_begin=0.f, path_end=1.f;
    float profile_begin=0.f, profile_end=1.f, hollow=0.f;
    float scale_x=1.f, scale_y=1.f;
    std::uint8_t shear_x=0, shear_y=0;
    std::uint8_t twist_begin=0, twist_end=0;
    std::uint8_t radius_offset=0, taper_x=0, taper_y=0;
    std::uint8_t revolutions=0, skew=0;
    bool full_layout=false;
};

// High 12 ObjectUpdate and High 13 ObjectUpdateCompressed carry the same
// logical 23 LLVolumeParams bytes in different layouts. This function only
// decodes fields; it does not claim unsupported path/profile curves render.
[[nodiscard]] bool decode(std::span<const std::uint8_t> bytes,
                          bool full_layout, Parameters& out) noexcept;

} // namespace kami::protocol::volume
