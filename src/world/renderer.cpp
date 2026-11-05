#include "kami/world/renderer.hpp"
#include "kami/world/live_terrain.hpp"
#include "kami/world/live_water.hpp"
#include "kami/world/object_proxies.hpp"
#include "kami/world/live_primitive_geometry.hpp"
#include "kami/world/texture_atlas.hpp"
#include "kami/world/scene_diff.hpp"
#include "kami/protocol/simulator_transport.hpp"
#include "kami/features/creator_limits.hpp"
#include "kami/protocol/bootstrap.hpp"
#include "kami/world/scene.hpp"
#include "kami/world/shader_cache.hpp"
#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace kami::world {
namespace {
constexpr DXGI_FORMAT kColorFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthFormat=DXGI_FORMAT_D32_FLOAT;
UINT scaled(UINT source,unsigned scale) {
    return std::max<UINT>(64U,static_cast<UINT>((static_cast<std::uint64_t>(source)*scale+50U)/100U));
}



D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES h{};
    h.Type=type;
    h.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    h.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN;
    h.CreationNodeMask=1;
    h.VisibleNodeMask=1;
    return h;
}
D3D12_RESOURCE_DESC buffer_desc(UINT64 bytes) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=bytes;
    d.Height=1;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.Format=DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
D3D12_RESOURCE_BARRIER barrier(ID3D12Resource* resource,
                                D3D12_RESOURCE_STATES from,
                                D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=resource;
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore=from;
    b.Transition.StateAfter=to;
    return b;
}
} // namespace

Renderer::~Renderer() { shutdown(); }

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::rtv_handle(UINT index) const noexcept {
    auto handle=rtv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=static_cast<SIZE_T>(index)*rtv_step_;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Renderer::shadow_dsv() const noexcept {
    auto handle=dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=device_.native_device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    return handle;
}

bool Renderer::make_shadow() {
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=kShadowSize;
    desc.Height=kShadowSize;
    desc.DepthOrArraySize=1;
    desc.MipLevels=1;
    desc.Format=DXGI_FORMAT_R32_TYPELESS;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};
    clear.Format=DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth=1.f;
    const auto properties=heap(D3D12_HEAP_TYPE_DEFAULT);
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &properties,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,
            IID_PPV_ARGS(shadow_map_.ReleaseAndGetAddressOf()))))
        return false;
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format=DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    device_.native_device()->CreateDepthStencilView(
        shadow_map_.Get(),&dsv,shadow_dsv());
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels=1;
    device_.native_device()->CreateShaderResourceView(
        shadow_map_.Get(),&srv,
        shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart());
    return true;
}



static_assert(sizeof(Vertex)==68U,
              "KAMI GPU vertex input layout must match 68-byte packed vertices");
static_assert(sizeof(rig::Matrix)==64U,
              "KAMI GPU skinning palette must pack four float4 rows per joint");

bool Renderer::make_skin_palette() {
    if(!device_.native_device()||!shadow_srv_heap_)return false;
    // A small dynamically updated 10-bone palette; one mapped upload buffer.
    // GPU frames are fenced before reuse by the existing draw() scheduler.
    const auto properties=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto desc=buffer_desc(sizeof(rig::Matrix)*rig::kJointCount);
    if(FAILED(device_.native_device()->CreateCommittedResource(
        &properties,D3D12_HEAP_FLAG_NONE,&desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
        IID_PPV_ARGS(skin_palette_buffer_.ReleaseAndGetAddressOf()))))
        return false;
    const rig::Pose rest{};
    if(!rig::make_palette(rest,skin_matrices_))return false;
    if(!update_skin_palette())return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=DXGI_FORMAT_UNKNOWN;
    srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.FirstElement=0;
    srv.Buffer.NumElements=static_cast<UINT>(rig::kJointCount*4U);
    srv.Buffer.StructureByteStride=sizeof(float)*4U;
    srv.Buffer.Flags=D3D12_BUFFER_SRV_FLAG_NONE;
    auto handle=shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=2U*device_.native_device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device_.native_device()->CreateShaderResourceView(
        skin_palette_buffer_.Get(),&srv,handle);
    return true;
}
bool Renderer::update_skin_palette() noexcept {
    if(!skin_palette_buffer_)return false;
    void* data=nullptr;
    const D3D12_RANGE no_read{0,0};
    if(FAILED(skin_palette_buffer_->Map(0,&no_read,&data))||!data)return false;
    std::memcpy(data,skin_matrices_.data(),sizeof(skin_matrices_));
    const D3D12_RANGE write{0,sizeof(skin_matrices_)};
    skin_palette_buffer_->Unmap(0,&write);
    return true;
}


