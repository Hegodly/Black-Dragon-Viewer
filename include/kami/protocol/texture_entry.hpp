#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace kami::protocol::texture {
struct Base {
    std::array<std::uint8_t,16U> image_id{};
    // Sparse original SL TE face-image exceptions (0..44).
    std::array<std::array<std::uint8_t,16U>,45U> face_image_ids{};
    std::array<bool,45U> face_image_override{};
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
// SL network UUID ordering, canonical lowercase format. All-zero UUID
// denotes no image. The transport must not fetch zero or malformed IDs.
[[nodiscard]] std::string default_image_uuid(const Base& entry);
[[nodiscard]] std::string face_image_uuid(const Base& entry,unsigned face);
} // namespace kami::protocol::texture
