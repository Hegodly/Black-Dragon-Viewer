#pragma once
#include <cstdint>
#include <span>
#include <vector>
namespace kami::protocol::texture {
// Actual decoded JPEG 2000 texture image from an authenticated
// ViewerAsset transport response. RGBA8 only, no invented image pixels.
// GPU upload / per-face material binding is a separate integration step.
struct DecodedTexture {
    std::uint32_t width=0U,height=0U;
    std::vector<std::uint8_t> rgba;
};
[[nodiscard]] bool decode_jpeg2000(
    std::span<const std::uint8_t> j2c,DecodedTexture& output) noexcept;
} // namespace kami::protocol::texture