TextureUploadStatus Renderer::make_material_texture(
    const std::map<std::string,protocol::texture::DecodedTexture>* images,
    const std::map<std::string,std::uint16_t>* previous_layers) {
    // R50: retain the GPU Texture2DArray and SRV. Only upload layers whose
    // UUID changed; the old path rebuilt all 49 slices (12.25 MiB), allocated
    // a new GPU texture, and stalled the graphics queue on each new J2C.
    constexpr UINT tw=kLiveTextureSize,th=kLiveTextureSize;
    constexpr UINT slices=kLiveTextureLayers+1U;
    auto* dev=device_.native_device();
    if(!dev||!shadow_srv_heap_||!texture_copy_allocator_||
       !texture_copy_commands_||!fence_)return TextureUploadStatus::Failed;
    // Never reset a copy command allocator while the GPU still uses it.
    // The normal draw loop fences after presenting, so this branch only
    // defers an unusually rapid second texture update, without stalling.
    if(texture_copy_submitted_fence_!=0U){
        if(fence_->GetCompletedValue()<texture_copy_submitted_fence_)
            return TextureUploadStatus::Deferred;
        material_upload_.Reset();
        texture_copy_submitted_fence_=0U;
    }
    const bool incremental=material_texture_&&previous_layers;
    std::vector<std::uint16_t> dirty;
    if(incremental)
        dirty=changed_atlas_layers(*previous_layers,live_texture_layers_);
    else{
        dirty.reserve(slices);
        for(UINT i=0;i<slices;++i)
            dirty.push_back(static_cast<std::uint16_t>(i));
    }
    if(dirty.empty())return TextureUploadStatus::Committed;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=tw;
    desc.Height=th;
    desc.DepthOrArraySize=static_cast<UINT16>(slices);
    desc.MipLevels=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D12Resource> image_gpu;
    if(incremental)image_gpu=material_texture_;
    else{
        const auto heap_default=heap(D3D12_HEAP_TYPE_DEFAULT);
        if(FAILED(dev->CreateCommittedResource(
            &heap_default,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
            IID_PPV_ARGS(image_gpu.GetAddressOf()))))return TextureUploadStatus::Failed;
    }
    // One upload footprint per changed layer (not the entire array).
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0U;
    UINT64 row_bytes=0U,one_size=0U;
    dev->GetCopyableFootprints(
        &desc,0U,1U,0U,&footprint,&rows,&row_bytes,&one_size);
    if(rows!=th||row_bytes!=tw*4U||one_size==0U||
       one_size>1024U*1024U||dirty.size()>slices||
       dirty.size()>16U*1024U*1024U/one_size)return TextureUploadStatus::Failed;
    const UINT64 upload_size=one_size*dirty.size();
    const auto heap_upload=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto upload_desc=buffer_desc(upload_size);
    Microsoft::WRL::ComPtr<ID3D12Resource> staging;
    if(FAILED(dev->CreateCommittedResource(
        &heap_upload,D3D12_HEAP_FLAG_NONE,&upload_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
        IID_PPV_ARGS(staging.GetAddressOf()))))return TextureUploadStatus::Failed;

    std::array<const protocol::texture::DecodedTexture*,slices> decoded{};
    if(images){
        for(const auto& [id,layer]:live_texture_layers_){
            if(layer==0U||layer>=slices)continue;
            const auto it=images->find(id);
            if(it==images->end())continue;
            const auto& bitmap=it->second;
            const UINT64 pixels=UINT64(bitmap.width)*bitmap.height;
            if(bitmap.width==0U||bitmap.height==0U||
               bitmap.width>2048U||bitmap.height>2048U||
               pixels>4U*1024U*1024U||
               bitmap.rgba.size()!=pixels*4U)continue;
            decoded[layer]=&bitmap;
        }
    }
    std::uint8_t* mapped=nullptr;
    const D3D12_RANGE no_read{0,0};
    if(FAILED(staging->Map(0,&no_read,
        reinterpret_cast<void**>(&mapped)))||!mapped)return TextureUploadStatus::Failed;
    std::memset(mapped,0,static_cast<std::size_t>(upload_size));
    for(std::size_t index=0U;index<dirty.size();++index){
        const UINT layer=dirty[index];
        const auto* source=decoded[layer];
        auto* dest=mapped+static_cast<std::size_t>(index*one_size);
        for(UINT y=0;y<th;++y){
            auto* row=dest+static_cast<std::size_t>(y)*
                footprint.Footprint.RowPitch;
            for(UINT x=0;x<tw;++x){
                auto* pixel=row+static_cast<std::size_t>(x)*4U;
                if(source){
                    const auto sx=std::size_t(
                        UINT64(x)*source->width/tw);
                    const auto sy=std::size_t(
                        UINT64(y)*source->height/th);
                    const auto at=4U*(sy*source->width+sx);
                    std::memcpy(pixel,source->rgba.data()+at,4U);
                }else{
                    const bool tile=((x/32U+y/32U)%2U)==0U;
                    pixel[0]=tile?220U:128U;
                    pixel[1]=tile?205U:160U;
                    pixel[2]=tile?180U:215U;
                    pixel[3]=255U;
                }
            }
        }
    }
    const D3D12_RANGE written{0,static_cast<SIZE_T>(upload_size)};
    staging->Unmap(0,&written);
    if(FAILED(texture_copy_allocator_->Reset())||
       FAILED(texture_copy_commands_->Reset(
           texture_copy_allocator_.Get(),nullptr)))
       return TextureUploadStatus::Failed;
    auto* copy=texture_copy_commands_.Get();
    if(incremental){
        const auto copying=barrier(image_gpu.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
        copy->ResourceBarrier(1,&copying);
    }
    for(std::size_t index=0U;index<dirty.size();++index){
        const UINT layer=dirty[index];
        D3D12_TEXTURE_COPY_LOCATION dest{};
        dest.pResource=image_gpu.Get();
        dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex=layer;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource=staging.Get();
        src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint=footprint;
        src.PlacedFootprint.Offset=UINT64(index)*one_size;
        copy->CopyTextureRegion(&dest,0,0,0,&src,nullptr);
    }
    const auto ready=barrier(image_gpu.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    copy->ResourceBarrier(1,&ready);
    if(FAILED(copy->Close()))return TextureUploadStatus::Failed;
    ID3D12CommandList* lists[]={copy};
    // Copy and graphics draw use the SAME ordered queue. No CPU wait is
    // required here: the following draw sees the updated texture state.
    device_.graphics_queue()->ExecuteCommandLists(1,lists);
    // Hold the upload heap until the copy fence completes. Do not free
    // it at function scope or reuse the allocator before that fence.
    material_upload_=std::move(staging);
    const auto signal=++fence_counter_;
    if(FAILED(device_.graphics_queue()->Signal(fence_.Get(),signal))){
        ready_=false;
        error_=L"DX12 texture copy fence signal failed";
        return TextureUploadStatus::Failed;
    }
    texture_copy_submitted_fence_=signal;
    if(!incremental){
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2DArray.MipLevels=1;
        srv.Texture2DArray.ArraySize=slices;
        auto handle=shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart();
        handle.ptr+=dev->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        dev->CreateShaderResourceView(image_gpu.Get(),&srv,handle);
        material_texture_=std::move(image_gpu);
    }
    return TextureUploadStatus::Committed;
}

bool Renderer::make_targets() {
    auto* graphics=device_.native_device();
    for(UINT i=0;i<kBuffers;++i) {
        buffers_[i].Reset();
        if(FAILED(swapchain_->GetBuffer(
                i,IID_PPV_ARGS(buffers_[i].ReleaseAndGetAddressOf()))))
            return false;
        graphics->CreateRenderTargetView(buffers_[i].Get(),nullptr,rtv_handle(i));
    }
    return true;
}

bool Renderer::make_depth() {
    depth_.Reset();
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=width_;
    desc.Height=height_;
    desc.DepthOrArraySize=1;
    desc.MipLevels=1;
    desc.Format=kDepthFormat;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};
    clear.Format=kDepthFormat;
    clear.DepthStencil.Depth=1.f;
    const auto properties=heap(D3D12_HEAP_TYPE_DEFAULT);
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &properties,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,
            IID_PPV_ARGS(depth_.ReleaseAndGetAddressOf()))))
        return false;
    device_.native_device()->CreateDepthStencilView(
        depth_.Get(),nullptr,dsv_heap_->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool Renderer::make_pipeline() {
    ShaderPreparation report;
    if(!ShaderCache::prepare(device_.native_device(),signature_,pipeline_,report))
        return false;
    ShaderPreparation shadow_report;
    if(!ShaderCache::prepare_shadow(
        device_.native_device(),signature_.Get(),shadow_pipeline_,shadow_report))
        return false;
    ShaderPreparation sky_report;
    return ShaderCache::prepare_sky(
        device_.native_device(),signature_.Get(),sky_pipeline_,sky_report);
}

bool Renderer::make_geometry() {
    SceneMesh generated;
    if(!preview_mesh_&&!region_mesh_&&!live_water_mesh_)
        generated=build_test_scene(graphics_.terrain_divisions());
    // Local creator preview temporarily overrides a decoded region scene.
    // Both static and animated scenes go through the SAME DX12 buffers,
    // shader stages, PBR and depth/shadow passes.
    const SceneMesh& scene=preview_mesh_?*preview_mesh_:
        region_mesh_?*region_mesh_:
        live_water_mesh_?*live_water_mesh_:generated;
    if(scene.vertices.empty() ||
       scene.vertices.size()>std::numeric_limits<UINT>::max()/sizeof(Vertex))
        return false;
    const auto audit=rig::inspect(scene.vertices);
    if(audit.status==rig::Compatibility::Invalid)return false;
    const UINT bytes=static_cast<UINT>(scene.vertices.size()*sizeof(Vertex));
    const auto properties=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto desc=buffer_desc(bytes);
    // GPU uploads are transactional. Never release a known-good world
    // buffer until CreateCommittedResource AND Map both succeed.
    Microsoft::WRL::ComPtr<ID3D12Resource> candidate;
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &properties,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
            IID_PPV_ARGS(candidate.GetAddressOf()))))
        return false;
    void* mapped=nullptr;
    const D3D12_RANGE no_read{0,0};
    if(FAILED(candidate->Map(0,&no_read,&mapped))||!mapped)return false;
    std::memcpy(mapped,scene.vertices.data(),bytes);
    const D3D12_RANGE written{0,bytes};
    candidate->Unmap(0,&written);
    geometry_=std::move(candidate);
    skin_audit_=audit;
    terrain_count_=scene.terrain_count;
    avatar_count_=scene.avatar_count;
    vertex_view_.BufferLocation=geometry_->GetGPUVirtualAddress();
    vertex_view_.SizeInBytes=bytes;
    vertex_view_.StrideInBytes=sizeof(Vertex);
    return true;
}

