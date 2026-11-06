#include "kami/world/device.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include "kami/world/scene.hpp"
#include "kami/world/texture_atlas.hpp"
#include "kami/world/scene_diff.hpp"
#include <array>
#include <limits>
static_assert(sizeof(kami::world::Vertex)==72U,
    "DX12 mesh texture layer must fit the vertex input layout");

int main() {
    static_assert(offsetof(kami::world::Vertex,pbr_material)==68U);
    // Incremental GPU atlas uploads must touch only changed material
    // slices, not repack unrelated textures on every J2C arrival.
    using kami::world::changed_atlas_layers;
    const std::map<std::string,std::uint16_t> before{
        {"texture-a",1U},{"texture-b",6U},{"texture-c",24U}};
    const std::map<std::string,std::uint16_t> after{
        {"texture-a",1U},{"texture-d",6U},{"texture-c",24U}};
    if(changed_atlas_layers(before,before).size()!=0U)return 7;
    if(changed_atlas_layers(before,after)!=
       std::vector<std::uint16_t>{6U})return 8;
    if(changed_atlas_layers(after,{} )!=
       std::vector<std::uint16_t>({1U,6U,24U}))return 9;
    if(changed_atlas_layers({},before)!=
       std::vector<std::uint16_t>({1U,6U,24U}))return 10;
    if(!changed_atlas_layers(before,after,0U).empty())return 11;
    // A changed simulator revision is not evidence that the world
    // vertex buffer changed. Unchanged payloads should not trigger DX12
    // heap allocation, Map, resource replacement, or GPU fence waits.
    using kami::world::same_scene_geometry;
    kami::world::SceneMesh scene_a{},scene_b{};
    if(!same_scene_geometry(scene_a,scene_b))return 12;
    kami::world::Vertex vertex{};
    vertex.px=3.5f;vertex.ny=1.f;
    vertex.r=0.2f;vertex.g=0.4f;vertex.b=0.6f;
    scene_a.vertices.push_back(vertex);
    scene_b=scene_a;
    scene_a.terrain_count=scene_b.terrain_count=1U;
    if(!same_scene_geometry(scene_a,scene_b))return 13;
    scene_b.vertices[0].px=5.5f;
    if(same_scene_geometry(scene_a,scene_b))return 14;
    scene_b.vertices[0].px=3.5f;
    scene_b.avatar_count=1U;
    if(same_scene_geometry(scene_a,scene_b))return 15;
    scene_b.avatar_count=0U;
    scene_b.vertices.push_back(vertex);
    if(same_scene_geometry(scene_a,scene_b))return 16;
    // Updating an authenticated own-avatar position by centimeters must
    // not rebuild the whole world. A real 12-meter movement must still
    // request a new region-relative scene/interest set.
    using kami::world::world_scene_rebase_due;
    constexpr std::array<float,3> previous_pose{64.f,96.f,32.f};
    if(world_scene_rebase_due({64.04f,96.f,32.f},previous_pose,true))
        return 17;
    if(world_scene_rebase_due({75.99f,96.f,32.f},previous_pose,true))
        return 18;
    if(!world_scene_rebase_due({76.f,96.f,32.f},previous_pose,true))
        return 19;
    if(!world_scene_rebase_due({64.f,96.f,32.f},previous_pose,false))
        return 20;
    if(world_scene_rebase_due(
           {std::numeric_limits<float>::infinity(),96.f,32.f},
           previous_pose,true))return 21;
    // A 12m simulator movement refresh must NOT shift buildings away
    // from the camera, and must NEVER translate the local avatar marker.
    kami::world::SceneMesh rebased{};
    kami::world::Vertex world_v{},avatar_v{};
    world_v.px=7.f;world_v.py=2.f;world_v.pz=5.f;
    avatar_v.px=3.f;avatar_v.py=4.f;avatar_v.pz=5.f;
    rebased.vertices={world_v,avatar_v};
    rebased.terrain_count=1U;rebased.avatar_count=1U;
    if(!kami::world::align_world_vertices_to_origin(
        rebased,{146.f,150.f,44.f},{128.f,128.f,40.f}))
        return 22;
    if(rebased.vertices[0].px!=25.f||
       rebased.vertices[0].py!=6.f||
       rebased.vertices[0].pz!=27.f||
       rebased.vertices[1].px!=avatar_v.px||
       rebased.vertices[1].py!=avatar_v.py||
       rebased.vertices[1].pz!=avatar_v.pz)return 23;
    auto malformed_rebase=rebased;
    malformed_rebase.terrain_count=3U;
    if(kami::world::align_world_vertices_to_origin(
        malformed_rebase,{1.f,2.f,3.f},{1.f,2.f,3.f}))return 24;
    if(kami::world::align_world_vertices_to_origin(
        rebased,{std::numeric_limits<float>::infinity(),150.f,44.f},
        {128.f,128.f,40.f}))return 25;
    // Failed live-origin alignment must not partially move previously
    // visible terrain while the simulator retries a malformed update.
    kami::world::SceneMesh malformed_positions{};
    kami::world::Vertex valid{},invalid{};
    valid.px=1.f;valid.py=2.f;valid.pz=3.f;
    invalid.px=std::numeric_limits<float>::max();
    malformed_positions.vertices={valid,invalid};
    malformed_positions.terrain_count=2U;
    // Translating a finite FLT_MAX by -2 is still finite (often rounds
    // back to FLT_MAX). Use an offset large enough to overflow the
    // transformed vertex, rather than asserting that any large input
    // must fail.
    if(kami::world::align_world_vertices_to_origin(
           malformed_positions,{std::numeric_limits<float>::max(),0.f,0.f},
           {-std::numeric_limits<float>::max(),0.f,0.f}))
        return 27;
    if(malformed_positions.vertices[0].px!=1.f||
       malformed_positions.vertices[1].px!=invalid.px)
        return 28;
    malformed_positions.vertices[1].px=
        std::numeric_limits<float>::quiet_NaN();
    if(kami::world::align_world_vertices_to_origin(
           malformed_positions,{2.f,0.f,0.f},{0.f,0.f,0.f}))
        return 29;
    if(malformed_positions.vertices[0].px!=1.f)return 30;
    // Layers above the old limit must be copied on reassignment, with
    // unchanged material slots left untouched.
    const std::map<std::string,std::uint16_t> high_before{
        {"old-image",kami::protocol::texture::kGPUImageLayers}};
    const std::map<std::string,std::uint16_t> high_after{
        {"new-image",kami::protocol::texture::kGPUImageLayers}};
    if(changed_atlas_layers(high_before,high_after)!=
       std::vector<std::uint16_t>{kami::protocol::texture::kGPUImageLayers})
        return 26;
    kami::world::Device device;
    if(device.initialize(true)==kami::world::DeviceStatus::Unavailable)
        return 0; // The hosted VM may not have a DX12 adapter.
    D3D12_RESOURCE_DESC texture{};
    texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width=64;
    texture.Height=64;
    texture.DepthOrArraySize=1;
    texture.MipLevels=1;
    texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count=1;
    texture.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT rows=0;
    UINT64 row_bytes=0,total_bytes=0;
    device.native_device()->GetCopyableFootprints(
        &texture,0,1,0,&layout,&rows,&row_bytes,&total_bytes);
    if(rows!=64 || row_bytes!=256) return 1;
    if(layout.Footprint.RowPitch<row_bytes ||
       layout.Footprint.RowPitch%256!=0) return 2;
    if(total_bytes<row_bytes*rows) return 3;
    if(layout.Footprint.Format!=DXGI_FORMAT_R8G8B8A8_UNORM) return 4;
    // Full shipped texture array: fallback plus bounded SL image layers.
    constexpr unsigned slices=kami::protocol::texture::kGPUImageLayers+1U;
    texture.Width=kami::protocol::texture::kPreviewExtent;
    texture.Height=kami::protocol::texture::kPreviewExtent;
    texture.DepthOrArraySize=slices;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,slices> mapped{};
    std::array<UINT,slices> rows_arr{};
    std::array<UINT64,slices> sizes{};
    UINT64 upload=0;
    device.native_device()->GetCopyableFootprints(
        &texture,0,slices,0,mapped.data(),rows_arr.data(),sizes.data(),&upload);
    if(upload==0||upload>kami::protocol::texture::kMaxUploadBytes)return 5;
    for(unsigned i=0;i<slices;++i){
        if(rows_arr[i]!=256U||sizes[i]!=1024U||
           mapped[i].Footprint.RowPitch%256U!=0U)return 6;
    }
    return 0;
}

