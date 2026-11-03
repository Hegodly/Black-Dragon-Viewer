#include "kami/features/telemetry.hpp"
#include <windows.h>
#include <dxgi1_6.h>
#include <psapi.h>
#include <wrl/client.h>
#include <algorithm>

namespace kami::telemetry {
namespace {
std::uint64_t ticks(FILETIME stamp) noexcept {
    ULARGE_INTEGER value{};
    value.HighPart=stamp.dwHighDateTime;
    value.LowPart=stamp.dwLowDateTime;
    return value.QuadPart;
}
} // namespace
Snapshot Monitor::sample(ID3D12Device* device) noexcept {
    Snapshot value{};
    const HANDLE process=GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS counters{};
    if(GetProcessMemoryInfo(process,&counters,sizeof(counters)))
        value.process_ram_mb=counters.WorkingSetSize/(1024U*1024U);

    FILETIME now{},created{},exited{},kernel{},user{};
    GetSystemTimeAsFileTime(&now);
    if(GetProcessTimes(process,&created,&exited,&kernel,&user)) {
        const auto current_wall=ticks(now);
        const auto current_cpu=ticks(kernel)+ticks(user);
        if(last_wall_!=0 && current_wall>last_wall_ && current_cpu>=last_cpu_) {
            const double elapsed=static_cast<double>(current_wall-last_wall_);
            const double busy=static_cast<double>(current_cpu-last_cpu_);
            const DWORD cores=std::max<DWORD>(1,GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
            value.process_cpu_percent=static_cast<float>(
                std::clamp(100.*busy/(elapsed*cores),0.,100.));
        }
        last_wall_=current_wall;
        last_cpu_=current_cpu;
    }
    if(device) {
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
        if(SUCCEEDED(CreateDXGIFactory2(0,IID_PPV_ARGS(factory.GetAddressOf())))) {
            Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
            if(SUCCEEDED(factory->EnumAdapterByLuid(
                    device->GetAdapterLuid(),IID_PPV_ARGS(adapter.GetAddressOf())))) {
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if(SUCCEEDED(adapter->QueryVideoMemoryInfo(
                    0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info))) {
                    value.gpu_vram_used_mb=info.CurrentUsage/(1024U*1024U);
                    value.gpu_vram_budget_mb=info.Budget/(1024U*1024U);
                    value.gpu_readable=true;
                }
            }
        }
    }
    return value;
}
} // namespace kami::telemetry
