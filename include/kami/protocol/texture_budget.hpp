#pragma once
#include <cstddef>
#include <cstdint>
namespace kami::protocol::texture {
// Bounded resident images shared by terrain and all five glTF map slots.
inline constexpr std::uint32_t kPreviewExtent=256U;
inline constexpr std::uint16_t kGPUImageLayers=512U;
inline constexpr std::size_t kCPUImageAssets=1024U;
inline constexpr std::size_t kCPUImageBytes=256U*1024U*1024U;
inline constexpr std::size_t kMaxUploadBytes=256U*1024U*1024U;
static_assert(kCPUImageAssets*kPreviewExtent*kPreviewExtent*4U<=kCPUImageBytes);
static_assert((kGPUImageLayers+1U)*kPreviewExtent*kPreviewExtent*4U<=kMaxUploadBytes);
}

