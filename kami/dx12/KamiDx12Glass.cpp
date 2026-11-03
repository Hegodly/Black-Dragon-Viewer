// Native D3D12 glass compositor experiment. This is an offscreen GPU test,
// not a live Second Life UI integration; no OpenGL dependency or fallback.
#include "KamiDx12Renderer.h"
#include <d3dcompiler.h>
#include <cstdint>
#include <cstring>
#include <limits>

using Microsoft::WRL::ComPtr;

namespace kami::dx12 {
namespace {
constexpr const char* kGlassComputeShader = R"(
Texture2D<float4> scene : register(t0);
RWTexture2D<float4> outputSurface : register(u0);

[numthreads(8, 8, 1)]
void CSGlass(uint3 thread : SV_DispatchThreadID)
{
    uint width, height;
    outputSurface.GetDimensions(width, height);
    if (thread.x >= width || thread.y >= height) return;

    // This rectangle represents the footprint of a native UI glass panel.
    // World pixels outside the panel pass through without any blur.
    float4 original = scene.Load(int3(thread.xy, 0));
    if (thread.x < width/4 || thread.x >= width*3/4 ||
        thread.y < height/4 || thread.y >= height*3/4)
    {
        outputSurface[thread.xy] = original;
        return;
    }
    // Gaussian separable weights multiplied into a 5x5 footprint.
    // Sampling nearby *scene* pixels is true backdrop blur, not alpha alone.
    const float weights[5] = {1.0, 4.0, 6.0, 4.0, 1.0};
    float3 accum = float3(0.0, 0.0, 0.0);
    [unroll] for (int iy = -2; iy <= 2; ++iy)
    {
        [unroll] for (int ix = -2; ix <= 2; ++ix)
        {
            int2 coord = clamp(int2(thread.xy) + int2(ix, iy),
                               int2(0, 0), int2(width - 1, height - 1));
            float weight = weights[ix + 2] * weights[iy + 2] / 256.0;
            accum += scene.Load(int3(coord, 0)).rgb * weight;
        }
    }
    float3 frosted = lerp(accum, float3(0.82, 0.86, 0.94), 0.16);
    outputSurface[thread.xy] = float4(frosted, 1.0);
}
)";

D3D12_RESOURCE_BARRIER glassTransition(ID3D12Resource* resource,
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

bool Renderer::renderGlassOffscreenTest(UINT width, UINT height)
{
    if (!deviceReady() || width < 32 || height < 32 || width > 512 || height > 512)
        return fail("DX12 glass WARP test dimensions must be 32-512", E_INVALIDARG);

    // Keep this test isolated; a production pass will take a real scene
    // framebuffer SRV and composite onto the viewer's UI render target.
    constexpr DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;

    ComPtr<ID3D12Resource> scene, glass;
    HRESULT hr = device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&scene));
    if (FAILED(hr)) return fail("Create DX12 glass scene texture", hr);

    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    hr = device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&glass));
    if (FAILED(hr)) return fail("Create native DX12 frosted-glass UAV", hr);

    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0;
    UINT64 rowBytes = 0, totalBytes = 0;
    device_->GetCopyableFootprints(&desc, 0, 1, 0, &footprint,
                                   &rows, &rowBytes, &totalBytes);
    if (!totalBytes || totalBytes > std::numeric_limits<SIZE_T>::max())
        return fail("Invalid native glass texture footprint", E_INVALIDARG);

    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = totalBytes;
    buffer.Height = 1;
    buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ComPtr<ID3D12Resource> upload, readback;
    hr = device_->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload));
    if (FAILED(hr)) return fail("Create glass upload buffer", hr);
    hr = device_->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr)) return fail("Create glass readback buffer", hr);

    // An alternating high-contrast stripe pattern proves whether the GPU
    // really samples neighboring pixels inside the panel.
    void* mapped = nullptr;
    const D3D12_RANGE emptyRead{0, 0};
    hr = upload->Map(0, &emptyRead, &mapped);
    if (FAILED(hr)) return fail("Map glass source upload", hr);
    std::memset(mapped, 0, static_cast<SIZE_T>(totalBytes));
    for (UINT y = 0; y < height; ++y)
    {
        auto* row = static_cast<std::uint8_t*>(mapped)
                    + static_cast<SIZE_T>(y) * footprint.Footprint.RowPitch;
        for (UINT x = 0; x < width; ++x)
        {
            const std::uint8_t tone = (x % 2 == 0) ? 255 : 0;
            row[x * 4 + 0] = tone;
            row[x * 4 + 1] = tone;
            row[x * 4 + 2] = tone;
            row[x * 4 + 3] = 255;
        }
    }
    const D3D12_RANGE written{0, static_cast<SIZE_T>(totalBytes)};
    upload->Unmap(0, &written);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 2;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> heap;
    hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
    if (FAILED(hr)) return fail("Create glass SRV/UAV descriptors", hr);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = format;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    const UINT stride = device_->GetDescriptorHandleIncrementSize(heapDesc.Type);
    auto cpu = heap->GetCPUDescriptorHandleForHeapStart();
    device_->CreateShaderResourceView(scene.Get(), &srv, cpu);
    cpu.ptr += static_cast<SIZE_T>(stride);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};
    uav.Format = format;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    device_->CreateUnorderedAccessView(glass.Get(), nullptr, &uav, cpu);

    D3D12_DESCRIPTOR_RANGE ranges[2]{};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    D3D12_ROOT_PARAMETER parameters[2]{};
    for (UINT i = 0; i < 2; ++i)
    {
        parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameters[i].DescriptorTable.NumDescriptorRanges = 1;
        parameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
        parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.NumParameters = 2;
    rootDesc.pParameters = parameters;
    ComPtr<ID3DBlob> rootBlob, errors;
    hr = D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                     &rootBlob, &errors);
    if (FAILED(hr)) return fail("Serialize glass compute root signature", hr);
    ComPtr<ID3D12RootSignature> root;
    hr = device_->CreateRootSignature(0, rootBlob->GetBufferPointer(),
                                      rootBlob->GetBufferSize(), IID_PPV_ARGS(&root));
    if (FAILED(hr)) return fail("Create glass compute root signature", hr);

    ComPtr<ID3DBlob> compute;
    hr = D3DCompile(kGlassComputeShader, std::strlen(kGlassComputeShader),
                    "KamiGlassCS.hlsl", nullptr, nullptr, "CSGlass", "cs_5_1",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &compute, &errors);
    if (FAILED(hr)) return fail("Compile native glass 5x5 Gaussian shader", hr);
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = root.Get();
    psoDesc.CS = {compute->GetBufferPointer(), compute->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso;
    hr = device_->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) return fail("Create D3D12 glass compute pipeline", hr);

    if (!waitForFrame(0)) return false;
    hr = allocators_[0]->Reset();
    if (FAILED(hr)) return fail("Reset glass test command allocator", hr);
    hr = commandList_->Reset(allocators_[0].Get(), pso.Get());
    if (FAILED(hr)) return fail("Reset glass compute command list", hr);

    D3D12_TEXTURE_COPY_LOCATION fromUpload{};
    fromUpload.pResource = upload.Get();
    fromUpload.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    fromUpload.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION toScene{};
    toScene.pResource = scene.Get();
    toScene.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    toScene.SubresourceIndex = 0;
    commandList_->CopyTextureRegion(&toScene, 0, 0, 0, &fromUpload, nullptr);
    const auto barrier = glassTransition(scene.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
                                         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList_->ResourceBarrier(1, &barrier);
    commandList_->SetComputeRootSignature(root.Get());
    commandList_->SetPipelineState(pso.Get());
    ID3D12DescriptorHeap* heaps[] = {heap.Get()};
    commandList_->SetDescriptorHeaps(1, heaps);
    auto gpu = heap->GetGPUDescriptorHandleForHeapStart();
    commandList_->SetComputeRootDescriptorTable(0, gpu);
    gpu.ptr += static_cast<UINT64>(stride);
    commandList_->SetComputeRootDescriptorTable(1, gpu);
    commandList_->Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    const auto toCopy = glassTransition(glass.Get(),
                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                       D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList_->ResourceBarrier(1, &toCopy);

    D3D12_TEXTURE_COPY_LOCATION toReadback{};
    toReadback.pResource = readback.Get();
    toReadback.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    toReadback.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION fromGlass{};
    fromGlass.pResource = glass.Get();
    fromGlass.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    fromGlass.SubresourceIndex = 0;
    commandList_->CopyTextureRegion(&toReadback, 0, 0, 0, &fromGlass, nullptr);
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("Close glass compute command list", hr);
    ID3D12CommandList* commandLists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, commandLists);
    if (!flush()) return false;

    const D3D12_RANGE readyRead{0, static_cast<SIZE_T>(totalBytes)};
    mapped = nullptr;
    hr = readback->Map(0, &readyRead, &mapped);
    if (FAILED(hr)) return fail("Map actual D3D12 glass GPU pixels", hr);
    const auto* pixels = static_cast<const std::uint8_t*>(mapped);
    const auto value = [&](UINT x, UINT y, UINT channel) -> std::uint8_t {
        return pixels[static_cast<SIZE_T>(y) * footprint.Footprint.RowPitch
                      + static_cast<SIZE_T>(x) * 4 + channel];
    };
    const bool clearBlackOutside = value(3, 3, 0) == 0;
    const bool clearWhiteOutside = value(2, 3, 0) == 255;
    const bool blendedInside = value(width/2, height/2, 0) > 65 &&
                               value(width/2, height/2, 0) < 215 &&
                               value(width/2, height/2, 3) == 255;
    const D3D12_RANGE noWrite{0, 0};
    readback->Unmap(0, &noWrite);
    if (!clearBlackOutside || !clearWhiteOutside || !blendedInside)
        return fail("DX12 glass GPU readback: blur or passthrough did not execute", E_FAIL);
    return true;
}
} // namespace kami::dx12