bool Renderer::initialize(HWND window,UINT width,UINT height,
                          bool allow_software_dx12,
                          bool force_software_dx12) {
    if(ready_) return true;
    if(!window || !width || !height) { error_=L"Invalid viewport dimensions"; return false; }
    window_=window;
    avatar_.reset(sample_test_terrain_height);
    rig_physics_.reset();
    client_width_=width;
    client_height_=height;
    width_=scaled(width,graphics_.render_scale_percent);
    height_=scaled(height,graphics_.render_scale_percent);
    if(device_.initialize(allow_software_dx12,force_software_dx12)==
       DeviceStatus::Unavailable) {
        error_=L"No DirectX 12 device available";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(factory.GetAddressOf())))) {
        error_=L"DXGI factory creation failed";
        return false;
    }
    Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;
    if(SUCCEEDED(factory.As(&factory5))) {
        BOOL supported=FALSE;
        if(SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING,&supported,sizeof(supported))))
            allow_tearing_=supported!=FALSE;
    }
    DXGI_SWAP_CHAIN_DESC1 swap{};
    swap.Width=width_;
    swap.Height=height_;
    swap.Format=kColorFormat;
    swap.Stereo=FALSE;
    swap.SampleDesc.Count=1;
    swap.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.BufferCount=kBuffers;
    swap.Scaling=DXGI_SCALING_STRETCH;
    swap.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
    swap.Flags=allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> chain;
    if(FAILED(factory->CreateSwapChainForHwnd(
            device_.graphics_queue(),window_,&swap,nullptr,nullptr,
            chain.GetAddressOf())) ||
       FAILED(chain.As(&swapchain_))) {
        error_=L"DX12 swap chain creation failed";
        return false;
    }
    (void)factory->MakeWindowAssociation(window_,DXGI_MWA_NO_ALT_ENTER);

    D3D12_DESCRIPTOR_HEAP_DESC rtv{};
    rtv.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv.NumDescriptors=kBuffers;
    if(FAILED(device_.native_device()->CreateDescriptorHeap(
            &rtv,IID_PPV_ARGS(rtv_heap_.ReleaseAndGetAddressOf())))) {
        error_=L"Could not allocate render targets";
        return false;
    }
    rtv_step_=device_.native_device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};
    dsv.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv.NumDescriptors=2;
    D3D12_DESCRIPTOR_HEAP_DESC srv{};
    srv.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv.NumDescriptors=3;
    srv.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(FAILED(device_.native_device()->CreateDescriptorHeap(
            &dsv,IID_PPV_ARGS(dsv_heap_.ReleaseAndGetAddressOf()))) ||
       FAILED(device_.native_device()->CreateDescriptorHeap(
            &srv,IID_PPV_ARGS(shadow_srv_heap_.ReleaseAndGetAddressOf()))) ||
       !make_targets() || !make_depth() || !make_shadow()) {
        error_=L"Depth or color attachments failed";
        return false;
    }
    if(!make_pipeline()) {
        error_=L"DirectX 12 scene shader compilation or pipeline failed";
        return false;
    }
    if(!make_geometry()) {
        error_=L"Could not create terrain/avatar geometry";
        return false;
    }
    if(FAILED(device_.native_device()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator_.ReleaseAndGetAddressOf()))) ||
       FAILED(device_.native_device()->CreateCommandList(
            0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.Get(),pipeline_.Get(),
            IID_PPV_ARGS(commands_.ReleaseAndGetAddressOf()))) ||
       FAILED(commands_->Close())) {
        error_=L"Could not create command buffers";
        return false;
    }
    // Texture copies are ordered on the same graphics queue but have
    // their own allocator/list. This makes it safe to submit a J2C slice
    // upload before the normal draw without resetting an in-flight
    // allocator or waiting for its completion on the UI thread.
    if(FAILED(device_.native_device()->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(texture_copy_allocator_.ReleaseAndGetAddressOf())))||
       FAILED(device_.native_device()->CreateCommandList(
            0,D3D12_COMMAND_LIST_TYPE_DIRECT,
            texture_copy_allocator_.Get(),nullptr,
            IID_PPV_ARGS(texture_copy_commands_.ReleaseAndGetAddressOf())))||
       FAILED(texture_copy_commands_->Close())){
        error_=L"Could not create independent DX12 texture copy commands";
        return false;
    }
    if(FAILED(device_.native_device()->CreateFence(
            0,D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())))) {
        error_=L"Could not create GPU fence";
        return false;
    }
    fence_event_=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!fence_event_) {
        error_=L"Could not create GPU event";
        return false;
    }
    if(make_material_texture()!=TextureUploadStatus::Committed) {
        error_=L"Could not upload DX12 material texture";
        return false;
    }
    if(!make_skin_palette()) {
        error_=L"Could not create DX12 skinned-avatar palette";
        return false;
    }
    ready_=true;
    return true;
}

void Renderer::wait_for_gpu() noexcept {
    if(!fence_ || !fence_event_ || !device_.graphics_queue()) return;
    if(FAILED(device_.graphics_queue()->Signal(fence_.Get(),++fence_counter_))) return;
    if(fence_->GetCompletedValue()<fence_counter_) {
        if(SUCCEEDED(fence_->SetEventOnCompletion(fence_counter_,fence_event_)))
            (void)WaitForSingleObject(fence_event_,5000);
    }
}

