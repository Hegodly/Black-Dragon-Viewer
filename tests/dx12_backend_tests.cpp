#include "kami/world/device.hpp"
#include "kami/world/dx12_mesh_buffer_cache.hpp"
#include <array>
#include <cmath>
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
    const D3D12_VERTEX_BUFFER_VIEW valid_view{0x1000U,204U,68U};
    if(!Cache::valid_instance_vertex_view(valid_view,3U))return 107;
    auto invalid_view=valid_view;invalid_view.BufferLocation=0U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 108;
    invalid_view=valid_view;invalid_view.BufferLocation=0x1001U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 109;
    invalid_view=valid_view;invalid_view.StrideInBytes=0U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 110;
    invalid_view=valid_view;invalid_view.SizeInBytes=205U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 111;
    invalid_view=valid_view;invalid_view.SizeInBytes=272U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 112;
    invalid_view=valid_view;invalid_view.BufferLocation=
        std::numeric_limits<std::uint64_t>::max()-3U;
    if(Cache::valid_instance_vertex_view(invalid_view,3U))return 113;

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
        const auto max_local_id=std::numeric_limits<std::uint32_t>::max();
        if(!cache.upload_instance(device.native_device(),"region:west",max_local_id,7,
                                  scene.data(),scene.size(),0,3))return 114;
        if(!cache.upload_instance(device.native_device(),"region",max_local_id,8,
                                  scene.data(),scene.size(),0,3))return 115;
        if(!cache.can_draw_instance_batch("region:west",{max_local_id})||
           !cache.can_draw_instance_batch("region",{max_local_id})||
           !cache.can_draw_exact_instance_batch("region:west",7,
                                                {Requirement{max_local_id,3}})||
           !cache.can_draw_exact_instance_batch("region",8,
                                                {Requirement{max_local_id,3}}))return 116;
        if(!cache.can_draw_exact_instance_batch("region",1,
                                                {Requirement{17,3},Requirement{18,3}}))return 33;
        if(cache.can_draw_exact_instance_batch("region",2,{Requirement{17,3}}))return 34;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,4}}))return 35;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3},Requirement{17,3}}))return 36;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,3},Requirement{19,3}}))return 37;
        if(cache.can_draw_exact_instance_batch("region",1,{Requirement{17,0}}))return 38;
        if(cache.can_draw_exact_instance_batch("",1,{Requirement{17,3}}))return 39;
        if(cache.can_draw_exact_instance_batch("region",1,{}))return 40;
        std::vector<Requirement> oversized_batch(Cache::kMaxResidentBatchDraws+1U,
                                                   Requirement{17,3});
        if(cache.can_draw_exact_instance_batch("region",1,oversized_batch))return 100;
        std::vector<std::uint32_t> oversized_ids(Cache::kMaxResidentBatchDraws+1U);
        for(std::size_t i=0U;i<oversized_ids.size();++i)
            oversized_ids[i]=static_cast<std::uint32_t>(i+1U);
        if(cache.can_draw_instance_batch("region",oversized_ids))return 117;
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
        const std::array<std::uint32_t,1> only_nearby{{17U}};
        if(!cache.record_visible_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}},
             only_nearby.data(),only_nearby.size()))return 92;
        const std::array<std::uint32_t,2> duplicate_visible{{17U,17U}};
        if(cache.record_visible_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}},
             duplicate_visible.data(),duplicate_visible.size()))return 93;
        const std::array<std::uint32_t,2> unsorted_visible{{18U,17U}};
        if(cache.record_visible_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}},
             unsorted_visible.data(),unsorted_visible.size()))return 95;
        const std::array<std::uint32_t,1> unknown_visible{{99U}};
        if(cache.record_visible_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}},
             unknown_visible.data(),unknown_visible.size()))return 96;
        if(!cache.record_visible_instance_batch(batch_commands.Get(),"region",1,
             {Requirement{17,3},Requirement{18,3}},only_nearby.data(),0U))
            return 97;
        std::array<Cache::IndirectDrawCommand,2> indirect_commands{};
        std::size_t indirect_count=99U;
        const std::array<std::uint32_t,2> both_visible{{17U,18U}};
        if(!cache.build_visible_instance_indirect_commands(
            "region",1,{Requirement{17,3},Requirement{18,3}},
            both_visible.data(),both_visible.size(),indirect_commands.data(),
            indirect_commands.size(),indirect_count)||
           indirect_count!=2U||
           indirect_commands[0].vertex_buffer.BufferLocation!=
               cache.find("region:17")->view.BufferLocation||
           indirect_commands[0].draw.VertexCountPerInstance!=3U||
           indirect_commands[0].draw.InstanceCount!=1U||
           indirect_commands[1].vertex_buffer.BufferLocation!=
               cache.find("region:18")->view.BufferLocation||
           indirect_commands[1].draw.VertexCountPerInstance!=3U)
            return 103;
        // Invalid membership and undersized output never publish a partial
        // command list that ExecuteIndirect could consume.
        indirect_commands[0].draw.VertexCountPerInstance=0xdeadbeefU;
        indirect_count=99U;
        if(cache.build_visible_instance_indirect_commands(
            "region",1,{Requirement{17,3},Requirement{18,3}},
            unknown_visible.data(),unknown_visible.size(),indirect_commands.data(),
            indirect_commands.size(),indirect_count)||
           indirect_count!=0U||
           indirect_commands[0].draw.VertexCountPerInstance!=0xdeadbeefU)
            return 104;
        if(cache.build_visible_instance_indirect_commands(
            "region",1,{Requirement{17,3},Requirement{18,3}},
            both_visible.data(),both_visible.size(),indirect_commands.data(),
            1U,indirect_count)||indirect_count!=0U)return 105;
        if(!cache.build_visible_instance_indirect_commands(
            "region",1,{Requirement{17,3},Requirement{18,3}},
            only_nearby.data(),0U,indirect_commands.data(),
            indirect_commands.size(),indirect_count)||indirect_count!=0U)
            return 106;
        const std::uint64_t indirect_stride=
            sizeof(Cache::IndirectDrawCommand);
        const std::uint64_t indirect_bytes=
            2U*Cache::kMaxResidentBatchDraws*indirect_stride;
        if(!Cache::valid_indirect_argument_range(
            indirect_bytes,0U,2U,Cache::kMaxResidentBatchDraws)||
           !Cache::valid_indirect_argument_range(
            indirect_bytes,Cache::kMaxResidentBatchDraws*indirect_stride,
            Cache::kMaxResidentBatchDraws,Cache::kMaxResidentBatchDraws))
            return 114;
        if(Cache::valid_indirect_argument_range(
            indirect_bytes,1U,1U,Cache::kMaxResidentBatchDraws)||
           Cache::valid_indirect_argument_range(
            indirect_bytes,indirect_bytes-4U,1U,1U)||
           Cache::valid_indirect_argument_range(
            indirect_bytes,0U,Cache::kMaxResidentBatchDraws+1U,
            Cache::kMaxResidentBatchDraws)||
           Cache::valid_indirect_argument_range(
            indirect_bytes,std::numeric_limits<std::uint64_t>::max()-3U,
            1U,1U)||
           Cache::valid_indirect_argument_range(
            indirect_bytes,0U,0U,Cache::kMaxResidentBatchDraws+1U))
            return 115;
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
        if(!Cache::potentially_visible({3,0,0,.5f,true},
            std::vector<Cache::VisibilityPlane>{}))return 55;
        if(!Cache::potentially_visible({3,0,0,-1.f,true},cube))return 56;
        const Cache::VisibilityAabb outside_box{
            1.5f,-.5f,-.5f,2.5f,.5f,.5f,true};
        const Cache::VisibilityAabb crossing_box{
            .9f,-.5f,-.5f,1.1f,.5f,.5f,true};
        if(!Cache::potentially_visible({1.8f,0,0,1.f,true},cube)||
           Cache::potentially_visible_aabb(outside_box,cube)||
           !Cache::potentially_visible_aabb(crossing_box,cube)||
           !Cache::potentially_visible_aabb({},cube))return 98;
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
        // Direct3D LH perspective frustum: near=1, far=10, FOV=90 degrees.
        const std::array<float,16> perspective{{
            1,0,0,0, 0,1,0,0, 0,0,1.1111111f,1, 0,0,-1.1111111f,0}};
        std::array<Cache::VisibilityPlane,6> extracted{};
        if(!Cache::extract_frustum_planes(perspective,extracted))return 80;
        if(!Cache::potentially_visible({0,0,5.f,.1f,true},extracted))return 81;
        if(Cache::potentially_visible({20,0,5.f,.1f,true},extracted))return 82;
        if(Cache::potentially_visible({0,0,.5f,.1f,true},extracted))return 83;
        if(Cache::potentially_visible({0,0,12.f,.1f,true},extracted))return 84;
        std::array<float,16> invalid_matrix{};
        if(Cache::extract_frustum_planes(invalid_matrix,extracted))return 85;
        struct PositionVertex {float px,py,pz;};
        const std::array<PositionVertex,3> triangle{{
            {-1.f,0.f,0.f},{1.f,0.f,0.f},{0.f,2.f,0.f}}};
        const auto triangle_bounds=Cache::bounds_for_vertices(
            triangle.data(),triangle.size());
        const auto triangle_aabb=Cache::aabb_for_vertices(
            triangle.data(),triangle.size());
        if(!triangle_bounds.valid||std::fabs(triangle_bounds.x)>1e-6f||
           std::fabs(triangle_bounds.y-1.f)>1e-6f||
           std::fabs(triangle_bounds.radius-std::sqrt(2.f))>1e-5f||
           !triangle_aabb.valid||triangle_aabb.min_x!=-1.f||
           triangle_aabb.min_y!=0.f||triangle_aabb.min_z!=0.f||
           triangle_aabb.max_x!=1.f||triangle_aabb.max_y!=2.f||
           triangle_aabb.max_z!=0.f)return 86;
        auto corrupt_triangle=triangle;
        corrupt_triangle[1].px=std::numeric_limits<float>::quiet_NaN();
        const auto corrupt_bounds=Cache::bounds_for_vertices(
            corrupt_triangle.data(),corrupt_triangle.size());
        const auto corrupt_aabb=Cache::aabb_for_vertices(
            corrupt_triangle.data(),corrupt_triangle.size());
        if(corrupt_bounds.valid||
           !Cache::potentially_visible(corrupt_bounds,extracted)||
           corrupt_aabb.valid||
           !Cache::potentially_visible_aabb(corrupt_aabb,extracted))return 87;
        using Requirement=Cache::InstanceDrawRequirement;
        using Visibility=Cache::InstanceVisibility;
        // Requirements intentionally arrive in a different order from the
        // sorted bounds cache; output IDs are canonical while draw order stays
        // governed by the original requirement list.
        const std::vector<Requirement> visibility_requirements{
            {20,3},{18,3},{17,3},{19,3}};
        const std::vector<Visibility> visibility_bounds{
            {17,{0,0,5.f,.1f,true}},
            {18,{20,0,5.f,.1f,true}},
            {19,{20,0,5.f,.1f,false}}};
        std::array<std::uint32_t,4> visible_ids{};
        std::size_t visible_count=0U;
        if(!Cache::select_visible_instance_ids(
            visibility_requirements,visibility_bounds,extracted,
            visible_ids.data(),visible_ids.size(),visible_count)||
           visible_count!=3U||visible_ids[0]!=17U||
           visible_ids[1]!=19U||visible_ids[2]!=20U)return 88;
        const std::array<Cache::VisibilityPlane,6> shadow_frustum{{
            {1,0,0,100},{-1,0,0,100},{0,1,0,100},
            {0,-1,0,100},{0,0,1,100},{0,0,-1,100}}};
        std::array<std::uint32_t,4> shadow_ids{};
        std::size_t shadow_count=0U;
        if(!Cache::select_visible_instance_ids(
            visibility_requirements,visibility_bounds,shadow_frustum,
            shadow_ids.data(),shadow_ids.size(),shadow_count)||
           shadow_count!=4U||shadow_ids[0]!=17U||shadow_ids[1]!=18U||
           shadow_ids[2]!=19U||shadow_ids[3]!=20U)return 99;
        auto ambiguous_bounds=visibility_bounds;
        ambiguous_bounds.insert(ambiguous_bounds.begin()+2,
            {18,{20,0,5.f,.1f,true}});
        if(!Cache::select_visible_instance_ids(
            visibility_requirements,ambiguous_bounds,extracted,
            visible_ids.data(),visible_ids.size(),visible_count)||
           visible_count!=4U)return 89;
        auto unsorted_bounds=visibility_bounds;
        std::swap(unsorted_bounds[0],unsorted_bounds[2]);
        if(Cache::select_visible_instance_ids(
            visibility_requirements,unsorted_bounds,extracted,
            visible_ids.data(),visible_ids.size(),visible_count)||
           visible_count!=0U)return 94;
        if(Cache::select_visible_instance_ids(
            {{17,3},{17,3}},visibility_bounds,extracted,
            visible_ids.data(),visible_ids.size(),visible_count))return 90;
        if(Cache::select_visible_instance_ids(
            visibility_requirements,visibility_bounds,extracted,
            visible_ids.data(),1U,visible_count))return 91;
        // The selector must never write past its fixed 256-ID scratch budget.
        std::vector<Requirement> oversized_visibility(Cache::kMaxResidentBatchDraws+1U);
        for(std::size_t i=0U;i<oversized_visibility.size();++i)
            oversized_visibility[i]={static_cast<std::uint32_t>(i+1U),3U};
        std::array<std::uint32_t,Cache::kMaxResidentBatchDraws+1U> oversized_output{};
        oversized_output[0]=0xdeadbeefU;
        visible_count=123U;
        if(Cache::select_visible_instance_ids(
            oversized_visibility,visibility_bounds,extracted,
            oversized_output.data(),oversized_output.size(),visible_count)||
           visible_count!=0U||oversized_output[0]!=0xdeadbeefU)return 101;
        // Exact AABBs take precedence over loose spheres, while unknown bounds
        // still fall back conservatively to sphere/missing-bound visibility.
        const std::vector<Visibility> aabb_selection_bounds{
            {17,{20,0,5.f,.1f,true},{-.1f,-.1f,4.9f,.1f,.1f,5.1f,true}},
            {18,{0,0,5.f,.1f,true},{6.f,-.5f,4.9f,7.f,.5f,5.1f,true}},
            {19,{20,0,5.f,.1f,false},{} }};
        if(!Cache::select_visible_instance_ids(
            visibility_requirements,aabb_selection_bounds,extracted,
            visible_ids.data(),visible_ids.size(),visible_count)||
           visible_count!=3U||visible_ids[0]!=17U||
           visible_ids[1]!=19U||visible_ids[2]!=20U)return 102;
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
