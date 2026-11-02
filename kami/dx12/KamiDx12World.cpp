#include "KamiDx12Renderer.h"

#include <d3dcompiler.h>
#include <cstring>
#include <limits>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace kami::dx12 {
namespace {
constexpr const char* kWorldShader = R"(
cbuffer PerMesh : register(b0)
{
    row_major float4x4 worldViewProjection;
}
struct VSIn
{
    float3 position : POSITION;
    float4 color : COLOR0;
};
struct PSIn
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};
PSIn VSWorld(VSIn input)
{
    PSIn output;
    output.position = mul(float4(input.position, 1.0), worldViewProjection);
    output.color = input.color;
    return output;
}
float4 PSWorld(PSIn input) : SV_Target0
{
    return input.color;
}
)";

D3D12_RESOURCE_BARRIER worldTransition(ID3D12Resource* resource,
                                        D3D12_RESOURCE_STATES before,
                                        D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    return barrier;
}
} // namespace

bool Renderer::prepareWorldPipeline()
{
    if (worldPipeline_ && worldRootSignature_) return true;
    if (!device_) return fail("World pass requires D3D12 device", E_FAIL);

    D3D12_ROOT_PARAMETER constants{};
    constants.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.Constants.ShaderRegister = 0;
    constants.Constants.RegisterSpace = 0;
    constants.Constants.Num32BitValues = 16;
    constants.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters = 1;
    root.pParameters = &constants;
    root.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature, errors;
    HRESULT hr = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1,
                                              &signature, &errors);
    if (FAILED(hr)) return fail("Serialize indexed-world root signature", hr);
    hr = device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                      signature->GetBufferSize(),
                                      IID_PPV_ARGS(&worldRootSignature_));
    if (FAILED(hr)) return fail("Create indexed-world root signature", hr);

    ComPtr<ID3DBlob> vs, ps;
    hr = D3DCompile(kWorldShader, std::strlen(kWorldShader), "KamiDx12World.hlsl",
                    nullptr, nullptr, "VSWorld", "vs_5_1",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &errors);
    if (FAILED(hr)) return fail("Compile D3D12 world vertex shader", hr);
    errors.Reset();
    hr = D3DCompile(kWorldShader, std::strlen(kWorldShader), "KamiDx12World.hlsl",
                    nullptr, nullptr, "PSWorld", "ps_5_1",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &errors);
    if (FAILED(hr)) return fail("Compile D3D12 world pixel shader", hr);

    D3D12_INPUT_ELEMENT_DESC inputs[2]{};
    inputs[0] = {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    inputs[1] = {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
                 D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = worldRootSignature_.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.InputLayout = {inputs, 2};
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = kColorFormat;
    pso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    pso.SampleDesc.Count = 1;
    hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&worldPipeline_));
    if (FAILED(hr)) return fail("Create indexed-world depth-tested pipeline", hr);
    return true;
}

bool Renderer::ensureWorldDepth(UINT width, UINT height)
{
    if (worldDepth_ && worldDepthWidth_ == width && worldDepthHeight_ == height)
        return true;
    if (!device_ || !width || !height)
        return fail("World depth requires nonzero dimensions", E_INVALIDARG);

    // A resized depth image may still be referenced by earlier queued frames.
    if (worldDepth_ && !flush()) return false;
    worldDepth_.Reset();
    worldDepthHeap_.Reset();
    worldDepthWidth_ = worldDepthHeight_ = 0;

    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.NumDescriptors = 1;
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    HRESULT hr = device_->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&worldDepthHeap_));
    if (FAILED(hr)) return fail("Create D3D12 world depth descriptor", hr);

    D3D12_RESOURCE_DESC resource{};
    resource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resource.Width = width;
    resource.Height = height;
    resource.DepthOrArraySize = 1;
    resource.MipLevels = 1;
    resource.Format = DXGI_FORMAT_D32_FLOAT;
    resource.SampleDesc.Count = 1;
    resource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resource.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = DXGI_FORMAT_D32_FLOAT;
    clear.DepthStencil.Depth = 1.f;
    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    hr = device_->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                          &resource, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                          &clear, IID_PPV_ARGS(&worldDepth_));
    if (FAILED(hr)) return fail("Create native D3D12 depth texture", hr);

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv{};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    device_->CreateDepthStencilView(
        worldDepth_.Get(), &dsv, worldDepthHeap_->GetCPUDescriptorHandleForHeapStart());
    worldDepthWidth_ = width;
    worldDepthHeight_ = height;
    return true;
}

