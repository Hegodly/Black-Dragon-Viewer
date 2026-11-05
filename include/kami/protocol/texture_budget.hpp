#pragma once
#include <cstddef>
#include <cstdint>
namespace kami::protocol::texture {
// Match CPU previews to the actual GPU slice resolution. Four times as
// many decoded images fit within the existing 96 MiB pixel-data budget.
inline constexpr std::uint32_t kPreviewExtent=256U;
inline constexpr std::uint16_t kGPUImageLayers=192U;
inline constexpr std::size_t kCPUImageAssets=384U;
inline constexpr std::size_t kCPUImageBytes=96U*1024U*1024U;
inline constexpr std::size_t kMaxUploadBytes=64U*1024U*1024U;
static_assert(kCPUImageAssets*kPreviewExtent*kPreviewExtent*4U<=kCPUImageBytes);
static_assert((kGPUImageLayers+1U)*kPreviewExtent*kPreviewExtent*4U<=kMaxUploadBytes);
}
