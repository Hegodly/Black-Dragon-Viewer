#pragma once
#include <cstddef>

namespace kami::creator {
// Triangle-list geometry expands every face to three GPU vertices.
// Preview and importer must enforce identical limits; accepting data here
// that renderer later rejects produces misleading 'DX12 allocation' errors.
inline constexpr std::size_t kMaxPreviewTriangles = 333333U;
inline constexpr std::size_t kMaxPreviewVertices = kMaxPreviewTriangles * 3U;
inline constexpr std::size_t kMaxPreviewFileBytes = 128U * 1024U * 1024U;
} // namespace kami::creator
