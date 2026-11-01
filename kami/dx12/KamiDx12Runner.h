#pragma once
// Minimal ABI boundary for invoking the native D3D12 renderer from KAMIViewer.
// Keep the Renderer object's allocation, construction, use and destruction
// entirely in KamiDx12Renderer.cpp, not the viewer/PCH translation unit.
namespace kami::dx12 {
int runEmbeddedSmokeTest(bool warpForTesting, bool testWorldDepth) noexcept;
}