void Renderer::resize(UINT width,UINT height) {
    if(!width || !height) return;
    client_width_=width;
    client_height_=height;
    const UINT target_width=scaled(width,graphics_.render_scale_percent);
    const UINT target_height=scaled(height,graphics_.render_scale_percent);
    if(!ready_ || (target_width==width_ && target_height==height_)) return;
    wait_for_gpu();
    for(auto& buffer:buffers_) buffer.Reset();
    depth_.Reset();
    if(FAILED(swapchain_->ResizeBuffers(kBuffers,target_width,target_height,
                kColorFormat,allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0))) {
        ready_=false;
        error_=L"Failed to resize DirectX 12 render scale";
        return;
    }
    width_=target_width;
    height_=target_height;
    if(!make_targets() || !make_depth()) {
        ready_=false;
        error_=L"Failed to recreate DirectX 12 attachments";
    }
}

void Renderer::set_graphics(graphics::Settings settings) {
    settings=graphics::normalize(settings);
    const bool detail_changed=graphics_.preset!=settings.preset;
    const bool scale_changed=graphics_.render_scale_percent!=settings.render_scale_percent;
    graphics_=settings;
    if(!ready_) return;
    if(detail_changed) {
        wait_for_gpu();
        if(!make_geometry()) {
            ready_=false;
            error_=L"Cannot rebuild terrain for selected graphics preset";
            return;
        }
    }
    if(scale_changed) resize(client_width_,client_height_);
}

void Renderer::reset_avatar() noexcept {
    avatar_.reset(sample_test_terrain_height);
    rig_physics_.reset();
    camera_.set_subject({avatar_.state().x,avatar_.state().y,avatar_.state().z},
                        avatar_.state().heading);
    camera_.reset();
}
void Renderer::orbit(float yaw_delta,float elevation_delta) {
    camera_.orbit(yaw_delta,elevation_delta);
}
void Renderer::zoom(float distance_delta) {
    camera_.zoom(distance_delta);
}

