#pragma once

// Interactive preview of the actual, decoded Second Life scene rendered into
// a separate native D3D12 swap chain (not an OpenGL compatibility layer).
// This is NOT a replacement for LLRender or the main viewer window.
#if !defined(_WIN32)
#error "KAMI D3D12 preview is Windows-only"
#endif
class LLViewerCamera;
void kamiDX12LivePreviewTick(const LLViewerCamera& camera);
