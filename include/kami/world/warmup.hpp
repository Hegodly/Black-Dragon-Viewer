#pragma once
#include <atomic>
#include <thread>

namespace kami::world {

enum class WarmupResult {
    Idle,
    Optimizing,
    CacheReady,
    Optimized,
    NoHardware,
    Failed
};

struct WarmupSnapshot {
    WarmupResult result;
    unsigned percent;
    bool shader_cache_hit;
    bool pipeline_cache_hit;
    bool cache_written;
};

// Runs the original DX12 shader/PSO preparation once per app launch
// without blocking the native login/landing screen. Disk cache persists
// across launches and is keyed to graphics hardware + shader source.
class ShaderWarmup final {
public:
    ShaderWarmup() = default;
    ShaderWarmup(const ShaderWarmup&) = delete;
    ShaderWarmup& operator=(const ShaderWarmup&) = delete;
    ~ShaderWarmup() = default;

    void begin();
    [[nodiscard]] WarmupSnapshot snapshot() const noexcept;
private:
    std::atomic<WarmupResult> result_{WarmupResult::Idle};
    std::atomic<unsigned> percent_{0};
    std::atomic<bool> shader_hit_{false};
    std::atomic<bool> pipeline_hit_{false};
    std::atomic<bool> cache_written_{false};
    // Destroyed after state. jthread joins before atomics are destroyed.
    std::jthread worker_;
};
} // namespace kami::world
