#include "kami/world/device.hpp"
#include "kami/world/dx12_mesh_buffer_cache.hpp"
#include <array>
#include <cstdint>

int main() {
    kami::world::Device device;
    if (device.status() != kami::world::DeviceStatus::NotInitialized) return 1;
    if (device.native_device() != nullptr) return 2;
    if (device.graphics_queue() != nullptr) return 3;

    // Source-span parity checks are CPU-only and must run even without WARP.
    using Span=kami::world::Dx12MeshBufferCache::InstanceSourceSpan;
    using Cache=kami::world::Dx12MeshBufferCache;
    if(!Cache::validate_instance_source_spans(12,{Span{1,0,3},Span{2,3,6}}))return 45;
    if(Cache::validate_instance_source_spans(12,{Span{1,0,6},Span{2,3,3}}))return 46;
    if(Cache::validate_instance_source_spans(12,{Span{1,0,3},Span{1,3,3}}))return 47;
    if(Cache::validate_instance_source_spans(12,{Span{1,9,6}}))return 48;
    if(Cache::validate_instance_source_spans(12,{Span{1,1,3}}))return 49;
    if(Cache::validate_instance_source_spans(12,{Span{1,0,4}}))return 50;
    if(Cache::validate_instance_source_spans(12,{Span{0,0,3}}))return 51;
    if(Cache::validate_instance_source_spans(12,{}))return 52;
    if(!Cache::validate_instance_source_spans(12,{Span{1,9,3},Span{2,0,3}}))return 53;

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
        // Batch preflight must never accept a partially resident scene.
        if(!cache.can_draw_instance_batch("region",{17}))return 25;
        if(cache.can_draw_instance_batch("region",{17,18}))return 26;
        if(cache.can_draw_instance_batch("region",{17,17}))return 27;
        if(cache.can_draw_instance_batch("region",{0}))return 28;
        if(cache.can_draw_instance_batch("",{17}))return 29;
        if(cache.can_draw_instance_batch("region",{}))return 30;
        if(!cache.upload_instance(device.native_device(),"region",18,1,
                                  scene.data(),scene.size(),0,3))return 31;
        if(!cache.can_draw_instance_batch("region",{17,18}))return 32;
        using Requirement=kami::world::Dx12MeshBufferCache::InstanceDrawRequirement;
        if(!cache.can_draw_exact_instance_batch("region",1,
                                                {Requirement{17,3},Requirement{18,3}}))return 33;
        if(cache.can_draw_exact_instance_batch("region",2,{Requirement{17,3}}))return 34;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,4}}))return 35;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3},Requirement{17,3}}))return 36;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3},Requirement{19,3}}))return 37;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,0}}))return 38;
        if(cache.can_draw_exact_instance_batch("",1,{Requirement{17,3}}))return 39;
        if(cache.can_draw_exact_instance_batch("region",1,{}))return 40;
        // Coverage is stricter than nonoverlap: a missing triangle must
        // never disappear when the per-object draw path is activated.
        using Span=kami::world::Dx12MeshBufferCache::InstanceSourceSpan;
        if(!kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                6,{Span{17,0,3},Span{18,3,3}}))return 45;
        if(!kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                6,{Span{18,3,3},Span{17,0,3}}))return 46;
        if(kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                9,{Span{17,0,3},Span{18,6,3}}))return 47;
        if(kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                9,{Span{17,0,3},Span{18,3,3}}))return 48;
        if(kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                6,{Span{17,0,3},Span{18,0,3}}))return 49;
        if(kami::world::Dx12MeshBufferCache::covers_complete_mesh_scene(
                6,{}))return 50;
        // Material parity must preserve opaque/shadow and alpha-blend paths.
        using Cache=kami::world::Dx12MeshBufferCache;
        if(!Cache::opaque_material_parity({1,1,1,0,0,0},{false,false}))return 51;
        if(Cache::opaque_material_parity({1,1,1},{false,true}))return 52;
        if(Cache::opaque_material_parity({1,1,0},{false,false}))return 53;
        if(Cache::opaque_material_parity({2,2,2},{false,false}))return 54;
        if(Cache::opaque_material_parity({1,1},{false,false}))return 55;
        if(Cache::opaque_material_parity({},{false,false}))return 56;
        // Conservative frustum checks: never discard missing or corrupt
        // simulator bounds, and reject only definitely outside spheres.
        using Cache=kami::world::Dx12MeshBufferCache;
        const std::vector<Cache::VisibilityPlane> cube{
            {1,0,0,1},{-1,0,0,1},{0,1,0,1},
            {0,-1,0,1},{0,0,1,1},{0,0,-1,1}};
        if(!Cache::potentially_visible({0,0,0,.5f,true},cube))return 51;
        if(Cache::potentially_visible({3,0,0,.5f,true},cube))return 52;
        if(!Cache::potentially_visible({1.5f,0,0,.5f,true},cube))return 53;
        if(!Cache::potentially_visible({3,0,0,.5f,false},cube))return 54;
        if(!Cache::potentially_visible({3,0,0,.5f,true},{}))return 55;
        if(!Cache::potentially_visible({3,0,0,-1.f,true},cube))return 56;
        // A region generation change must invalidate old mesh residency until
        // the new scene explicitly reuploads or touches its visible assets.
        cache.begin_generation();
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3}}))return 41;
        cache.touch("region:17");
        if(!cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3}}))return 42;
        if(cache.can_draw_exact_instance_batch("region",1,
                                                {Requirement{17,3},Requirement{18,3}}))return 43;
        cache.touch("region:18");
        if(!cache.can_draw_exact_instance_batch("region",1,
                                                 {Requirement{17,3},Requirement{18,3}}))return 44;

    } else {
        if (status != kami::world::DeviceStatus::Unavailable) return 7;
        if (device.native_device() || device.graphics_queue()) return 8;
    }
    return 0;
}
