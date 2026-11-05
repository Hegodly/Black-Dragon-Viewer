#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace kami::protocol::texture {
struct Base {
    std::array<std::uint8_t,16U> image_id{};
    float red=1.f,green=1.f,blue=1.f,alpha=1.f;
};
// Decode the default image ID and default RGBA tint in an SL TextureEntry.
// Face-specific overrides and PBR material textures are intentionally not
// substituted for the base tint; callers must not claim textures are loaded.
// See secondlife/viewer indra/llprimitive/llprimitive.cpp packTEField:
// image UUID default + mask/UUID exceptions + zero sentinel,
// then inverted RGBA default + subsequent per-face color exceptions.
[[nodiscard]] bool decode_base(std::span<const std::uint8_t> bytes,
                               Base& out) noexcept;
} // namespace kami::protocol::texture
