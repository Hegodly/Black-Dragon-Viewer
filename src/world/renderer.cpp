#include "kami/world/renderer.hpp"
#include "kami/world/scene.hpp"
#include "kami/world/shaders.hpp"
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace kami::world {
namespace {
constexpr DXGI_FORMAT kColorFormat=DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthFormat=DXGI_FORMAT_D32_FLOAT;



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
    auto* graphics=device_.native_device();
    D3D12_ROOT_PARAMETER constants{};
    constants.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.Constants.ShaderRegister=0;
    constants.Constants.RegisterSpace=0;
    constants.Constants.Num32BitValues=20;
    constants.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters=1;
    root.pParameters=&constants;
    root.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    if(FAILED(D3D12SerializeRootSignature(
            &root,D3D_ROOT_SIGNATURE_VERSION_1,
            serialized.GetAddressOf(),errors.GetAddressOf())))
        return false;
    if(FAILED(graphics->CreateRootSignature(
            0,serialized->GetBufferPointer(),serialized->GetBufferSize(),
            IID_PPV_ARGS(signature_.ReleaseAndGetAddressOf()))))
        return false;

    Microsoft::WRL::ComPtr<ID3DBlob> vs,ps;
    constexpr UINT flags=D3DCOMPILE_ENABLE_STRICTNESS;
    if(FAILED(D3DCompile(kSceneShader,sizeof(kSceneShader)-1,nullptr,nullptr,nullptr,
            "vertex_main","vs_5_0",flags,0,
            vs.GetAddressOf(),errors.ReleaseAndGetAddressOf()))) return false;
    if(FAILED(D3DCompile(kSceneShader,sizeof(kSceneShader)-1,nullptr,nullptr,nullptr,
            "pixel_main","ps_5_0",flags,0,
            ps.GetAddressOf(),errors.ReleaseAndGetAddressOf()))) return false;

    const D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature=signature_.Get();
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};
    desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    desc.InputLayout={input,3};
    desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    desc.RasterizerState.FrontCounterClockwise=FALSE;
    desc.RasterizerState.DepthClipEnable=TRUE;
    desc.BlendState.RenderTarget[0].RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    desc.DepthStencilState.DepthEnable=TRUE;
    desc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;
    desc.SampleMask=UINT_MAX;
    desc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    desc.NumRenderTargets=1;
    desc.RTVFormats[0]=kColorFormat;
    desc.DSVFormat=kDepthFormat;
    desc.SampleDesc.Count=1;
    return SUCCEEDED(graphics->CreateGraphicsPipelineState(
        &desc,IID_PPV_ARGS(pipeline_.ReleaseAndGetAddressOf())));
}

bool Renderer::make_geometry() {
    const SceneMesh scene=build_test_scene();
    if(scene.vertices.empty() ||
       scene.vertices.size()>std::numeric_limits<UINT>::max()/sizeof(Vertex))
        return false;
    terrain_count_=scene.terrain_count;
    avatar_count_=scene.avatar_count;
    const UINT bytes=static_cast<UINT>(scene.vertices.size()*sizeof(Vertex));
    const auto properties=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto desc=buffer_desc(bytes);
    if(FAILED(device_.native_device()->CreateCommittedResource(
            &properties,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
            IID_PPV_ARGS(geometry_.ReleaseAndGetAddressOf()))))
        return false;
    void* mapped=nullptr;
    if(FAILED(geometry_->Map(0,nullptr,&mapped))) return false;
    std::memcpy(mapped,scene.vertices.data(),bytes);
    geometry_->Unmap(0,nullptr);
    vertex_view_.BufferLocation=geometry_->GetGPUVirtualAddress();
    vertex_view_.SizeInBytes=bytes;
    vertex_view_.StrideInBytes=sizeof(Vertex);
    return true;
}

