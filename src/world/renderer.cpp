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
constexpr DXGI_FORMAT kColorFormat=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
constexpr DXGI_FORMAT kSwapchainFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
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



// Preserve the input ABI consumed by KAMI's existing DX12 terrain,
// shadow, avatar and PBR shaders when changing mesh admission/scheduling.
// A compatible stride alone does not catch reordered material/UV fields.
static_assert(sizeof(Vertex)==72U,
              "KAMI GPU vertex input layout must match 72-byte packed vertices");
static_assert(offsetof(Vertex,px)==0U && offsetof(Vertex,nx)==12U &&
              offsetof(Vertex,r)==24U && offsetof(Vertex,metallic)==36U &&
              offsetof(Vertex,u)==44U && offsetof(Vertex,body_region)==52U &&
              offsetof(Vertex,bone_indices)==56U &&
              offsetof(Vertex,bone_weights)==60U &&
              offsetof(Vertex,texture_layer)==64U &&
              offsetof(Vertex,pbr_material)==68U,
              "KAMI DX12 shader input ABI changed: update shader input layout and all passes together");
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


bool Renderer::make_material_records(const std::vector<PBRRow>& rows){
    if(rows.empty()||rows.size()>std::size_t(protocol::pbr::kMaxGPURecords+1U)*protocol::pbr::kRows)return false;
    const auto properties=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto desc=buffer_desc(rows.size()*sizeof(PBRRow));
    Microsoft::WRL::ComPtr<ID3D12Resource> candidate;
    if(FAILED(device_.native_device()->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,
        &desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(candidate.GetAddressOf()))))return false;
    void* data=nullptr;const D3D12_RANGE no_read{0U,0U};
    if(FAILED(candidate->Map(0U,&no_read,&data))||!data)return false;
    std::memcpy(data,rows.data(),rows.size()*sizeof(PBRRow));candidate->Unmap(0U,nullptr);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=DXGI_FORMAT_UNKNOWN;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.NumElements=static_cast<UINT>(rows.size());srv.Buffer.StructureByteStride=sizeof(PBRRow);
    auto handle=shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr+=3U*device_.native_device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device_.native_device()->CreateShaderResourceView(candidate.Get(),&srv,handle);
    material_records_=std::move(candidate);return true;
}
bool Renderer::sort_blended_geometry(float x,float y,float z){
    if(transparent_vertices_.empty())return true;
    std::vector<std::pair<float,std::size_t>> order;
    order.reserve(transparent_vertices_.size()/3U);
    for(std::size_t at=0U;at<transparent_vertices_.size();at+=3U){
        float cx=0.f,cy=0.f,cz=0.f;
        for(unsigned i=0;i<3U;++i){const auto& v=transparent_vertices_[at+i];cx+=v.px;cy+=v.py;cz+=v.pz;}
        cx=cx/3.f-x;cy=cy/3.f-y;cz=cz/3.f-z;
        order.emplace_back(cx*cx+cy*cy+cz*cz,at);
    }
    std::stable_sort(order.begin(),order.end(),[](const auto& a,const auto& b){return a.first>b.first;});
    void* data=nullptr;const D3D12_RANGE no_read{0U,0U};
    if(FAILED(geometry_->Map(0U,&no_read,&data))||!data)return false;
    auto* destination=static_cast<Vertex*>(data)+terrain_count_+avatar_count_;
    for(const auto& [distance,at]:order){(void)distance;
        std::memcpy(destination,transparent_vertices_.data()+at,3U*sizeof(Vertex));destination+=3U;}
    geometry_->Unmap(0U,nullptr);return true;
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
    desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;
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
       dirty.size()>protocol::texture::kMaxUploadBytes/one_size)return TextureUploadStatus::Failed;
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
        srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        handle.ptr+=3U*dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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
        D3D12_RENDER_TARGET_VIEW_DESC rtv{};rtv.Format=kColorFormat;
        rtv.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
        graphics->CreateRenderTargetView(buffers_[i].Get(),&rtv,rtv_handle(i));
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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> blend_signature;
    ShaderPreparation blend_report;
    if(!ShaderCache::prepare(device_.native_device(),blend_signature,blend_pipeline_,blend_report,{},true))return false;
    ShaderPreparation shadow_report;
    if(!ShaderCache::prepare_shadow(
        device_.native_device(),signature_.Get(),shadow_pipeline_,shadow_report))
        return false;
    ShaderPreparation sky_report;
    if(!ShaderCache::prepare_sky(device_.native_device(),signature_.Get(),sky_pipeline_,sky_report))return false;
    ShaderPreparation environment_report;
    return ShaderCache::prepare_environment(device_.native_device(),signature_.Get(),
        environment_capture_pipeline_,environment_sky_pipeline_,environment_filter_pipeline_,environment_report);
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
       scene.vertices.size()>std::numeric_limits<UINT>::max()/sizeof(Vertex)){
        error_=L"DX12 geometry vertex count invalid: "+std::to_wstring(scene.vertices.size());
        return false;
    }
    const auto audit=rig::inspect(scene.vertices);
    if(audit.status==rig::Compatibility::Invalid){
        error_=L"DX12 geometry rejected by rig audit: "+
            std::to_wstring(audit.corrupt_vertices)+L" invalid vertices / "+
            std::to_wstring(scene.vertices.size())+L" total";
        return false;
    }
    if(scene.terrain_count%3U!=0U||scene.avatar_count%3U!=0U||
       scene.terrain_count>scene.vertices.size()||
       scene.avatar_count>scene.vertices.size()-scene.terrain_count){
        error_=L"DX12 geometry section counts invalid";
        return false;
    }
    // A resident batch is authoritative only while every instance still
    // matches the accepted region revision. Otherwise the complete combined
    // scene remains the fallback.
    if(!preview_mesh_&&region_mesh_&&resident_mesh_batch_candidate_&&
       (!resident_mesh_batch_ready_||
        !mesh_instance_cache_.can_draw_exact_instance_batch(
            "__kami_live_region__",resident_mesh_revision_,
            resident_mesh_requirements_))){
        Dx12MeshBufferCache staged;
        staged.begin_generation();
        bool staged_ok=!resident_mesh_spans_.empty()&&
            resident_mesh_spans_.size()==resident_mesh_requirements_.size();
        for(const auto& span:resident_mesh_spans_){
            if(!staged.upload_instance(device_.native_device(),
                "__kami_live_region__",span.local_id,resident_mesh_revision_,
                scene.vertices.data(),scene.vertices.size(),
                span.first_vertex,span.vertex_count)){
                staged_ok=false;
                break;
            }
        }
        if(staged_ok&&staged.can_draw_exact_instance_batch(
            "__kami_live_region__",resident_mesh_revision_,
            resident_mesh_requirements_)){
            mesh_instance_cache_=std::move(staged);
            resident_mesh_batch_ready_=true;
        }else{
            resident_mesh_batch_ready_=false;
        }
    }
    bool use_resident_mesh_batch=!preview_mesh_&&region_mesh_&&
        resident_mesh_batch_candidate_&&resident_mesh_batch_ready_&&
        mesh_instance_cache_.can_draw_exact_instance_batch(
            "__kami_live_region__",resident_mesh_revision_,
            resident_mesh_requirements_);

    std::vector<Vertex> upload,transparent;
    upload.reserve(scene.vertices.size());
    for(std::size_t at=0U;at<scene.terrain_count;at+=3U){
        if(use_resident_mesh_batch&&
           Dx12MeshBufferCache::triangle_is_in_instance_spans(
               at,resident_mesh_spans_))continue;
        const auto index=scene.vertices[at].pbr_material;
        const auto row=std::size_t(index)*protocol::pbr::kRows+4U;
        const bool blend=index!=0U&&row<pbr_rows_.size()&&pbr_rows_[row][0]>1.5f;
        auto& target=blend?transparent:upload;
        target.insert(target.end(),scene.vertices.begin()+at,scene.vertices.begin()+at+3U);
    }
    // A scene consisting exclusively of resident meshes still needs a
    // non-empty combined resource for the established fallback path.
    if(use_resident_mesh_batch&&upload.empty()&&transparent.empty()&&
       scene.avatar_count==0U){
        upload.insert(upload.end(),scene.vertices.begin(),
                      scene.vertices.begin()+scene.terrain_count);
        resident_mesh_batch_candidate_=false;
        resident_mesh_batch_ready_=false;
        use_resident_mesh_batch=false;
    }
    const auto opaque_count=upload.size();
    upload.insert(upload.end(),scene.vertices.begin()+scene.terrain_count,scene.vertices.end());
    upload.insert(upload.end(),transparent.begin(),transparent.end());
    const UINT bytes=static_cast<UINT>(upload.size()*sizeof(Vertex));
    // R88 phase 1: the production DX12 scene now uses the persistent GPU
    // buffer manager. Preserve the existing vertex ABI and all shader passes.
    // Scene revisions are unique for each changed geometry upload.
    // The existing graphics fence protects in-place updates.
    ++scene_gpu_revision_;
    if(!mesh_gpu_cache_.upload(device_.native_device(),"__kami_scene__",
                               scene_gpu_revision_,upload.data(),bytes,
                               sizeof(Vertex))){
        error_=L"R88 persistent DX12 scene buffer upload failed";
        return false;
    }
    const auto* resident=mesh_gpu_cache_.find("__kami_scene__");
    if(!resident||!resident->resource)return false;
    geometry_=resident->resource;
    skin_audit_=audit;
    terrain_count_=opaque_count;
    transparent_vertices_=std::move(transparent);
    avatar_count_=scene.avatar_count;
    vertex_view_.BufferLocation=geometry_->GetGPUVirtualAddress();
    vertex_view_.SizeInBytes=bytes;
    vertex_view_.StrideInBytes=sizeof(Vertex);
    resident_mesh_batch_active_=use_resident_mesh_batch;
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
    swap.Format=kSwapchainFormat;
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
    srv.NumDescriptors=10;
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
    if(!make_material_records(pbr_rows_)){error_=L"Could not upload glTF material records";return false;}
    if(!make_skin_palette()) {
        error_=L"Could not create DX12 skinned-avatar palette";
        return false;
    }
    if(!make_environment()){error_=L"Could not create HDR PBR environment resources";return false;}
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
                kSwapchainFormat,allow_tearing_?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0))) {
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
    const bool has_spatial_content=
        !circuit.decoded_spatial_objects().empty()||
        !circuit.decoded_linked_objects().empty();
    if(circuit.state()!=protocol::sim::CircuitState::MovementConfirmed||
       !region.verified||!agent.verified||
       (patches.empty()&&!has_spatial_content)){
        error_=L"R12 live scene needs verified simulator data";
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
            const double cx=static_cast<double>(p->x)*p->size+
                            p->size*.5-static_cast<double>(agent.x);
            const double cy=static_cast<double>(p->y)*p->size+
                            p->size*.5-static_cast<double>(agent.y);
            return cx*cx+cy*cy;
        };
        const double da=d(a),db=d(b);
        // Stable selection at equal distances avoids patch thrashing
        // and redundant GPU scene uploads when a region is at capacity.
        if(da!=db)return da<db;
        if(a->x!=b->x)return a->x<b->x;
        if(a->y!=b->y)return a->y<b->y;
        return a->size<b->size;
    };
    // A standard 256 m region can contain 256 16x16 land patches.
    // Capping at 64 silently discarded three quarters of the terrain,
    // leaving distant buildings without a visible ground surface.
    // Keep a bounded full-region budget; select the closest patches first
    // when the simulator supplies a larger region or 32x32 variants.
    constexpr std::size_t kMaxVisibleLandPatches = 256U;
    if(nearest.size()>kMaxVisibleLandPatches){
        std::partial_sort(nearest.begin(),
                          nearest.begin()+kMaxVisibleLandPatches,
                          nearest.end(),patch_nearer);
        nearest.resize(kMaxVisibleLandPatches);
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
        if(!root.reflection_probe&&root.primitive==protocol::objectspatial::PrimitiveKind::Unknown)
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
    std::vector<std::string> terrain_images;
    if(region.terrain_metadata)for(const auto& id:region.terrain_assets){
        protocol::pbr::Material material;
        if(protocol::pbr::effective(id,0U,0U,circuit.decoded_materials(),{},material))
            terrain_images.insert(terrain_images.end(),material.images.begin(),material.images.end());
        else terrain_images.push_back(id);
    }
    const auto next_layers=select_visible_mesh_texture_layers(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        circuit.known_avatar_roots(),
        circuit.decoded_mesh_assets(),
        circuit.decoded_texture_assets(),
        agent.x,agent.y,agent.z,kLiveTextureLayers,live_texture_layers_,
        circuit.decoded_materials(),circuit.material_overrides(),terrain_images);
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
    auto pbr_table=build_pbr_table(circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),circuit.decoded_materials(),
        circuit.material_overrides(),live_texture_layers_,region.terrain_assets,
        std::array<float,3>{agent.x,agent.y,agent.z});
    terrain_texture_layers_=terrain_pbr_layers_=0U;
    std::array<PBRRow,protocol::pbr::kRows> terrain_record{};
    for(unsigned i=0U;i<4U;++i){
        const auto material=pbr_table.faces.find(pbr_face_key(0U,i));
        if(material!=pbr_table.faces.end()){terrain_record[0][i]=float(material->second);++terrain_pbr_layers_;}
        const auto image=live_texture_layers_.find(region.terrain_assets[i]);
        if(image!=live_texture_layers_.end()){terrain_record[1][i]=float(image->second);++terrain_texture_layers_;}
    }
    terrain_record[2]=region.terrain_start;terrain_record[3]=region.terrain_range;
    const auto land_origin=live_region_origin_valid_?live_region_origin_:std::array<float,3>{agent.x,agent.y,agent.z};
    terrain_record[4]={-1.f,land_origin[0],land_origin[2],land_origin[1]};
    if(region.terrain_metadata&&(terrain_pbr_layers_||terrain_texture_layers_)){
        const auto index=static_cast<std::uint32_t>(pbr_table.rows.size()/protocol::pbr::kRows);
        pbr_table.rows.insert(pbr_table.rows.end(),terrain_record.begin(),terrain_record.end());
        for(std::size_t i=0U;i<built.scene.terrain_count;++i)scene.vertices[i].pbr_material=index;
    }
    // R64: mesh-first GPU vertex scheduling. Previously the new 4,080
    // primitive allowance ran BEFORE the mesh pass and could spend the
    // entire four-million-vertex scene budget on procedural prims, leaving
    // zero capacity for nearby real buildings. Preserve actual mesh LODs
    // first, then fill spare DX12 scene capacity with closest primitives.
    // Both paths still bind the same already-selected SL texture/PBR atlas.
    // R87 migration phase 1: track shared decoded asset identities and
    // simulator instances without changing the known-good DX12 draw path.
    // GPU buffer reuse will be introduced only after parity tests.
    mesh_residency_.begin_generation();
    for(const auto& [asset_id,lod]:circuit.decoded_mesh_assets())
        mesh_residency_.see_asset(asset_id,lod.triangles.size(),
                                  lod.decoded_bytes);
    for(const auto& [id,root]:circuit.decoded_spatial_objects())
        if(!root.mesh_asset_id.empty())
            mesh_residency_.see_instance(id,root.parent_id,root.mesh_asset_id);
    for(const auto& [id,child]:circuit.decoded_linked_objects())
        if(!child.mesh_asset_id.empty())
            mesh_residency_.see_instance(id,child.parent_id,child.mesh_asset_id);
    mesh_residency_.end_generation();
    const auto mesh_objects=build_live_mesh_geometry(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        circuit.known_avatar_roots(),
        circuit.decoded_mesh_assets(),
        agent.x,agent.y,agent.z,std::min<std::size_t>(
            graphics_.mesh_triangles(),live_world_vertices_remaining(
                scene.vertices.size())/3U),
        live_texture_layers_,pbr_table.faces);
    const auto new_mesh_vertices=mesh_objects.scene.vertices.size();
    const auto mesh_scene_first=scene.vertices.size();
    scene.vertices.insert(scene.vertices.end(),
        mesh_objects.scene.vertices.begin(),mesh_objects.scene.vertices.end());
    scene.terrain_count+=new_mesh_vertices;

    const auto primitives=build_live_primitive_geometry(
        circuit.decoded_spatial_objects(),
        circuit.decoded_linked_objects(),
        agent.x,agent.y,agent.z,
        graphics_.preset==graphics::Preset::Quality?kMaxQualityPrimitiveShapes:
        graphics_.preset==graphics::Preset::Balanced?512U:
        graphics_.preset==graphics::Preset::Performance?256U:128U,
        circuit.known_avatar_roots(),live_texture_layers_,pbr_table.faces,
        live_world_vertices_remaining(scene.vertices.size()));
    const auto new_primitive_vertices=primitives.scene.vertices.size();
    scene.vertices.insert(scene.vertices.end(),
                          primitives.scene.vertices.begin(),
                          primitives.scene.vertices.end());
    scene.terrain_count+=new_primitive_vertices;

    // Actual SL land stays in the terrain section. Append only an
    // obvious generic local placeholder in the separate avatar section.
    if(live_avatar_preview_.active()){
        const auto marker=build_live_avatar_placeholder();
        scene.avatar_count=marker.avatar_count;
        scene.vertices.insert(scene.vertices.end(),
                              marker.vertices.begin(),marker.vertices.end());
    }
    if(scene.vertices.empty()||scene.vertices.size()>4000000U){
        error_=L"R13 verified live data produced no drawable DX12 geometry";
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
    const bool material_changed=pbr_rows_!=pbr_table.rows;
    const auto previous_rows=pbr_rows_;

    using SourceSpan=Dx12MeshBufferCache::InstanceSourceSpan;
    using DrawRequirement=Dx12MeshBufferCache::InstanceDrawRequirement;
    std::vector<SourceSpan> source_spans,candidate_spans;
    std::vector<DrawRequirement> candidate_requirements;
    std::vector<Dx12MeshBufferCache::InstanceVisibility> candidate_visibility;
    source_spans.reserve(mesh_objects.instance_ranges.size());
    candidate_spans.reserve(mesh_objects.instance_ranges.size());
    candidate_requirements.reserve(mesh_objects.instance_ranges.size());
    candidate_visibility.reserve(mesh_objects.instance_ranges.size());
    bool ranges_valid=true;
    for(const auto& range:mesh_objects.instance_ranges){
        if(!range.local_id||!range.vertex_count||
           range.vertex_count>std::numeric_limits<UINT>::max()||
           range.first_vertex>new_mesh_vertices||
           range.vertex_count>new_mesh_vertices-range.first_vertex||
           mesh_scene_first>std::numeric_limits<std::size_t>::max()-range.first_vertex){
            ranges_valid=false;
            break;
        }
        const auto absolute_first=mesh_scene_first+range.first_vertex;
        if(absolute_first>scene.vertices.size()||
           range.vertex_count>scene.vertices.size()-absolute_first){
            ranges_valid=false;
            break;
        }
        source_spans.push_back({range.local_id,range.first_vertex,range.vertex_count});
        candidate_spans.push_back({range.local_id,absolute_first,range.vertex_count});
        candidate_requirements.push_back(
            {range.local_id,static_cast<UINT>(range.vertex_count)});
        candidate_visibility.push_back({range.local_id,
            Dx12MeshBufferCache::bounds_for_vertices(
                mesh_objects.scene.vertices.data()+range.first_vertex,
                range.vertex_count)});
    }
    std::sort(candidate_visibility.begin(),candidate_visibility.end(),
        [](const auto& a,const auto& b){return a.local_id<b.local_id;});
    std::sort(candidate_spans.begin(),candidate_spans.end(),
        [](const SourceSpan& a,const SourceSpan& b){
            if(a.first_vertex!=b.first_vertex)return a.first_vertex<b.first_vertex;
            return a.local_id<b.local_id;
        });
    std::sort(candidate_requirements.begin(),candidate_requirements.end(),
        [](const DrawRequirement& a,const DrawRequirement& b){
            return a.local_id<b.local_id;
        });
    const bool complete_source_batch=ranges_valid&&new_mesh_vertices>0U&&
        Dx12MeshBufferCache::covers_complete_mesh_scene(
            new_mesh_vertices,source_spans)&&
        candidate_requirements.size()==mesh_objects.instance_ranges.size();

    std::vector<std::uint32_t> mesh_material_ids;
    mesh_material_ids.reserve(new_mesh_vertices);
    for(const auto& vertex:mesh_objects.scene.vertices)
        mesh_material_ids.push_back(vertex.pbr_material);
    const auto material_count=pbr_table.rows.size()/protocol::pbr::kRows+1U;
    std::vector<bool> blended_materials(material_count,false);
    for(std::size_t material=1;material<material_count;++material){
        const auto row=material*protocol::pbr::kRows+4U;
        if(row<pbr_table.rows.size())
            blended_materials[material]=pbr_table.rows[row][0]>1.5f;
    }
    const bool opaque_parity=Dx12MeshBufferCache::opaque_material_parity(
        mesh_material_ids,blended_materials);
    const bool candidate_batch=complete_source_batch&&opaque_parity&&
        Dx12MeshBufferCache::within_resident_batch_draw_budget(
            candidate_requirements.size());

    const auto same_spans=[](const auto& a,const auto& b){
        if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i){
            if(a[i].local_id!=b[i].local_id||
               a[i].first_vertex!=b[i].first_vertex||
               a[i].vertex_count!=b[i].vertex_count)return false;
        }
        return true;
    };
    const auto same_requirements=[](const auto& a,const auto& b){
        if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i){
            if(a[i].local_id!=b[i].local_id||
               a[i].vertex_count!=b[i].vertex_count)return false;
        }
        return true;
    };
    const auto same_visibility=[](const auto& a,const auto& b){
        if(a.size()!=b.size())return false;
        for(std::size_t i=0;i<a.size();++i){
            const auto& x=a[i];const auto& y=b[i];
            if(x.local_id!=y.local_id||x.bounds.valid!=y.bounds.valid||
               x.bounds.x!=y.bounds.x||x.bounds.y!=y.bounds.y||
               x.bounds.z!=y.bounds.z||x.bounds.radius!=y.bounds.radius)return false;
        }
        return true;
    };
    const bool unchanged=!material_changed&&region_mesh_&&
        same_scene_geometry(*region_mesh_,scene)&&
        same_spans(candidate_spans,resident_mesh_spans_)&&
        same_requirements(candidate_requirements,resident_mesh_requirements_)&&
        same_visibility(candidate_visibility,resident_mesh_visibility_)&&
        candidate_batch==resident_mesh_batch_candidate_&&
        (!candidate_batch||resident_mesh_batch_ready_);

    pbr_rows_=pbr_table.rows;
    if(!unchanged){
        auto previous=std::move(region_mesh_);
        auto previous_instance_cache=std::move(mesh_instance_cache_);
        auto previous_spans=std::move(resident_mesh_spans_);
        auto previous_requirements=std::move(resident_mesh_requirements_);
        auto previous_visibility=std::move(resident_mesh_visibility_);
        const auto previous_candidate=resident_mesh_batch_candidate_;
        const auto previous_ready=resident_mesh_batch_ready_;
        const auto previous_active=resident_mesh_batch_active_;
        const auto previous_revision=resident_mesh_revision_;

        region_mesh_=std::move(scene);
        resident_mesh_spans_=std::move(candidate_spans);
        resident_mesh_requirements_=std::move(candidate_requirements);
        resident_mesh_visibility_=std::move(candidate_visibility);
        resident_mesh_batch_candidate_=candidate_batch;
        resident_mesh_batch_ready_=false;
        resident_mesh_batch_active_=false;
        if(++resident_mesh_revision_==0U)resident_mesh_revision_=1U;

        // All instance uploads and the combined-scene replacement happen
        // after the prior graphics fence. If any stage fails, restore the
        // previous scene/cache as one transaction.
        if(ready_){
            wait_for_gpu();
            if(!make_geometry()){
                region_mesh_=std::move(previous);
                pbr_rows_=previous_rows;
                mesh_instance_cache_=std::move(previous_instance_cache);
                resident_mesh_spans_=std::move(previous_spans);
                resident_mesh_requirements_=std::move(previous_requirements);
                resident_mesh_visibility_=std::move(previous_visibility);
                resident_mesh_batch_candidate_=previous_candidate;
                resident_mesh_batch_ready_=previous_ready;
                resident_mesh_batch_active_=previous_active;
                resident_mesh_revision_=previous_revision;
                if(error_.empty())error_=L"R72 DX12 land mesh upload failed";
                return false;
            }
        }
    }
    if(material_changed&&ready_&&!make_material_records(pbr_rows_)){
        ready_=false;error_=L"DX12 glTF material record upload failed";return false;
    }
    bound_pbr_faces_=pbr_table.faces.size();
    if(!live_region_origin_valid_){
        live_region_origin_=stable_origin;
        live_region_origin_valid_=true;
    }
    std::map<std::uint32_t,protocol::objectspatial::Transform> probes;
    for(const auto& [id,t]:circuit.decoded_spatial_objects())if(t.reflection_probe&&probes.size()<64U)
        probes.emplace(id,t);
    bool probes_changed=probes.size()!=environment_server_probes_.size();
    if(!probes_changed)for(const auto& [id,t]:probes){
        const auto old=environment_server_probes_.find(id);
        if(old==environment_server_probes_.end()||!protocol::objectspatial::same_spatial_transform(t,old->second)){
            probes_changed=true;break;
        }
    }
    environment_server_probes_=std::move(probes);
    environment_dirty_=environment_dirty_||!unchanged||probes_changed;
    uploaded_root_bounds_=new_proxy_count;
    uploaded_root_primitive_shapes_=primitives.root_shapes;
    uploaded_linked_primitive_shapes_=primitives.linked_shapes;
    uploaded_primitive_vertices_=new_primitive_vertices;
    uploaded_mesh_vertices_=new_mesh_vertices;
    previous_mesh_draw_count_=uploaded_mesh_shapes_;
    uploaded_mesh_shapes_=mesh_objects.root_meshes+mesh_objects.linked_meshes;
    uploaded_mesh_triangles_=mesh_objects.emitted_triangles;
    mesh_eligible_instances_=mesh_objects.eligible_instances;
    mesh_missing_lod_instances_=mesh_objects.missing_lod_instances;
    mesh_waiting_for_parent_=mesh_objects.waiting_for_parent;
    mesh_out_of_range_instances_=mesh_objects.out_of_range_instances;
    mesh_invalid_pose_instances_=mesh_objects.invalid_pose_instances;
    mesh_undrawn_instances_=mesh_objects.undrawn_eligible_instances;
    mesh_parent_missing_instances_=mesh_objects.parent_missing_instances;
    mesh_parent_avatar_instances_=mesh_objects.parent_avatar_instances;
    mesh_parent_cycle_instances_=mesh_objects.parent_cycle_instances;
    mesh_parent_invalid_instances_=mesh_objects.parent_invalid_instances;
    mesh_parent_deep_instances_=mesh_objects.parent_too_deep_instances;
    mesh_undrawn_transparent_instances_=mesh_objects.undrawn_transparent_instances;
    mesh_undrawn_bad_geometry_instances_=mesh_objects.undrawn_bad_geometry_instances;
    mesh_undrawn_budget_instances_=mesh_objects.undrawn_budget_instances;
    mesh_undrawn_global_limit_=mesh_objects.undrawn_global_limit;
    mesh_undrawn_object_limit_=mesh_objects.undrawn_object_limit;
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
    if(nearest.empty()&&first_live_land){
        // Some simulators deliver object updates before the first terrain
        // layer. Keep those authenticated objects visible instead of
        // waiting for land; frame the camera around the verified spawn.
        landscape_anchor_={0.f,-2.77f,0.f};
        landscape_anchor_valid_=true;
        camera_.set_subject({0.f,-1.7f,0.f});
        camera_.apply_studio_shot(camera::StudioShot::Wide);
    }
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
    auto prior_instance_cache=std::move(mesh_instance_cache_);
    auto prior_spans=std::move(resident_mesh_spans_);
    auto prior_requirements=std::move(resident_mesh_requirements_);
    auto prior_visibility=std::move(resident_mesh_visibility_);
    const auto prior_candidate=resident_mesh_batch_candidate_;
    const auto prior_ready=resident_mesh_batch_ready_;
    const auto prior_active=resident_mesh_batch_active_;
    const auto prior_revision=resident_mesh_revision_;
    region_mesh_=std::move(decoded_static_geometry);
    resident_mesh_spans_.clear();
    resident_mesh_requirements_.clear();
    resident_mesh_visibility_.clear();
    resident_mesh_batch_candidate_=false;
    resident_mesh_batch_ready_=false;
    resident_mesh_batch_active_=false;
    if(++resident_mesh_revision_==0U)resident_mesh_revision_=1U;
    if(ready_&&!make_geometry()) {
        region_mesh_=std::move(prior);
        mesh_instance_cache_=std::move(prior_instance_cache);
        resident_mesh_spans_=std::move(prior_spans);
        resident_mesh_requirements_=std::move(prior_requirements);
        resident_mesh_visibility_=std::move(prior_visibility);
        resident_mesh_batch_candidate_=prior_candidate;
        resident_mesh_batch_ready_=prior_ready;
        resident_mesh_batch_active_=prior_active;
        resident_mesh_revision_=prior_revision;
        error_=L"DX12 could not allocate decoded region geometry";
        return false;
    }
    error_.clear();
    return true;
}
void Renderer::clear_decoded_region_geometry() {
    // Always invalidate simulator-local instance identities on disconnect,
    // even if no region vertex buffer was successfully uploaded yet.
    mesh_residency_.clear_instances();
    previous_mesh_draw_count_=0;
    mesh_instance_cache_.clear();
    resident_mesh_spans_.clear();
    resident_mesh_requirements_.clear();
    resident_mesh_visibility_.clear();
    resident_mesh_batch_candidate_=false;
    resident_mesh_batch_ready_=false;
    resident_mesh_batch_active_=false;
    if(++resident_mesh_revision_==0U)resident_mesh_revision_=1U;
    if(!region_mesh_)return;
    if(ready_)wait_for_gpu();
    // Clear server data immediately on disconnect. If restoring the local
    // scene fails, stop rendering rather than showing stale world geometry.
    region_mesh_.reset();
    environment_ready_=false;environment_face_=0U;environment_dirty_=true;
    environment_server_probes_.clear();write_environment_probe();
    terrain_texture_layers_=terrain_pbr_layers_=0U;
    uploaded_root_bounds_=0;
    uploaded_root_primitive_shapes_=0;
    uploaded_linked_primitive_shapes_=0;
    uploaded_primitive_vertices_=0;
    uploaded_mesh_vertices_=uploaded_mesh_shapes_=uploaded_mesh_triangles_=0;
    mesh_eligible_instances_=mesh_missing_lod_instances_=0;
    mesh_waiting_for_parent_=mesh_out_of_range_instances_=0;
    mesh_invalid_pose_instances_=mesh_undrawn_instances_=0;
    mesh_parent_missing_instances_=mesh_parent_avatar_instances_=0;
    mesh_parent_cycle_instances_=mesh_parent_invalid_instances_=0;
    mesh_parent_deep_instances_=0;
    mesh_undrawn_transparent_instances_=0;
    mesh_undrawn_bad_geometry_instances_=0;
    mesh_undrawn_budget_instances_=0;
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
    // A failed residency preflight is recoverable: fence old readers and
    // rebuild the full combined-scene fallback before recording this frame.
    if(resident_mesh_batch_active_&&
       !mesh_instance_cache_.can_draw_exact_instance_batch(
           "__kami_live_region__",resident_mesh_revision_,
           resident_mesh_requirements_)){
        wait_for_gpu();
        resident_mesh_batch_ready_=false;
        resident_mesh_batch_active_=false;
        if(!make_geometry()){
            ready_=false;
            if(error_.empty())error_=L"DX12 resident mesh fallback rebuild failed";
            return;
        }
    }
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
    if(!sort_blended_geometry(camera.eye.x,camera.eye.y,camera.eye.z)){
        ready_=false;error_=L"DX12 transparent geometry update failed";return;
    }
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
    using VisibilityCache=Dx12MeshBufferCache;
    std::array<float,16> camera_matrix{},shadow_matrix{};
    std::memcpy(camera_matrix.data(),&viewproj,sizeof(viewproj));
    std::memcpy(shadow_matrix.data(),&lightproj,sizeof(lightproj));
    std::array<VisibilityCache::VisibilityPlane,6> camera_frustum{},shadow_frustum{};
    const bool camera_frustum_valid=
        VisibilityCache::extract_frustum_planes(camera_matrix,camera_frustum);
    const bool shadow_frustum_valid=
        VisibilityCache::extract_frustum_planes(shadow_matrix,shadow_frustum);
    std::array<std::uint32_t,VisibilityCache::kMaxResidentBatchDraws>
        camera_visible_ids{},shadow_visible_ids{};
    std::size_t camera_visible_count=0U,shadow_visible_count=0U;
    if(resident_mesh_batch_active_){
        const auto all_visible=[&](auto& ids,std::size_t& count){
            count=0U;
            for(const auto& item:resident_mesh_requirements_)
                ids[count++]=item.local_id;
        };
        if(!camera_frustum_valid||
           !VisibilityCache::select_visible_instance_ids(
               resident_mesh_requirements_,resident_mesh_visibility_,camera_frustum,
               camera_visible_ids.data(),camera_visible_ids.size(),
               camera_visible_count))
            all_visible(camera_visible_ids,camera_visible_count);
        if(!graphics_.shadows||!shadow_frustum_valid||
           !VisibilityCache::select_visible_instance_ids(
               resident_mesh_requirements_,resident_mesh_visibility_,shadow_frustum,
               shadow_visible_ids.data(),shadow_visible_ids.size(),
               shadow_visible_count))
            all_visible(shadow_visible_ids,shadow_visible_count);
    }
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
        if(!mesh_gpu_cache_.record_draw(commands_.Get(),"__kami_scene__",
                                        static_cast<UINT>(terrain_count_))) {
            // Preserve the previous draw path if residency is unavailable.
            commands_->IASetVertexBuffers(0,1,&vertex_view_);
            commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1,0,0);
        }
        if(resident_mesh_batch_active_){
            if(!mesh_instance_cache_.record_visible_instance_batch(
                commands_.Get(),"__kami_live_region__",resident_mesh_revision_,
                resident_mesh_requirements_,shadow_visible_ids.data(),
                shadow_visible_count)){
                ready_=false;
                error_=L"DX12 resident shadow visibility batch lost its preflight";
                return;
            }
            commands_->IASetVertexBuffers(0,1,&vertex_view_);
        }
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

    if(!record_environment(constants)){ready_=false;error_=L"PBR environment capture failed";return;}
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
    if(!mesh_gpu_cache_.record_draw(commands_.Get(),"__kami_scene__",
                                        static_cast<UINT>(terrain_count_))) {
            // Preserve the previous draw path if residency is unavailable.
            commands_->IASetVertexBuffers(0,1,&vertex_view_);
            commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1,0,0);
        }
    if(resident_mesh_batch_active_){
        if(!mesh_instance_cache_.record_visible_instance_batch(
            commands_.Get(),"__kami_live_region__",resident_mesh_revision_,
            resident_mesh_requirements_,camera_visible_ids.data(),
            camera_visible_count)){
            ready_=false;
            error_=L"DX12 resident PBR visibility batch lost its preflight";
            return;
        }
        commands_->IASetVertexBuffers(0,1,&vertex_view_);
    }
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
    if(!transparent_vertices_.empty()){
        constants[25]=0.f;constants[47]=0.f;constants[55]=0.f;
        constants[56]=0.f;constants[57]=0.f;constants[58]=0.f;constants[59]=0.f;
        constants[60]=0.f;constants[61]=0.f;constants[62]=0.f;
        commands_->SetPipelineState(blend_pipeline_.Get());
        commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);
        commands_->DrawInstanced(static_cast<UINT>(transparent_vertices_.size()),1,
            static_cast<UINT>(terrain_count_+avatar_count_),0);
    }
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
    mesh_gpu_cache_.clear();
    scene_gpu_revision_=0;
    ready_=false;
    if(fence_event_) { CloseHandle(fence_event_); fence_event_=nullptr; }
    depth_.Reset();
    shadow_map_.Reset();
    material_texture_.Reset();
    material_upload_.Reset();
    skin_palette_buffer_.Reset();
    material_records_.Reset();transparent_vertices_.clear();
    environment_raw_.Reset();environment_filtered_.Reset();environment_diffuse_.Reset();environment_brdf_.Reset();
    environment_depth_.Reset();environment_data_.Reset();environment_rtv_.Reset();environment_dsv_.Reset();
    environment_capture_pipeline_.Reset();environment_sky_pipeline_.Reset();environment_filter_pipeline_.Reset();
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
    blend_pipeline_.Reset();
    shadow_pipeline_.Reset();
    sky_pipeline_.Reset();
    signature_.Reset();
    fence_.Reset();
    rtv_heap_.Reset();
    dsv_heap_.Reset();
    shadow_srv_heap_.Reset();
}
} // namespace kami::world


