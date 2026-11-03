#pragma once
#include "kami/features/creator.hpp"
#include <cstddef>
#include <istream>
#include <string>

namespace kami::creator {
struct Preview {
    world::SceneMesh scene;
    Audit audit;
    std::size_t source_positions = 0;
    std::size_t source_polygons = 0;
    std::size_t skipped_degenerate_triangles = 0;
    float source_height = 0.f;
};

// Original Wavefront OBJ triangle/quad/polygon importer for local DX12 previews.
// Validates geometry and bounds resource consumption. Materials, rigging, LOD,
// permissions and Second Life uploads are not handled by this local importer.
[[nodiscard]] bool import_obj(std::istream& input, Preview& output,
                               std::string& error);
[[nodiscard]] bool import_obj_file(const std::wstring& path, Preview& output,
                                   std::string& error);
} // namespace kami::creator
