#include "kami/world/device.hpp"
#include "kami/world/dx12_mesh_buffer_cache.hpp"
#include <array>
#include <cstdint>

int main() {
    kami::world::Device device;
    if (device.status() != kami::world::DeviceStatus::NotInitialized) return 1;
    if (device.native_device() != nullptr) return 2;
    if (device.graphics_queue() != nullptr) return 3;

    // GitHub CI may lack a hardware GPU. This tests safe initialization
    // without claiming that a rendered Second Life world exists.
    const auto status = device.initialize(true);
    if (device.is_ready()) {
        if (status != kami::world::DeviceStatus::ReadyHardware &&
            status != kami::world::DeviceStatus::ReadySoftware) return 4;
        if (!device.native_device() || !device.graphics_queue()) return 5;
        if (device.adapter_name().empty()) return 6;
        // Validate persistent per-instance DX12 buffers on hardware or WARP.
        struct Vertex { float x,y,z; std::uint32_t material; };
        const std::array<Vertex,6> scene{{{0,0,0,1},{1,0,0,1},
            {0,1,0,1},{2,0,0,2},{3,0,0,2},{2,1,0,2}}};
        kami::world::Dx12MeshBufferCache cache;
        cache.begin_generation();
        if(!cache.upload_instance(device.native_device(),"region",17,1,
                                  scene.data(),scene.size(),3,3))return 9;
        const auto* resident=cache.find("region:17");
        if(!resident||!resident->resource||
           resident->view.SizeInBytes!=3*sizeof(Vertex)||
           resident->view.StrideInBytes!=sizeof(Vertex))return 10;
        const auto before=resident->resource.Get();
        if(!cache.upload_instance(device.native_device(),"region",17,1,
                                  scene.data(),scene.size(),3,3))return 11;
        if(cache.find("region:17")->resource.Get()!=before)return 12;
        if(cache.upload_instance(device.native_device(),"region",18,1,
                                 scene.data(),scene.size(),5,2))return 13;
        if(cache.upload_instance(device.native_device(),"region",0,1,
                                 scene.data(),scene.size(),0,3))return 14;
        if(cache.record_draw(nullptr,"region:17",3))return 15;
        if(cache.size()!=1)return 16;
        // Per-instance draw API must fail safely without a command list,
        // including missing IDs and absent region identities.
        if(cache.record_instance_draw(nullptr,"region",17))return 17;
        if(cache.record_instance_draw(nullptr,"region",18))return 18;
        if(cache.record_instance_draw(nullptr,"",17))return 19;
        if(cache.record_instance_draw(nullptr,"region",0))return 20;
        if(cache.record_draw(nullptr,"region:17",4))return 21;
        if(cache.record_draw(nullptr,"region:17",3,1))return 22;
        if(cache.record_draw(nullptr,"region:17",0))return 23;
        if(cache.size()!=1)return 24;
    } else {
        if (status != kami::world::DeviceStatus::Unavailable) return 7;
        if (device.native_device() || device.graphics_queue()) return 8;
    }
    return 0;
}