bool Renderer::set_creator_preview(SceneMesh mesh) {
    if(mesh.vertices.empty() ||
       mesh.terrain_count+mesh.avatar_count!=mesh.vertices.size()) {
        error_=L"OBJ preview has an invalid triangle section layout";
        return false;
    }
    if(mesh.vertices.size()>creator::kMaxPreviewVertices) {
        error_=L"OBJ too dense for KAMI preview; reduce polygons in Blender";
        return false;
    }
    const auto audited=rig::inspect(mesh.vertices);
    if(audited.status==rig::Compatibility::Invalid) {
        error_=L"OBJ contains non-finite geometry or invalid rig data";
        return false;
    }
    if(ready_)wait_for_gpu();
    auto previous=std::move(preview_mesh_);
    preview_mesh_=std::move(mesh);
    if(ready_ && !make_geometry()) {
        // make_geometry is transactional: old GPU geometry remains valid
        // even after allocation/map failure; no risky second upload.
        preview_mesh_=std::move(previous);
        error_=L"DX12 could not allocate OBJ preview buffer; try a lighter mesh";
        return false;
    }
    // Center the normalized OBJ (0..3.2 m tall) regardless of head tracking.
    const float head=camera_.settings().head_tracking?2.77f:1.7f;
    camera_.set_subject({0.f,1.6f-head,0.f});
    camera_.reset();
    error_.clear();
    return true;
}
bool Renderer::set_verified_live_terrain(
    const protocol::sim::Circuit& circuit) {
    const auto& region=circuit.region_metadata();
    const auto& agent=circuit.movement_position();
    const auto& patches=circuit.decoded_land_patches();
    if(circuit.state()!=protocol::sim::CircuitState::MovementConfirmed||
       !region.verified||!agent.verified||patches.empty()){
        error_=L"R12 terrain needs verified SL simulator and decoded land";
        return false;
    }
    // Work in local meters relative to the verified avatar, mapping
    // the simulator's Z-up coordinate system to DX12's Y-up scene.
    std::vector<const protocol::terrain::Patch*> nearest;
    nearest.reserve(patches.size());
    for(const auto& entry:patches){
        const auto& patch=entry.second;
        if(patch.size!=16U&&patch.size!=32U)continue;
        nearest.push_back(&patch);
    }
    const auto patch_nearer=[&](const auto* a,const auto* b){
        auto d=[&](const auto* p){
            const float cx=static_cast<float>(p->x)*p->size+
                           p->size*.5f-agent.x;
            const float cy=static_cast<float>(p->y)*p->size+
                           p->size*.5f-agent.y;
            return cx*cx+cy*cy;
        };
        return d(a)<d(b);
    };
    // We draw only 64 land patches. On a full region, partial_sort
    // avoids sorting the many offscreen patches that won't reach DX12.
    if(nearest.size()>64U){
        std::partial_sort(nearest.begin(),nearest.begin()+64U,
                          nearest.end(),patch_nearer);
        nearest.resize(64U);
    }else std::sort(nearest.begin(),nearest.end(),patch_nearer);
    if(!live_avatar_preview_.active()&&!live_avatar_preview_.reset(agent)){
        error_=L"R16 avatar marker needs verified simulator spawn";
        return false;
    }
    // The first land upload also needs to reframe the offline camera.
    // Without this, a user can receive dozens of valid height patches
    // and still see only sky because the camera is looking over them.
    const bool first_live_land=!region_mesh_;
    // Stitch patch edges to the NEXT patch's first height sample.
    // R12 drew only 15x15 quads from each 16x16 sample patch, leaving
    // one-meter seams that appeared as black grid lines in the live view.
    auto built=build_live_terrain_mesh(patches,nearest,
                            agent.x,agent.y,agent.z,region.water_height);
    SceneMesh scene=std::move(built.scene);
    // The flat water surface sits at the authenticated region water
    // height, with procedural blue styling (not an SL texture).
    const auto water=build_live_masked_water(
        patches,nearest,agent.x,agent.y,agent.z,region.water_height);
    const std::size_t new_water_count=water.scene.vertices.size();
    scene.vertices.insert(scene.vertices.end(),
                          water.scene.vertices.begin(),
                          water.scene.vertices.end());
    scene.terrain_count+=new_water_count;
    // Draw only server-sourced spatial root *bounding frames* in amber.
    // They are not prim meshes: profiles, rotations, textures and
    // hierarchies have NOT been decoded. The same DX12 geometry buffer
    // holds land + spatial proxy frames + local avatar.
    // Keep the old amber outlines for unknown/more complex roots;
    // supported simple primitives get actual finite solid geometry.
    std::map<std::uint32_t,protocol::objectspatial::Transform> bounds;
    for(const auto& [id,root]:circuit.decoded_spatial_objects()){
        if(root.primitive==protocol::objectspatial::PrimitiveKind::Unknown)
            bounds.emplace(id,root);
    }
    const auto proxies=build_live_object_spatial_proxies(
        bounds,agent.x,agent.y,agent.z);
    const auto new_proxy_count=proxies.vertices.size()/432U;
    scene.vertices.insert(scene.vertices.end(),
                          proxies.vertices.begin(),proxies.vertices.end());
    scene.terrain_count+=proxies.vertices.size();
    // R26 renders only verified root-space or correctly composed
    // linked shapes. Unknown mesh/sculpt/cut/hollow content remains
    // unrendered, not replaced with incorrect speculative geometry.
    // R49: the first 24 UUIDs in a std::map are not necessarily the
    // textures of nearby buildings. Select only decoded mesh-face assets
    // that can actually draw near the avatar, distance first; keep
    // existing array slots stable when the visible set is unchanged.
    const auto next_layers=select_visible_mesh_texture_layers(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        circuit.known_avatar_roots(),
        circuit.decoded_mesh_assets(),
        circuit.decoded_texture_assets(),
        agent.x,agent.y,agent.z,kLiveTextureLayers,live_texture_layers_);
    texture_upload_pending_=false;
    if(next_layers!=live_texture_layers_){
        auto previous=std::move(live_texture_layers_);
        live_texture_layers_=std::move(next_layers);
        if(ready_){
            // Previous DX12 upload may still be executing. A busy copy
            // allocator is retryable: keep the old GPU atlas and old
            // texture mappings instead of inventing a fatal renderer error.
            const auto outcome=make_material_texture(
                &circuit.decoded_texture_assets(),&previous);
            if(outcome!=TextureUploadStatus::Committed){
                live_texture_layers_=std::move(previous);
                if(outcome==TextureUploadStatus::Deferred)
                    texture_upload_pending_=true;
                else{
                    error_=L"DX12 could not stage decoded SL texture array";
                    return false;
                }
            }
        }
    }
    // Build the prim geometry AFTER GPU image residency is chosen, so
    // each real face has the appropriate DX12 texture array layer.
    const auto primitives=build_live_primitive_geometry(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        agent.x,agent.y,agent.z,128U,circuit.known_avatar_roots(),
        live_texture_layers_);
    const auto new_primitive_vertices=primitives.scene.vertices.size();
    scene.vertices.insert(scene.vertices.end(),
                          primitives.scene.vertices.begin(),
                          primitives.scene.vertices.end());
    scene.terrain_count+=new_primitive_vertices;
    // Real mesh asset triangles, decoded off-thread and validated against
    // the session's authenticated object roots/child hierarchy.
    const auto mesh_objects=build_live_mesh_geometry(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        circuit.known_avatar_roots(),
        circuit.decoded_mesh_assets(),
        agent.x,agent.y,agent.z,18000U,live_texture_layers_);
    const auto new_mesh_vertices=mesh_objects.scene.vertices.size();
    scene.vertices.insert(scene.vertices.end(),
        mesh_objects.scene.vertices.begin(),mesh_objects.scene.vertices.end());
    scene.terrain_count+=new_mesh_vertices;
    // Actual SL land stays in the terrain section. Append only an
    // obvious generic local placeholder in the separate avatar section.
    if(live_avatar_preview_.active()){
        const auto marker=build_live_avatar_placeholder();
        scene.avatar_count=marker.avatar_count;
        scene.vertices.insert(scene.vertices.end(),
                              marker.vertices.begin(),marker.vertices.end());
    }
    if(scene.vertices.empty()||scene.vertices.size()>750000U){
        error_=L"R13 decoded land has no contiguous terrain mesh";
        return false;
    }
    // The DX12 camera and local avatar marker use the original simulator
    // spawn. Every subsequent interest refresh decodes geometry relative
    // to the *current* agent position. Re-align only world vertices to
    // the stable spawn origin before comparing/uploading GPU geometry.
    const std::array<float,3> current_origin{agent.x,agent.y,agent.z};
    const auto stable_origin=live_region_origin_valid_?
        live_region_origin_:current_origin;
    if(!align_world_vertices_to_origin(scene,current_origin,stable_origin)){
        error_=L"R52 invalid live region coordinate rebase";
        return false;
    }
    // Revisions include frequent simulator interest-list packets and
    // decoded images which may never become drawable. Build a finite
    // candidate once, but skip the DX12 vertex buffer allocation, copy,
    // and associated GPU synchronization if its actual content is equal.
    // This also preserves the previous known-good resource on failure.
    const bool unchanged=region_mesh_&&same_scene_geometry(*region_mesh_,scene);
    if(!unchanged){
        auto previous=std::move(region_mesh_);
        region_mesh_=std::move(scene);
        // draw() already synchronizes the previous frame. New buffers are
        // immutable upload resources: replacing the pointer does not
        // require waiting for the independent texture-copy queue.
        if(ready_&&!make_geometry()){
            region_mesh_=std::move(previous);
            error_=L"R12 DX12 land mesh upload failed";
            return false;
        }
    }
    if(!live_region_origin_valid_){
        live_region_origin_=stable_origin;
        live_region_origin_valid_=true;
    }
    uploaded_root_bounds_=new_proxy_count;
    uploaded_root_primitive_shapes_=primitives.root_shapes;
    uploaded_linked_primitive_shapes_=primitives.linked_shapes;
    uploaded_primitive_vertices_=new_primitive_vertices;
    uploaded_mesh_vertices_=new_mesh_vertices;
    uploaded_mesh_shapes_=mesh_objects.root_meshes+mesh_objects.linked_meshes;
    uploaded_mesh_triangles_=mesh_objects.emitted_triangles;
    uploaded_textured_triangles_=mesh_objects.textured_triangles+
                                 primitives.textured_triangles;
    primitive_eligible_=primitives.eligible_shapes;
    primitive_unresolved_=primitives.unresolved_parent_shapes;
    primitive_missing_anchor_=primitives.missing_anchor_shapes;
    primitive_avatar_parent_=primitives.avatar_attachment_shapes;
    primitive_cycle_=primitives.cycle_shapes;
    primitive_unique_missing_=primitives.unique_missing_ids;
    primitive_distant_=primitives.out_of_range_shapes;
    primitive_deferred_=primitives.budget_deferred_shapes;
    uploaded_water_vertices_=new_water_count;
    skipped_terrain_discontinuities_=built.skipped_discontinuity_quads;
    if(!nearest.empty()){
        const auto& patch=*nearest.front();
        const unsigned middle=patch.size/2U;
        const float ground=patch.elevation[middle*patch.size+middle]-
                           stable_origin[2];
        const float x=static_cast<float>(patch.x)*patch.size+middle-
                      stable_origin[0];
        const float z=static_cast<float>(patch.y)*patch.size+middle-
                      stable_origin[1];
        // Anchor near a real decoded land patch, even if the avatar is
        // over water or the nearest patch is beyond the 200m far plane.
        // System::update adds the 2.77m head offset when head-tracking
        // is enabled, so lowering the subject centers nearby terrain.
        if(std::isfinite(x)&&std::isfinite(z)&&std::isfinite(ground)){
            // Height is actual decoded land. Camera::update adds 2.77m
            // to the target, so focus precisely on this land point.
            landscape_anchor_={x,ground-2.77f,z};
            landscape_anchor_valid_=true;
            if(first_live_land){
                camera_.set_subject({x,ground-1.7f,z});
                camera_.apply_studio_shot(camera::StudioShot::Wide);
            }
        }
    }
    error_.clear();
    return true;
}
bool Renderer::set_verified_live_water(
    const protocol::sim::Circuit& circuit) {
    const auto& region=circuit.region_metadata();
    const auto& agent=circuit.movement_position();
    if(circuit.state()!=protocol::sim::CircuitState::MovementConfirmed||
       !circuit.region_seen()||!circuit.movement_seen()||
       !region.verified||!agent.verified) {
        error_=L"R9 water preview requires authenticated simulator position";
        return false;
    }
    // Real simulator-reported elevations: SL Z-up maps to KAMI Y-up,
    // local origin anchored to the authenticated agent position.
    const float water_relative=region.water_height-agent.z;
    SceneMesh surface=build_live_water_preview(water_relative);
    if(surface.vertices.size()!=6U){
        error_=L"Invalid R9 simulator water surface elevation";
        return false;
    }
    if(ready_)wait_for_gpu();
    auto old=std::move(live_water_mesh_);
    live_water_mesh_=std::move(surface);
    if(ready_&&!make_geometry()){
        live_water_mesh_=std::move(old);
        error_=L"DX12 could not upload verified SL water-height preview";
        return false;
    }
    error_.clear();
    return true;
}
void Renderer::clear_live_water_preview() {
    if(!live_water_mesh_)return;
    if(ready_)wait_for_gpu();
    live_water_mesh_.reset();
    if(ready_&&!make_geometry()){
        ready_=false;
        error_=L"DX12 could not restore scene after live water disconnect";
    }
}
bool Renderer::set_decoded_region_geometry(
    const protocol::Bootstrap& authenticated_region,
    SceneMesh decoded_static_geometry,bool verified_region_data) {
    const auto issue=validate_decoded_region(
        authenticated_region,decoded_static_geometry,verified_region_data);
    if(issue!=RegionGeometryError::None) {
        error_=region_geometry_error(issue);
        return false;
    }
    if(ready_)wait_for_gpu();
    auto prior=std::move(region_mesh_);
    region_mesh_=std::move(decoded_static_geometry);
    if(ready_&&!make_geometry()) {
        region_mesh_=std::move(prior);
        error_=L"DX12 could not allocate decoded region geometry";
        return false;
    }
    error_.clear();
    return true;
}
void Renderer::clear_decoded_region_geometry() {
    if(!region_mesh_)return;
    if(ready_)wait_for_gpu();
    // Clear server data immediately on disconnect. If restoring the local
    // scene fails, stop rendering rather than showing stale world geometry.
    region_mesh_.reset();
    uploaded_root_bounds_=0;
    uploaded_root_primitive_shapes_=0;
    uploaded_linked_primitive_shapes_=0;
    uploaded_primitive_vertices_=0;
    uploaded_textured_triangles_=0;
    live_texture_layers_.clear();
    texture_upload_pending_=false;
    live_region_origin_={};
    live_region_origin_valid_=false;
    primitive_eligible_=0;
    primitive_unresolved_=0;
    primitive_distant_=0;
    primitive_deferred_=0;
    uploaded_water_vertices_=0;
    skipped_terrain_discontinuities_=0;
    landscape_view_=false;
    landscape_anchor_valid_=false;
    camera_.set_scenic_overlook(false);
    live_avatar_preview_.clear();
    live_preview_controls_={};
    camera_.set_subject({0.f,0.f,0.f});
    camera_.reset();
    if(ready_&&!make_geometry()) {
        ready_=false;
        error_=L"DX12 could not restore offline scene on disconnect";
    }
}
void Renderer::clear_creator_preview() {
    if(!preview_mesh_)return;
    if(ready_)wait_for_gpu();
    preview_mesh_.reset();
    if(ready_ && !make_geometry()) {
        ready_=false;error_=L"Could not restore original procedural scene";
    }
    camera_.set_subject({avatar_.state().x,avatar_.state().y,avatar_.state().z},
                        avatar_.state().heading);
    camera_.reset();
}
bool Renderer::capture_png(std::wstring destination) {
    if(!ready_||destination.empty()||capture_status_==CaptureStatus::Pending)
        return false;
    capture_path_=std::move(destination);
    capture_status_=CaptureStatus::Pending;
    return true;
}
void Renderer::toggle_landscape_view() noexcept {
    if(!region_mesh_||!landscape_anchor_valid_)return;
    landscape_view_=!landscape_view_;
    camera_.set_scenic_overlook(landscape_view_);
}
void Renderer::draw() {
    if(!ready_ || !width_ || !height_) return;
    if(FAILED(allocator_->Reset()) ||
       FAILED(commands_->Reset(allocator_.Get(),pipeline_.Get()))) return;

    using namespace DirectX;
    const auto now=std::chrono::steady_clock::now();
    const float dt=last_camera_frame_==std::chrono::steady_clock::time_point{} ?
        0.f : std::chrono::duration<float>(now-last_camera_frame_).count();
    last_camera_frame_=now;
    const float seconds=std::chrono::duration<float>(now-start_time_).count();
    if(!preview_mesh_&&!region_mesh_) {
        avatar_.step(dt,sample_test_terrain_height);
        rig_physics_.step(avatar_.state(),dt);
        if(!rig::make_palette(rigphysics::build_pose(rig_physics_.output()),
                              skin_matrices_)||!update_skin_palette()) {
            error_=L"Could not update DX12 avatar skin palette";
            ready_=false;
            return;
        }
        const auto& state=avatar_.state();
        camera_.set_subject({state.x,state.y,state.z},state.heading);
    }
    const bool preview_visible=region_mesh_&&!preview_mesh_&&
        live_avatar_preview_.active();
    if(preview_visible){
        live_avatar_preview_.step(dt,live_preview_controls_);
        const auto& marker=live_avatar_preview_.pose();
        // Marker follows LOCAL key prediction based on the verified initial
        // spawn. We still require real own-avatar ObjectUpdate replication.
        if(landscape_view_&&landscape_anchor_valid_)
            camera_.set_subject(landscape_anchor_,0.f);
        else
            camera_.set_subject({marker.x,marker.y-1.f,marker.z},
                                marker.heading);
    }
    camera_.set_live_avatar_follow(
        preview_visible&&!landscape_view_,preview_visible&&
        !landscape_view_&&
        std::fabs(live_preview_controls_.turn)>.01f);
    avatar_spring_.step(std::sin(seconds*2.f),dt);
    // Live SL terrain must not collide against the procedural offline
    // scene's unrelated height sampler. A live collision sampler will be
    // connected after the region height grid is fully assembled.
    camera_.update(dt,seconds,
                   region_mesh_?nullptr:sample_test_terrain_height,
                   graphics_.animate_avatar);
    const auto camera=camera_.pose();
    const XMVECTOR eye=XMVectorSet(camera.eye.x,camera.eye.y,camera.eye.z,0.f);
    const XMVECTOR focus=XMVectorSet(camera.focus.x,camera.focus.y,camera.focus.z,0.f);
    const XMMATRIX view=XMMatrixLookAtLH(
        eye,focus,XMVectorSet(0,1,0,0));
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(
        XMConvertToRadians(camera.fov_y_degrees),
        static_cast<float>(width_)/height_,.08f,200.f);
    const XMVECTOR sunOrigin=XMVectorScale(
        XMVector3Normalize(XMVectorSet(-.35f,.86f,-.38f,0.f)),27.f);
    const XMMATRIX lightView=XMMatrixLookAtLH(
        sunOrigin,XMVectorSet(0,0,0,1),XMVectorSet(0,1,0,0));
    const XMMATRIX lightProjection=XMMatrixOrthographicLH(
        40.f,40.f,.1f,80.f);

    XMFLOAT4X4 viewproj{},lightproj{};
    XMStoreFloat4x4(&viewproj,view*projection);
    XMStoreFloat4x4(&lightproj,lightView*lightProjection);
    std::array<float,63> constants{};
    std::memcpy(constants.data(),&viewproj,sizeof(viewproj));
    constants[16]=.35f;
    constants[17]=-.86f;
    constants[18]=.38f;
    constants[19]=graphics_.sun_strength();
    constants[20]=XMVectorGetX(eye);
    constants[21]=XMVectorGetY(eye);
    constants[22]=XMVectorGetZ(eye);
    constants[23]=1.f;
    const auto elapsed=std::chrono::steady_clock::now()-start_time_;
    constants[24]=std::chrono::duration<float>(elapsed).count();
    constants[25]=0.f;
    constants[26]=graphics_.pbr?1.f:0.f;
    constants[27]=graphics_.shadows?1.f:0.f;
    std::memcpy(constants.data()+28,&lightproj,sizeof(lightproj));
    constants[44]=graphics_.atmosphere_haze?1.f:0.f;
    constants[45]=graphics_.contact_ambient?1.f:0.f;
    constants[46]=graphics_.diamond_glints?1.f:0.f;
    constants[47]=0.f;
    constants[48]=static_cast<float>(photo_.exposure_tenths)/10.f;
    constants[49]=static_cast<float>(photo_.contrast_percent)/100.f;
    constants[50]=static_cast<float>(photo_.saturation_percent)/100.f;
    constants[51]=static_cast<float>(photo_.vignette_percent)/100.f;
    constants[52]=static_cast<float>(width_);
    constants[53]=static_cast<float>(height_);
    constants[54]=photo_.enabled?1.f:0.f;
    constants[55]=0.f;
    // The final four 32-bit root constants are an independent local-avatar
    // transform. Total 60 DWORD constants plus one SRV descriptor table remain
    // below the 64 DWORD DX12 root signature limit.
    constants[56]=0.f;constants[57]=0.f;
    constants[58]=0.f;constants[59]=0.f;
    constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
    const auto apply_avatar_marker=[&](){
        if(!preview_visible)return;
        const auto& p=live_avatar_preview_.pose();
        constants[25]=0.f;
        constants[47]=seconds*4.f;
        constants[55]=p.moving&&graphics_.animate_avatar?.65f:0.f;
        constants[56]=p.x;
        constants[57]=p.y;
        constants[58]=p.z;
        constants[59]=p.heading;
        constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
    };

    commands_->SetGraphicsRootSignature(signature_.Get());
    ID3D12DescriptorHeap* resources[]={shadow_srv_heap_.Get()};
    commands_->SetDescriptorHeaps(1,resources);
    commands_->SetGraphicsRootDescriptorTable(
        1,shadow_srv_heap_->GetGPUDescriptorHandleForHeapStart());
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands_->IASetVertexBuffers(0,1,&vertex_view_);

    // The depth-only pass and lit pass share the same geometry and original
    // GPU animation clock, ensuring the moving mannequin casts a matching
    // shadow. Skipping this pass when disabled saves the rendering workload.
    if(graphics_.shadows) {
        auto to_depth=barrier(shadow_map_.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE);
        commands_->ResourceBarrier(1,&to_depth);
        const auto shadowHandle=shadow_dsv();
        commands_->OMSetRenderTargets(0,nullptr,FALSE,&shadowHandle);
        commands_->ClearDepthStencilView(
            shadowHandle,D3D12_CLEAR_FLAG_DEPTH,1.f,0,0,nullptr);
        const D3D12_VIEWPORT shadowView{
            0.f,0.f,static_cast<float>(kShadowSize),
            static_cast<float>(kShadowSize),0.f,1.f
        };
        const D3D12_RECT shadowRect{
            0,0,static_cast<LONG>(kShadowSize),
            static_cast<LONG>(kShadowSize)
        };
        commands_->RSSetViewports(1,&shadowView);
        commands_->RSSetScissorRects(1,&shadowRect);
        commands_->SetPipelineState(shadow_pipeline_.Get());
        commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
        commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1,0,0);
        if(!preview_mesh_&&!region_mesh_) {
            const auto& a=avatar_.state();
            constants[25]=graphics_.animate_avatar && a.motion==avatar::Motion::Idle?
                .025f*avatar_spring_.position():0.f;
            constants[47]=a.animation_phase;
            constants[55]=graphics_.animate_avatar?a.gait:0.f;
            constants[56]=a.x;constants[57]=a.y;
            constants[58]=a.z;constants[59]=a.heading;
            // Rigged fitted-mesh surfaces use the full six-axis GPU skin
            // palette; keep these old marker offsets at zero to avoid
            // applying the deformation twice.
            constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
        }
        apply_avatar_marker();
        commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
        commands_->DrawInstanced(static_cast<UINT>(avatar_count_),1,
                                static_cast<UINT>(terrain_count_),0);
        constants[25]=0.f;constants[47]=0.f;constants[55]=0.f;
        constants[56]=0.f;constants[57]=0.f;
        constants[58]=0.f;constants[59]=0.f;
        constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
        auto to_srv=barrier(shadow_map_.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commands_->ResourceBarrier(1,&to_srv);
    }

    const UINT index=swapchain_->GetCurrentBackBufferIndex();
    auto begin=barrier(buffers_[index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands_->ResourceBarrier(1,&begin);
    const auto rtv=rtv_handle(index);
    const auto dsv=dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    constexpr float background[]={.055f,.09f,.15f,1.f};
    commands_->OMSetRenderTargets(1,&rtv,FALSE,&dsv);
    commands_->ClearRenderTargetView(rtv,background,0,nullptr);
    commands_->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1.f,0,0,nullptr);
    const D3D12_VIEWPORT viewport{
        0.f,0.f,static_cast<float>(width_),static_cast<float>(height_),0.f,1.f
    };
    const D3D12_RECT scissor{
        0,0,static_cast<LONG>(width_),static_cast<LONG>(height_)
    };
    commands_->RSSetViewports(1,&viewport);
    commands_->RSSetScissorRects(1,&scissor);
    // Draw atmosphere behind authenticated SL terrain, just one
    // fullscreen triangle with no depth writes or scene vertices.
    if(region_mesh_&&!preview_mesh_&&sky_pipeline_) {
        commands_->SetPipelineState(sky_pipeline_.Get());
        commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
        commands_->DrawInstanced(3,1,0,0);
    }
    commands_->SetPipelineState(pipeline_.Get());
    ID3D12DescriptorHeap* heaps[]={shadow_srv_heap_.Get()};
    commands_->SetDescriptorHeaps(1,heaps);
    commands_->SetGraphicsRootDescriptorTable(
        1,shadow_srv_heap_->GetGPUDescriptorHandleForHeapStart());
    commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
    commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1,0,0);
    if(!preview_mesh_&&!region_mesh_) {
        const auto& a=avatar_.state();
        constants[25]=graphics_.animate_avatar && a.motion==avatar::Motion::Idle?
            .025f*avatar_spring_.position():0.f;
        constants[47]=a.animation_phase;
        constants[55]=graphics_.animate_avatar?a.gait:0.f;
        constants[56]=a.x;constants[57]=a.y;
        constants[58]=a.z;constants[59]=a.heading;
            // Rigged fitted-mesh surfaces use the full six-axis GPU skin
            // palette; keep these old marker offsets at zero to avoid
            // applying the deformation twice.
            constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
    }
    apply_avatar_marker();
    commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
    commands_->DrawInstanced(static_cast<UINT>(avatar_count_),1,
                            static_cast<UINT>(terrain_count_),0);
    Microsoft::WRL::ComPtr<ID3D12Resource> photo_readback;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT photo_layout{};
    UINT64 photo_bytes=0;
    bool copying_photo=false;
    if(capture_status_==CaptureStatus::Pending) {
        if(width_>8192||height_>8192) {
            capture_status_=CaptureStatus::Failed;
        } else {
            const auto desc=buffers_[index]->GetDesc();
            UINT rows=0;
            UINT64 row_bytes=0;
            device_.native_device()->GetCopyableFootprints(
                &desc,0,1,0,&photo_layout,&rows,&row_bytes,&photo_bytes);
            const auto properties=heap(D3D12_HEAP_TYPE_READBACK);
            const auto allocation=buffer_desc(photo_bytes);
            if(photo_bytes!=0 && photo_bytes<=256ULL*1024ULL*1024ULL &&
               SUCCEEDED(device_.native_device()->CreateCommittedResource(
                   &properties,D3D12_HEAP_FLAG_NONE,&allocation,
                   D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
                   IID_PPV_ARGS(photo_readback.GetAddressOf())))) {
                const auto to_copy=barrier(buffers_[index].Get(),
                    D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
                commands_->ResourceBarrier(1,&to_copy);
                D3D12_TEXTURE_COPY_LOCATION source{};
                source.pResource=buffers_[index].Get();
                source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                source.SubresourceIndex=0;
                D3D12_TEXTURE_COPY_LOCATION destination{};
                destination.pResource=photo_readback.Get();
                destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                destination.PlacedFootprint=photo_layout;
                commands_->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
                const auto present=barrier(buffers_[index].Get(),
                    D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
                commands_->ResourceBarrier(1,&present);
                copying_photo=true;
            } else capture_status_=CaptureStatus::Failed;
        }
    }
    if(!copying_photo) {
        const auto end=barrier(buffers_[index].Get(),
            D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
        commands_->ResourceBarrier(1,&end);
    }
    if(FAILED(commands_->Close())) {
        if(copying_photo)capture_status_=CaptureStatus::Failed;
        return;
    }
    ID3D12CommandList* list[]={commands_.Get()};
    device_.graphics_queue()->ExecuteCommandLists(1,list);
    const UINT sync=graphics_.vsync?1U:0U;
    const UINT flags=(!graphics_.vsync && allow_tearing_)?
        DXGI_PRESENT_ALLOW_TEARING:0U;
    if(FAILED(swapchain_->Present(sync,flags))) {
        error_=L"DX12 presentation failed";
        ready_=false;
        if(copying_photo)capture_status_=CaptureStatus::Failed;
        return;
    }
    // The usual frame fence also completes any earlier texture copy
    // submitted on the same ordered queue. Safely free its staging heap
    // after completion; never before the GPU reads it.
    wait_for_gpu();
    if(texture_copy_submitted_fence_!=0U&&fence_&&
       fence_->GetCompletedValue()>=texture_copy_submitted_fence_){
        material_upload_.Reset();
        texture_copy_submitted_fence_=0U;
    }
    if(copying_photo) {
        bool saved=false;
        BYTE* mapped=nullptr;
        const D3D12_RANGE to_read{0,static_cast<SIZE_T>(photo_bytes)};
        if(SUCCEEDED(photo_readback->Map(
                0,&to_read,reinterpret_cast<void**>(&mapped))) && mapped) {
            const std::size_t stride=static_cast<std::size_t>(width_)*4;
            std::vector<std::uint8_t> bgra(
                stride*static_cast<std::size_t>(height_));
            for(UINT y=0;y<height_;++y) {
                const BYTE* row=mapped+
                    static_cast<std::size_t>(y)*photo_layout.Footprint.RowPitch;
                std::uint8_t* dst=bgra.data()+static_cast<std::size_t>(y)*stride;
                for(UINT x=0;x<width_;++x) {
                    dst[x*4+0]=row[x*4+2];
                    dst[x*4+1]=row[x*4+1];
                    dst[x*4+2]=row[x*4+0];
                    dst[x*4+3]=row[x*4+3];
                }
            }
            photo_readback->Unmap(0,nullptr);
            saved=photo::write_bgra_png(capture_path_,bgra,width_,height_);
        }
        capture_status_=saved?CaptureStatus::Saved:CaptureStatus::Failed;
    }
}
void Renderer::shutdown() noexcept {
    if(device_.graphics_queue()) wait_for_gpu();
    ready_=false;
    if(fence_event_) { CloseHandle(fence_event_); fence_event_=nullptr; }
    depth_.Reset();
    shadow_map_.Reset();
    material_texture_.Reset();
    material_upload_.Reset();
    skin_palette_buffer_.Reset();
    for(auto& buffer:buffers_) buffer.Reset();
    swapchain_.Reset();
    geometry_.Reset();
    commands_.Reset();
    allocator_.Reset();
    texture_copy_commands_.Reset();
    texture_copy_allocator_.Reset();
    texture_copy_submitted_fence_=0U;
    live_region_origin_={};
    live_region_origin_valid_=false;
    texture_upload_pending_=false;
    pipeline_.Reset();
    shadow_pipeline_.Reset();
    sky_pipeline_.Reset();
    signature_.Reset();
    fence_.Reset();
    rtv_heap_.Reset();
    dsv_heap_.Reset();
    shadow_srv_heap_.Reset();
}
} // namespace kami::world