bool Renderer::recordWorldDraws(
    const WorldFrame& scene, ID3D12Resource* target,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, UINT width, UINT height,
    D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to,
    std::vector<ComPtr<ID3D12Resource>>& liveUploads)
{
    if (!target || !worldPipeline_ || !worldDepth_ || !width || !height)
        return fail("World scene requires pipeline, target and depth", E_INVALIDARG);
    if (scene.meshes.size() > 256)
        return fail("World frame exceeds 256 mesh prototype cap", E_INVALIDARG);

    // This first-pass upload path deliberately bounds memory. Production must
    // replace per-frame uploads with cached DEFAULT heaps and streaming.
    constexpr std::size_t kMaxUploadBytes = 32u * 1024u * 1024u;
    std::size_t totalBytes = 0;
    for (const WorldMesh& mesh : scene.meshes)
    {
        if (mesh.vertices.empty() || mesh.indices.empty() ||
            mesh.indices.size() % 3 != 0 ||
            mesh.vertices.size() > (1u << 20) ||
            mesh.indices.size() > (1u << 21))
            return fail("World scene has invalid vertex/index counts", E_INVALIDARG);
        const std::size_t bytes = mesh.vertices.size() * sizeof(WorldVertex) +
                                  mesh.indices.size() * sizeof(std::uint32_t);
        if (bytes > kMaxUploadBytes - totalBytes)
            return fail("World scene exceeds 32 MiB upload budget", E_OUTOFMEMORY);
        totalBytes += bytes;
        for (const std::uint32_t index : mesh.indices)
            if (index >= mesh.vertices.size())
                return fail("World mesh has out-of-range index", E_INVALIDARG);
    }

    std::size_t uploadSlot = 0;
    auto upload = [this, &liveUploads, &uploadSlot](const void* data, std::size_t bytes,
                                       ComPtr<ID3D12Resource>& result) -> bool {
        D3D12_HEAP_PROPERTIES props{};
        props.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = static_cast<UINT64>(bytes);
        buffer.Height = 1;
        buffer.DepthOrArraySize = 1;
        buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        if (uploadSlot == liveUploads.size()) liveUploads.emplace_back();
        auto& cached = liveUploads[uploadSlot++];
        HRESULT hr = S_OK;
        if (!cached || cached->GetDesc().Width != bytes)
        {
            // Caller has retired this frame's fence before reusing its pool.
            cached.Reset();
            hr = device_->CreateCommittedResource(
                &props, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, IID_PPV_ARGS(&cached));
            if (FAILED(hr)) return fail("Create indexed-world GPU upload buffer", hr);
        }
        result = cached;
        void* mapped = nullptr;
        D3D12_RANGE readRange{0, 0};
        hr = result->Map(0, &readRange, &mapped);
        if (FAILED(hr)) return fail("Map indexed-world GPU upload buffer", hr);
        std::memcpy(mapped, data, bytes);
        const D3D12_RANGE writtenRange{0, static_cast<SIZE_T>(bytes)};
        result->Unmap(0, &writtenRange);
        // The referenced buffers must stay alive until command-list GPU fence.
        // Pool retains ownership until this frame's fence completes.
        return true;
    };

    if (from != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        const auto barrier = worldTransition(target, from, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->ResourceBarrier(1, &barrier);
    }
    const D3D12_VIEWPORT viewport{0.f, 0.f, static_cast<FLOAT>(width),
                                  static_cast<FLOAT>(height), 0.f, 1.f};
    const D3D12_RECT scissor{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);
    const auto dsv = worldDepthHeap_->GetCPUDescriptorHandleForHeapStart();
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList_->ClearRenderTargetView(rtv, scene.clearColor.data(), 0, nullptr);
    commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
    commandList_->SetGraphicsRootSignature(worldRootSignature_.Get());
    commandList_->SetPipelineState(worldPipeline_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const WorldMesh& mesh : scene.meshes)
    {
        ComPtr<ID3D12Resource> vertices, indices;
        const std::size_t vertexBytes = mesh.vertices.size() * sizeof(WorldVertex);
        const std::size_t indexBytes = mesh.indices.size() * sizeof(std::uint32_t);
        if (!upload(mesh.vertices.data(), vertexBytes, vertices) ||
            !upload(mesh.indices.data(), indexBytes, indices))
            return false;
        D3D12_VERTEX_BUFFER_VIEW vertexView{};
        vertexView.BufferLocation = vertices->GetGPUVirtualAddress();
        vertexView.SizeInBytes = static_cast<UINT>(vertexBytes);
        vertexView.StrideInBytes = sizeof(WorldVertex);
        D3D12_INDEX_BUFFER_VIEW indexView{};
        indexView.BufferLocation = indices->GetGPUVirtualAddress();
        indexView.SizeInBytes = static_cast<UINT>(indexBytes);
        indexView.Format = DXGI_FORMAT_R32_UINT;
        commandList_->IASetVertexBuffers(0, 1, &vertexView);
        commandList_->IASetIndexBuffer(&indexView);
        commandList_->SetGraphicsRoot32BitConstants(
            0, 16, mesh.worldViewProjection.data(), 0);
        commandList_->DrawIndexedInstanced(static_cast<UINT>(mesh.indices.size()),
                                            1, 0, 0, 0);
    }
    liveUploads.resize(uploadSlot); // Discard unused completed slots; pool stays within 32 MiB.
    if (to != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        const auto barrier = worldTransition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, to);
        commandList_->ResourceBarrier(1, &barrier);
    }
    return true;
}