bool Renderer::initialize(HWND window,UINT width,UINT height) {
    if(ready_) return true;
    if(!window || !width || !height) { error_=L"Invalid viewport dimensions"; return false; }
    window_=window;
    width_=width;
    height_=height;
    if(device_.initialize(true)==DeviceStatus::Unavailable) {
        error_=L"No DirectX 12 device available";
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(factory.GetAddressOf())))) {
        error_=L"DXGI factory creation failed";
        return false;
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
    dsv.NumDescriptors=1;
    if(FAILED(device_.native_device()->CreateDescriptorHeap(
            &dsv,IID_PPV_ARGS(dsv_heap_.ReleaseAndGetAddressOf()))) ||
       !make_targets() || !make_depth()) {
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
    if(!ready_ || !width || !height || (width==width_ && height==height_)) return;
    wait_for_gpu();
    for(auto& buffer:buffers_) buffer.Reset();
    depth_.Reset();
    if(FAILED(swapchain_->ResizeBuffers(kBuffers,width,height,kColorFormat,0))) {
        ready_=false;
        error_=L"Failed to resize DirectX 12 swap chain";
        return;
    }
    width_=width;
    height_=height;
    if(!make_targets() || !make_depth()) {
        ready_=false;
        error_=L"Failed to recreate DirectX 12 attachments";
    }
}

void Renderer::orbit(float yaw_delta,float elevation_delta) {
    yaw_+=yaw_delta;
    elevation_=std::clamp(elevation_+elevation_delta,1.f,18.f);
}
void Renderer::zoom(float distance_delta) {
    distance_=std::clamp(distance_+distance_delta,5.f,40.f);
}

void Renderer::draw() {
    if(!ready_ || !width_ || !height_) return;
    if(FAILED(allocator_->Reset()) ||
       FAILED(commands_->Reset(allocator_.Get(),pipeline_.Get()))) return;

    const UINT index=swapchain_->GetCurrentBackBufferIndex();
    auto begin=barrier(buffers_[index].Get(),
        D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands_->ResourceBarrier(1,&begin);
    const auto rtv=rtv_handle(index);
    const auto dsv=dsv_heap_->GetCPUDescriptorHandleForHeapStart();
    const float background[]={.055f,.09f,.15f,1.f};
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
    commands_->SetGraphicsRootSignature(signature_.Get());
    commands_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands_->IASetVertexBuffers(0,1,&vertex_view_);

    using namespace DirectX;
    const XMVECTOR eye=XMVectorSet(
        std::sin(yaw_)*distance_,elevation_,
        std::cos(yaw_)*distance_,0.f);
    const XMMATRIX view=XMMatrixLookAtLH(
        eye,XMVectorSet(0,1,0,0),XMVectorSet(0,1,0,0));
    const XMMATRIX projection=XMMatrixPerspectiveFovLH(
        XMConvertToRadians(65.f),static_cast<float>(width_)/height_,
        .08f,200.f);
    XMFLOAT4X4 viewproj{};
    XMStoreFloat4x4(&viewproj,view*projection);
    std::array<float,20> constants{};
    std::memcpy(constants.data(),&viewproj,sizeof(viewproj));
    constants[16]=.35f;
    constants[17]=-.86f;
    constants[18]=.38f;
    constants[19]=1.f;
    commands_->SetGraphicsRoot32BitConstants(0,20,constants.data(),0);
    commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1,0,0);
    commands_->DrawInstanced(static_cast<UINT>(avatar_count_),1,
                            static_cast<UINT>(terrain_count_),0);
    auto end=barrier(buffers_[index].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);
    commands_->ResourceBarrier(1,&end);
    if(FAILED(commands_->Close())) return;
    ID3D12CommandList* list[]={commands_.Get()};
    device_.graphics_queue()->ExecuteCommandLists(1,list);
    const HRESULT result=swapchain_->Present(1,0);
    if(FAILED(result)) {
        error_=L"DX12 presentation failed";
        ready_=false;
        return;
    }
    // Single in-flight frame prioritizes safe startup/test behavior.
    // A multiframe allocator/fence ring will replace this before FPS sign-off.
    wait_for_gpu();
}
void Renderer::shutdown() noexcept {
    if(device_.graphics_queue()) wait_for_gpu();
    ready_=false;
    if(fence_event_) { CloseHandle(fence_event_); fence_event_=nullptr; }
    depth_.Reset();
    for(auto& buffer:buffers_) buffer.Reset();
    swapchain_.Reset();
    geometry_.Reset();
    commands_.Reset();
    allocator_.Reset();
    pipeline_.Reset();
    signature_.Reset();
    fence_.Reset();
    rtv_heap_.Reset();
    dsv_heap_.Reset();
}
} // namespace kami::world
