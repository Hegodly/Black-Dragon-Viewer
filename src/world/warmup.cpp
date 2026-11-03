#include "kami/world/warmup.hpp"
#include "kami/world/device.hpp"
#include "kami/world/shader_cache.hpp"
#include <algorithm>

namespace kami::world {
void ShaderWarmup::begin() {
    WarmupResult expected = WarmupResult::Idle;
    if(!result_.compare_exchange_strong(expected,WarmupResult::Optimizing))
        return;
    worker_=std::jthread([this] {
        Device device;
        // WARP should not spend CPU cycles compiling shaders on the login
        // screen. If no hardware DX12 adapter is usable, show unavailable.
        if(device.initialize(false) != DeviceStatus::ReadyHardware) {
            result_.store(WarmupResult::NoHardware);
            return;
        }
        Microsoft::WRL::ComPtr<ID3D12RootSignature> signature;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline;
        ShaderPreparation report;
        const bool ok=ShaderCache::prepare(
            device.native_device(),signature,pipeline,report,
            [this](unsigned percent, const wchar_t*) {
                percent_.store(std::min(percent,100U));
            });
        shader_hit_.store(report.shader_cache_hit);
        pipeline_hit_.store(report.pipeline_cache_hit);
        cache_written_.store(report.written_to_disk);
        percent_.store(100);
        result_.store(!ok?WarmupResult::Failed:
                      report.pipeline_cache_hit?WarmupResult::CacheReady:
                                                WarmupResult::Optimized);
    });
}
WarmupSnapshot ShaderWarmup::snapshot() const noexcept {
    return {
        result_.load(),percent_.load(),shader_hit_.load(),
        pipeline_hit_.load(),cache_written_.load()
    };
}
} // namespace kami::world
