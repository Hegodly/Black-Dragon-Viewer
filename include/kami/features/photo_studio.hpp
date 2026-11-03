#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace kami::photo {
enum class Preset : std::uint32_t { Cinematic, Natural, Noir, NightStudio };
struct Settings {
    Preset preset=Preset::Cinematic;
    int exposure_tenths=-2;
    unsigned contrast_percent=118;
    unsigned saturation_percent=96;
    unsigned vignette_percent=22;
    bool enabled=true;
};
[[nodiscard]] Settings normalize(Settings value) noexcept;
[[nodiscard]] Settings for_preset(Preset preset) noexcept;
[[nodiscard]] const wchar_t* preset_name(Preset preset) noexcept;
[[nodiscard]] Settings load();
void save(Settings value) noexcept;

// Saves a genuine DX12 readback into a portable PNG via Windows Imaging
// Component. Pixels are top-down, packed BGRA8, tightly laid out. No GShade,
// MXAO, OpenGL, or external screenshot injector.
[[nodiscard]] bool write_bgra_png(const std::wstring& path,
                                  const std::vector<std::uint8_t>& pixels,
                                  unsigned width,unsigned height) noexcept;
} // namespace kami::photo
