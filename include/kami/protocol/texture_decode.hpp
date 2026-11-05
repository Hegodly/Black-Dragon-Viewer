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
// R49 low-res preview cache: keep up to 512px on the longer axis to
// prevent large SL J2C images from saturating RAM before the DX12 atlas
// can select nearby faces. Retains aspect ratio and never invents pixels.
[[nodiscard]] bool downscale_preview(DecodedTexture& image,
                                     std::uint32_t max_side=512U) noexcept;
} // namespace kami::protocol::texture
