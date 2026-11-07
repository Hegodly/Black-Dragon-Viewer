#include "kami/world/device.hpp"
#include "kami/world/dx12_mesh_buffer_cache.hpp"
#include <array>
#include <cstdint>
#include <limits>

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
    const std::vector<Span> owned_triangles{
        Span{11,3,6},Span{12,12,3}};
    if(!Cache::triangle_is_in_instance_spans(3,owned_triangles))return 57;
    if(!Cache::triangle_is_in_instance_spans(6,owned_triangles))return 58;
    if(!Cache::triangle_is_in_instance_spans(12,owned_triangles))return 59;
    if(Cache::triangle_is_in_instance_spans(0,owned_triangles))return 60;
    if(Cache::triangle_is_in_instance_spans(9,owned_triangles))return 61;
    if(Cache::triangle_is_in_instance_spans(15,owned_triangles))return 62;
    if(Cache::triangle_is_in_instance_spans(4,owned_triangles))return 63;
    if(!Cache::within_resident_batch_draw_budget(1U)||
       !Cache::within_resident_batch_draw_budget(Cache::kMaxResidentBatchDraws)||
       Cache::within_resident_batch_draw_budget(Cache::kMaxResidentBatchDraws+1U)||
       Cache::within_resident_batch_draw_budget(0U))return 64;

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
        if(cache.record_instance_batch(nullptr,"region",1,
             {Requirement{17,3},Requirement{18,3}}))return 73;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> batch_allocator;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> batch_commands;
        if(FAILED(device.native_device()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(batch_allocator.GetAddressOf()))))return 74;
        if(FAILED(device.native_device()->CreateCommandList(
            0,D3D12_COMMAND_LIST_TYPE_DIRECT,batch_allocator.Get(),nullptr,
            IID_PPV_ARGS(batch_commands.GetAddressOf()))))return 75;
        if(cache.record_instance_batch(batch_commands.Get(),"region",2,
             {Requirement{17,3},Requirement{18,3}}))return 76;
        if(cache.record_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{19,3}}))return 77;
        if(!cache.record_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}}))return 78;
        if(FAILED(batch_commands->Close()))return 79;
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
        // Large but finite region/frustum coordinates must be evaluated
        // without float dot-product overflow or accidental mesh loss.
        const float huge=std::numeric_limits<float>::max();
        const std::vector<Cache::VisibilityPlane> extreme{
            {huge,0,0,huge},{-huge,0,0,huge},
            {0,huge,0,huge},{0,-huge,0,huge},
            {0,0,huge,huge},{0,0,-huge,huge}};
        if(!Cache::potentially_visible({0,0,0,1.f,true},extreme))return 70;
        if(Cache::potentially_visible({3.f,0,0,.5f,true},extreme))return 71;
        if(!Cache::potentially_visible({1.5f,0,0,.5f,true},extreme))return 72;
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
