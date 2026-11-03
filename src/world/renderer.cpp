#include "kami/world/renderer.hpp"
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



static_assert(sizeof(Vertex)==64U,
              "KAMI GPU vertex input layout must match 64-byte packed vertices");
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

bool Renderer::make_material_texture() {
    // First GPU image is a KAMI-authored procedural 64x64 albedo texture.
    // This creates the real DX12 upload + shader-resource path required
    // later for independently decoded external assets.
    constexpr UINT tw=64,th=64;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width=tw;
    desc.Height=th;
    desc.DepthOrArraySize=1;
    desc.MipLevels=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;
    desc.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;

    const auto heap_default=heap(D3D12_HEAP_TYPE_DEFAULT);
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &heap_default,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
            IID_PPV_ARGS(material_texture_.ReleaseAndGetAddressOf()))))
        return false;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0;
    UINT64 rowBytes=0,uploadSize=0;
    device_.native_device()->GetCopyableFootprints(
        &desc,0,1,0,&footprint,&rows,&rowBytes,&uploadSize);
    if(rows!=th || rowBytes!=tw*4U || uploadSize==0) return false;
    const auto upload_desc=buffer_desc(uploadSize);
    const auto heap_upload=heap(D3D12_HEAP_TYPE_UPLOAD);
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &heap_upload,D3D12_HEAP_FLAG_NONE,&upload_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
            IID_PPV_ARGS(material_upload_.ReleaseAndGetAddressOf()))))
        return false;

    void* mapped=nullptr;
    if(FAILED(material_upload_->Map(0,nullptr,&mapped))) return false;
    std::memset(mapped,0,static_cast<std::size_t>(uploadSize));
    auto* pixels=static_cast<std::uint8_t*>(mapped);
    for(UINT y=0;y<th;++y) for(UINT x=0;x<tw;++x) {
        auto* pixel=pixels+static_cast<std::size_t>(y)*footprint.Footprint.RowPitch+
                     static_cast<std::size_t>(x)*4;
        const bool tile=((x/8U+y/8U)%2U)==0U;
        pixel[0]=tile?220:128;
        pixel[1]=tile?205:160;
        pixel[2]=tile?180:215;
        pixel[3]=255;
    }
    material_upload_->Unmap(0,nullptr);

    D3D12_TEXTURE_COPY_LOCATION dest{};
    dest.pResource=material_texture_.Get();
    dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dest.SubresourceIndex=0;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource=material_upload_.Get();
    source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint=footprint;
    if(FAILED(allocator_->Reset()) ||
       FAILED(commands_->Reset(allocator_.Get(),pipeline_.Get())))
        return false;
    commands_->CopyTextureRegion(&dest,0,0,0,&source,nullptr);
    const auto ready=barrier(material_texture_.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commands_->ResourceBarrier(1,&ready);
    if(FAILED(commands_->Close())) return false;
    ID3D12CommandList* list[]={commands_.Get()};
    device_.graphics_queue()->ExecuteCommandLists(1,list);
    wait_for_gpu();

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels=1;
    auto handle=shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=device_.native_device()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device_.native_device()->CreateShaderResourceView(
        material_texture_.Get(),&srv,handle);
    return true;
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
    return ShaderCache::prepare_shadow(
        device_.native_device(),signature_.Get(),shadow_pipeline_,shadow_report);
}

bool Renderer::make_geometry() {
    SceneMesh generated;
    if(!preview_mesh_&&!region_mesh_)
        generated=build_test_scene(graphics_.terrain_divisions());
    // Local creator preview temporarily overrides a decoded region scene.
    // Both static and animated scenes go through the SAME DX12 buffers,
    // shader stages, PBR and depth/shadow passes.
    const SceneMesh& scene=preview_mesh_?*preview_mesh_:
        region_mesh_?*region_mesh_:generated;
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
                          bool allow_software_dx12) {
    if(ready_) return true;
    if(!window || !width || !height) { error_=L"Invalid viewport dimensions"; return false; }
    window_=window;
    avatar_.reset(sample_test_terrain_height);
    rig_physics_.reset();
    client_width_=width;
    client_height_=height;
    width_=scaled(width,graphics_.render_scale_percent);
    height_=scaled(height,graphics_.render_scale_percent);
    if(device_.initialize(allow_software_dx12)==DeviceStatus::Unavailable) {
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
    if(!make_material_texture()) {
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
    avatar_spring_.step(std::sin(seconds*2.f),dt);
    camera_.update(dt,seconds,sample_test_terrain_height,graphics_.animate_avatar);
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
    // Safe single-frame synchronization; multiframe scheduling is still
    // pending performance measurements on a real GPU.
    wait_for_gpu();
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
    pipeline_.Reset();
    shadow_pipeline_.Reset();
    signature_.Reset();
    fence_.Reset();
    rtv_heap_.Reset();
    dsv_heap_.Reset();
    shadow_srv_heap_.Reset();
}
} // namespace kami::world
