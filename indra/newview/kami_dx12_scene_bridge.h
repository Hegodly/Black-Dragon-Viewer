#pragma once

// First bridge from actual viewer-resident geometry to the native D3D12
// WorldFrame. This is an opt-in capture API, not a replacement SL render loop.
#if !defined(_WIN32)
#error "KAMI D3D12 scene bridge supports Windows only"
#endif

#include "KamiDx12Renderer.h"

#include <cstddef>

class LLViewerCamera;

namespace kami::dx12 {

struct SceneCaptureStats
{
    std::size_t visitedObjects = 0;
    std::size_t visibleVolumes = 0;
    std::size_t submittedMeshes = 0;
    std::size_t submittedVertices = 0;
    std::size_t submittedIndices = 0;
    std::size_t skippedRiggedFaces = 0;
    std::size_t skippedTranslucentFaces = 0;
    bool hitBudget = false;
};

// Read-only snapshot of currently decoded, visible, non-rigged LLVolumeFace
// geometry from the main viewer thread. UVs/PBR/alpha/avatars/terrain/skin are
// NOT yet supported. No GL calls or graphics-state mutations occur here.
// The returned world frame is consumable by Renderer::renderWorldFrame(...).
WorldFrame captureLiveVolumeWorld(const LLViewerCamera& camera,
                                 SceneCaptureStats* stats = nullptr);

} // namespace kami::dx12