bool Renderer::worldFrameAvailable() const noexcept
{
    if (!ready() || !fence_) return false;
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    return fence_->GetCompletedValue() >= frameFenceValues_[index];
}

bool Renderer::renderWorldFrame(const WorldFrame& scene, bool vsync)
{
    if (!ready() || !rtvHeap_) return fail("No native D3D12 presentation surface", E_FAIL);
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    if (!worldFrameAvailable()) return true; // Busy: drop preview update, never wait.
    // Retain completed frame buffers for reuse; the availability poll retired their fence.
    if (!prepareWorldPipeline() || !ensureWorldDepth(width_, height_)) return false;

    HRESULT hr = allocators_[index]->Reset();
    if (FAILED(hr)) return fail("Reset indexed-world command allocator", hr);
    hr = commandList_->Reset(allocators_[index].Get(), worldPipeline_.Get());
    if (FAILED(hr)) return fail("Reset indexed-world command list", hr);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += static_cast<SIZE_T>(index) * rtvStride_;
    if (!recordWorldDraws(scene, targets_[index].Get(), rtv, width_, height_,
                          D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT,
                          frameUploads_[index]))
        return false;
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("Close indexed-world command list", hr);
    ID3D12CommandList* lists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    const UINT flags = ((!vsync && allowTearing_) ? DXGI_PRESENT_ALLOW_TEARING : 0)
                       | (!vsync ? DXGI_PRESENT_DO_NOT_WAIT : 0);
    hr = swapChain_->Present(vsync ? 1 : 0, flags);
    // Even an unpresented submission owns its buffers until the GPU fence.
    const HRESULT presentResult = hr;
    const UINT64 fence = signal();
    if (!fence) return false;
    frameFenceValues_[index] = fence;
    if (FAILED(presentResult) && presentResult != DXGI_ERROR_WAS_STILL_DRAWING)
        return fail("Present native indexed-world frame", presentResult);
    return true;
}

