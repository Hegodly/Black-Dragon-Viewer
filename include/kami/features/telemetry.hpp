#pragma once
#include <cstdint>
#include <d3d12.h>
namespace kami::telemetry {
struct Snapshot {
    float process_cpu_percent = 0.f;
    std::uint64_t process_ram_mb = 0;
    std::uint64_t gpu_vram_used_mb = 0;
    std::uint64_t gpu_vram_budget_mb = 0;
    bool gpu_readable = false;
};
class Monitor final {
public:
    [[nodiscard]] Snapshot sample(ID3D12Device* device) noexcept;
private:
    std::uint64_t last_wall_ = 0;
    std::uint64_t last_cpu_ = 0;
};
} // namespace kami::telemetry
