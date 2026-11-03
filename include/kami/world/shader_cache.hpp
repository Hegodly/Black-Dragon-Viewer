#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <functional>
#include <string>

namespace kami::world {
struct ShaderPreparation {
    bool shader_cache_hit = false;
    bool pipeline_cache_hit = false;
    bool written_to_disk = false;
    bool driver_cache_invalidated = false;
    std::wstring gpu_name;
    std::wstring error;
};

// A small, truthful COD-like warmup: optimize currently implemented shaders,
// persist GPU-specific DX12 PSO data, and reuse it on subsequent launches.
// Future renderer passes register their own PSOs here as they are implemented.
class ShaderCache final {
public:
    using Progress = std::function<void(unsigned, const wchar_t*)>;
    [[nodiscard]] static bool prepare(
        ID3D12Device* device,
        Microsoft::WRL::ComPtr<ID3D12RootSignature>& signature,
        Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline,
        ShaderPreparation& report,
        const Progress& progress = {});
    [[nodiscard]] static std::uint64_t source_version() noexcept;
};
} // namespace kami::world