bool Renderer::renderWorldOffscreenTest(UINT width, UINT height)
{
    if (!deviceReady() || width < 32 || height < 32)
        return fail("DX12 indexed-world test requires 32x32+ device", E_INVALIDARG);
    if (!prepareWorldPipeline() || !ensureWorldDepth(width, height)) return false;

    WorldFrame scene;
    WorldMesh foreground;
    // Red foreground covers the center. Blue background is drawn AFTERWARD,
    // and MUST fail the depth test where the nearer red triangle exists.
    foreground.vertices = {
        {{{-0.65f, -0.65f, 0.20f}}, {{0.91f, 0.16f, 0.08f, 1.f}}},
        {{{ 0.00f,  0.65f, 0.20f}}, {{0.91f, 0.16f, 0.08f, 1.f}}},
        {{{ 0.65f, -0.65f, 0.20f}}, {{0.91f, 0.16f, 0.08f, 1.f}}}
    };
    foreground.indices = {0, 1, 2};
    // Exercise per-mesh transforms as well as GPU vertex/index binding.
    foreground.worldViewProjection[12] = 0.10f;
    WorldMesh background;
    background.vertices = {
        {{{-0.82f, -0.82f, 0.80f}}, {{0.05f, 0.11f, 0.96f, 1.f}}},
        {{{ 0.00f,  0.82f, 0.80f}}, {{0.05f, 0.11f, 0.96f, 1.f}}},
        {{{ 0.82f, -0.82f, 0.80f}}, {{0.05f, 0.11f, 0.96f, 1.f}}}
    };
    background.indices = {0, 1, 2};
    scene.meshes.push_back(std::move(foreground));
    scene.meshes.push_back(std::move(background));

    D3D12_RESOURCE_DESC targetDesc{};
    targetDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    targetDesc.Width = width;
    targetDesc.Height = height;
    targetDesc.DepthOrArraySize = 1;
    targetDesc.MipLevels = 1;
    targetDesc.Format = kColorFormat;
    targetDesc.SampleDesc.Count = 1;
    targetDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear{};
    clear.Format = kColorFormat;
    clear.Color[3] = 1.f;
    ComPtr<ID3D12Resource> target;
    HRESULT hr = device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &targetDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clear,
        IID_PPV_ARGS(&target));
    if (FAILED(hr)) return fail("Create indexed-world offscreen target", hr);

    D3D12_DESCRIPTOR_HEAP_DESC heap{};
    heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    hr = device_->CreateDescriptorHeap(&heap, IID_PPV_ARGS(&rtvHeap));
    if (FAILED(hr)) return fail("Create indexed-world RTV heap", hr);
    const auto rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    device_->CreateRenderTargetView(target.Get(), nullptr, rtv);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0;
    UINT64 rowBytes = 0, bytes = 0;
    device_->GetCopyableFootprints(&targetDesc, 0, 1, 0,
                                   &footprint, &rows, &rowBytes, &bytes);
    if (!bytes || bytes > static_cast<UINT64>(std::numeric_limits<SIZE_T>::max()))
        return fail("Indexed-world invalid readback footprint", E_INVALIDARG);
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = bytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    ComPtr<ID3D12Resource> readback;
    hr = device_->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr)) return fail("Create indexed-world GPU readback", hr);

    // The test uses allocator zero; wait for its prior fence before reuse.
    if (!waitForFrame(0)) return false;
    hr = allocators_[0]->Reset();
    if (FAILED(hr)) return fail("Reset indexed-world test allocator", hr);
    hr = commandList_->Reset(allocators_[0].Get(), worldPipeline_.Get());
    if (FAILED(hr)) return fail("Reset indexed-world test command list", hr);
    std::vector<ComPtr<ID3D12Resource>> liveUploads;
    if (!recordWorldDraws(scene, target.Get(), rtv, width, height,
                          D3D12_RESOURCE_STATE_RENDER_TARGET,
                          D3D12_RESOURCE_STATE_COPY_SOURCE, liveUploads))
        return false;

    D3D12_TEXTURE_COPY_LOCATION from{};
    from.pResource = target.Get();
    from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    from.SubresourceIndex = 0;
    D3D12_TEXTURE_COPY_LOCATION to{};
    to.pResource = readback.Get();
    to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    to.PlacedFootprint = footprint;
    commandList_->CopyTextureRegion(&to, 0, 0, 0, &from, nullptr);
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("Close indexed-world test commands", hr);
    ID3D12CommandList* lists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    if (!flush()) return false;

    void* mapped = nullptr;
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(bytes)};
    hr = readback->Map(0, &readRange, &mapped);
    if (FAILED(hr)) return fail("Map indexed-world GPU readback", hr);
    const auto* pixels = static_cast<const unsigned char*>(mapped);
    const std::size_t center = static_cast<std::size_t>(height / 2) *
                               footprint.Footprint.RowPitch +
                               static_cast<std::size_t>(width / 2) * 4;
    // BGRA readback: foreground MUST stay red even though blue is drawn later.
    const bool redOccludesBlue =
        pixels[center + 0] < 65 &&
        pixels[center + 1] > 20 && pixels[center + 1] < 85 &&
        pixels[center + 2] > 180 && pixels[center + 3] > 245;
    const bool blackClear =
        pixels[0] < 5 && pixels[1] < 5 && pixels[2] < 5;
    D3D12_RANGE writtenRange{0, 0};
    readback->Unmap(0, &writtenRange);
    if (!redOccludesBlue || !blackClear)
        return fail("Indexed-world mesh, MVP or depth readback mismatched", E_FAIL);
    return true;
}

} // namespace kami::dx12
