#include "KamiDx12Renderer.h"
#include "KamiDx12Runner.h"

#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <iomanip>
#include <limits>
#include <sstream>

using Microsoft::WRL::ComPtr;

namespace kami::dx12 {
namespace {
// Trace only explicit test executions. Normal Second Life sessions never
// create a diagnostic file. Breadcrumbs survive Windows access violations.
void traceDx12Smoke(const char* stage) noexcept
{
    const wchar_t* cmd = GetCommandLineW();
    if (!cmd || (!std::wcsstr(cmd, L"--kami-dx12-") &&
                 !std::wcsstr(cmd, L"--warp")))
        return;
    HANDLE out = CreateFileW(L"kami-dx12-smoke.trace", FILE_APPEND_DATA,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(out, stage, static_cast<DWORD>(std::strlen(stage)), &written, nullptr);
    WriteFile(out, "\r\n", 2, &written, nullptr);
    CloseHandle(out);
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
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
D3D12_CPU_DESCRIPTOR_HANDLE offsetHandle(D3D12_CPU_DESCRIPTOR_HANDLE h,
                                         UINT index, UINT stride)
{
    h.ptr += static_cast<SIZE_T>(index) * stride;
    return h;
}
constexpr const char* kShader = R"(
struct VSOutput { float4 position : SV_Position; };
VSOutput VSMain(uint index : SV_VertexID)
{
    float2 vertices[3] = {
        float2(-0.80, -0.80),
        float2( 0.00,  0.80),
        float2( 0.80, -0.80)
    };
    VSOutput output;
    output.position = float4(vertices[index], 0.0, 1.0);
    return output;
}
float4 PSMain(VSOutput input) : SV_Target
{
    // Actual GPU pixel-shader output checked by the DX12 WARP smoke test.
    return float4(0.12, 0.82, 0.93, 1.0);
}
)";
} // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::fail(const char* what, HRESULT result)
{
    std::ostringstream stream;
    stream << what << " failed: HRESULT 0x" << std::hex
           << static_cast<unsigned long>(result);
    error_ = stream.str();
    return false;
}

bool Renderer::initialize(bool warpForTesting)
{
    traceDx12Smoke("initialize: begin");
    shutdown();
    error_.clear();

    traceDx12Smoke("initialize: CreateDXGIFactory2");
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_));
    if (FAILED(hr)) return fail("CreateDXGIFactory2", hr);

    ComPtr<IDXGIAdapter1> selected;
    if (warpForTesting)
    {
        traceDx12Smoke("initialize: EnumWarpAdapter");
        hr = factory_->EnumWarpAdapter(IID_PPV_ARGS(&selected));
        if (FAILED(hr)) return fail("EnumWarpAdapter (test-only)", hr);
    }
    else
    {
        for (UINT i = 0;; ++i)
        {
            ComPtr<IDXGIAdapter1> candidate;
            hr = factory_->EnumAdapterByGpuPreference(
                i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&candidate));
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr)) return fail("EnumAdapterByGpuPreference", hr);
            DXGI_ADAPTER_DESC1 desc{};
            if (FAILED(candidate->GetDesc1(&desc)) ||
                (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) continue;
            if (SUCCEEDED(D3D12CreateDevice(candidate.Get(),
                                           D3D_FEATURE_LEVEL_11_0,
                                           IID_PPV_ARGS(&device_))))
            {
                selected = candidate;
                break;
            }
        }
        if (!selected) return fail("No hardware DirectX 12 device", DXGI_ERROR_UNSUPPORTED);
    }

    if (!device_)
    {
        traceDx12Smoke("initialize: D3D12CreateDevice");
        hr = D3D12CreateDevice(selected.Get(), D3D_FEATURE_LEVEL_11_0,
                              IID_PPV_ARGS(&device_));
        if (FAILED(hr)) return fail("D3D12CreateDevice", hr);
    }
    DXGI_ADAPTER_DESC1 adapterDesc{};
    if (SUCCEEDED(selected->GetDesc1(&adapterDesc)))
        adapterName_ = adapterDesc.Description;

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    traceDx12Smoke("initialize: CreateCommandQueue");
    hr = device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_));
    if (FAILED(hr)) return fail("CreateCommandQueue", hr);

    for (auto& allocator : allocators_)
    {
        hr = device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator));
        if (FAILED(hr)) return fail("CreateCommandAllocator", hr);
    }

    traceDx12Smoke("initialize: CreateCommandList");
    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    allocators_[0].Get(), nullptr,
                                    IID_PPV_ARGS(&commandList_));
    if (FAILED(hr)) return fail("CreateCommandList", hr);
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("Close initial command list", hr);

    traceDx12Smoke("initialize: CreateFence");
    hr = device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) return fail("CreateFence", hr);
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_)
        return fail("CreateEventW", HRESULT_FROM_WIN32(GetLastError()));

    BOOL tearing = FALSE;
    if (SUCCEEDED(factory_->CheckFeatureSupport(
        DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing))))
        allowTearing_ = tearing == TRUE;

    traceDx12Smoke("initialize: makePipeline");
    return makePipeline();
}

bool Renderer::makePipeline()
{
    D3D12_ROOT_SIGNATURE_DESC rootDesc{};
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> signature, diagnostics;
    traceDx12Smoke("pipeline: SerializeRootSignature");
    HRESULT hr = D3D12SerializeRootSignature(
        &rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &diagnostics);
    if (FAILED(hr)) return fail("D3D12SerializeRootSignature", hr);
    traceDx12Smoke("pipeline: CreateRootSignature");
    hr = device_->CreateRootSignature(0, signature->GetBufferPointer(),
                                      signature->GetBufferSize(),
                                      IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) return fail("CreateRootSignature", hr);

    ComPtr<ID3DBlob> vs, ps;
    traceDx12Smoke("pipeline: CompileShaders");
    hr = D3DCompile(kShader, std::strlen(kShader), "KamiDx12Smoke.hlsl",
                    nullptr, nullptr, "VSMain", "vs_5_1",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &vs, &diagnostics);
    if (FAILED(hr)) return fail("D3DCompile vertex shader", hr);
    diagnostics.Reset();
    hr = D3DCompile(kShader, std::strlen(kShader), "KamiDx12Smoke.hlsl",
                    nullptr, nullptr, "PSMain", "ps_5_1",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &ps, &diagnostics);
    if (FAILED(hr)) return fail("D3DCompile pixel shader", hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask =
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = kColorFormat;
    pso.SampleDesc.Count = 1;
    traceDx12Smoke("pipeline: CreateGraphicsPipelineState");
    hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipeline_));
    if (FAILED(hr)) return fail("CreateGraphicsPipelineState", hr);
    return true;
}

bool Renderer::makeSwapchainTargets()
{
    if (!swapChain_) return fail("No DX12 swap chain", E_POINTER);
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.NumDescriptors = kFrameCount;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    HRESULT hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeap_));
    if (FAILED(hr)) return fail("Create RTV descriptor heap", hr);
    rtvStride_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto handle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; ++i)
    {
        hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&targets_[i]));
        if (FAILED(hr)) return fail("GetBuffer", hr);
        device_->CreateRenderTargetView(
            targets_[i].Get(), nullptr, offsetHandle(handle, i, rtvStride_));
    }
    return true;
}

bool Renderer::attachWindow(HWND hwnd, UINT width, UINT height)
{
    if (!device_ || !queue_) return fail("D3D12 device not initialized", E_FAIL);
    if (!hwnd || !IsWindow(hwnd) || width == 0 || height == 0)
        return fail("Invalid HWND or zero-sized surface", E_INVALIDARG);
    if (swapChain_) return fail("Window already attached", E_UNEXPECTED);

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = kColorFormat;
    desc.BufferCount = kFrameCount;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.Flags = allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> intermediate;
    HRESULT hr = factory_->CreateSwapChainForHwnd(
        queue_.Get(), hwnd, &desc, nullptr, nullptr, &intermediate);
    if (FAILED(hr)) return fail("CreateSwapChainForHwnd", hr);
    hr = intermediate.As(&swapChain_);
    if (FAILED(hr)) return fail("Query IDXGISwapChain3", hr);
    factory_->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    width_ = width;
    height_ = height;
    return makeSwapchainTargets();
}

bool Renderer::waitForFence(UINT64 value)
{
    if (value == 0 || fence_->GetCompletedValue() >= value) return true;
    HRESULT hr = fence_->SetEventOnCompletion(value, fenceEvent_);
    if (FAILED(hr)) return fail("Fence::SetEventOnCompletion", hr);
    DWORD result = WaitForSingleObject(fenceEvent_, 30000);
    if (result != WAIT_OBJECT_0)
    {
        if (result == WAIT_FAILED)
            return fail("WaitForSingleObject", HRESULT_FROM_WIN32(GetLastError()));
        return fail("GPU fence timeout", HRESULT_FROM_WIN32(WAIT_TIMEOUT));
    }
    return true;
}

bool Renderer::waitForFrame(UINT index)
{
    return index < kFrameCount && waitForFence(frameFenceValues_[index]);
}

UINT64 Renderer::signal()
{
    const UINT64 value = nextFenceValue_++;
    HRESULT hr = queue_->Signal(fence_.Get(), value);
    if (FAILED(hr))
    {
        fail("CommandQueue::Signal", hr);
        return 0;
    }
    return value;
}

bool Renderer::flush()
{
    if (!queue_ || !fence_ || !fenceEvent_) return true;
    const UINT64 fenceValue = signal();
    return fenceValue != 0 && waitForFence(fenceValue);
}

bool Renderer::encodeScene(ID3D12Resource* target,
                           D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                           UINT width, UINT height,
                           D3D12_RESOURCE_STATES initial,
                           D3D12_RESOURCE_STATES final)
{
    if (!target || width == 0 || height == 0 || !pipeline_)
        return fail("Missing render target or pipeline", E_INVALIDARG);

    if (initial != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        const auto barrier = transition(target, initial, D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->ResourceBarrier(1, &barrier);
    }
    D3D12_VIEWPORT viewport{0.f, 0.f, static_cast<FLOAT>(width),
                            static_cast<FLOAT>(height), 0.f, 1.f};
    D3D12_RECT scissor{0, 0, static_cast<LONG>(width),
                       static_cast<LONG>(height)};
    commandList_->RSSetViewports(1, &viewport);
    commandList_->RSSetScissorRects(1, &scissor);
    commandList_->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    const FLOAT clear[4] = {0.f, 0.f, 0.f, 1.f};
    commandList_->ClearRenderTargetView(rtv, clear, 0, nullptr);
    commandList_->SetGraphicsRootSignature(rootSignature_.Get());
    commandList_->SetPipelineState(pipeline_.Get());
    commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList_->DrawInstanced(3, 1, 0, 0);

    if (final != D3D12_RESOURCE_STATE_RENDER_TARGET)
    {
        const auto barrier = transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET, final);
        commandList_->ResourceBarrier(1, &barrier);
    }
    return true;
}

bool Renderer::renderFrame(bool vsync)
{
    if (!ready() || !rtvHeap_) return fail("Swap chain is not ready", E_FAIL);
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    if (!waitForFrame(index)) return false;

    HRESULT hr = allocators_[index]->Reset();
    if (FAILED(hr)) return fail("CommandAllocator::Reset", hr);
    hr = commandList_->Reset(allocators_[index].Get(), pipeline_.Get());
    if (FAILED(hr)) return fail("CommandList::Reset", hr);
    const auto rtv = offsetHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart(),
                                  index, rtvStride_);
    if (!encodeScene(targets_[index].Get(), rtv, width_, height_,
                     D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT))
        return false;
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("CommandList::Close", hr);

    ID3D12CommandList* lists[] = {commandList_.Get()};
    queue_->ExecuteCommandLists(1, lists);
    const UINT flags = (!vsync && allowTearing_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    hr = swapChain_->Present(vsync ? 1 : 0, flags);
    if (FAILED(hr)) return fail("SwapChain::Present", hr);
    const UINT64 value = signal();
    if (!value) return false;
    frameFenceValues_[index] = value;
    return true;
}

bool Renderer::resize(UINT width, UINT height)
{
    if (!ready() || width == 0 || height == 0)
        return fail("Resize requires initialized nonzero swap chain", E_INVALIDARG);
    if (width == width_ && height == height_) return true;
    if (!flush()) return false;
    for (auto& target : targets_) target.Reset();
    rtvHeap_.Reset();
    frameFenceValues_.fill(0);

    HRESULT hr = swapChain_->ResizeBuffers(kFrameCount, width, height, kColorFormat,
                                           allowTearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    if (FAILED(hr)) return fail("SwapChain::ResizeBuffers", hr);
    width_ = width;
    height_ = height;
    return makeSwapchainTargets();
}

bool Renderer::renderOffscreenSmokeTest(UINT width, UINT height)
{
    if (!deviceReady() || !pipeline_ || width < 16 || height < 16)
        return fail("Offscreen smoke test requires a DX12 device and 16+ pixels", E_INVALIDARG);

    D3D12_RESOURCE_DESC rtDesc{};
    rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Width = width;
    rtDesc.Height = height;
    rtDesc.DepthOrArraySize = 1;
    rtDesc.MipLevels = 1;
    rtDesc.Format = kColorFormat;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = kColorFormat;
    clearValue.Color[3] = 1.f;
    traceDx12Smoke("offscreen: CreateRenderTarget");
    ComPtr<ID3D12Resource> target;
    HRESULT hr = device_->CreateCommittedResource(
        &defaultHeap, D3D12_HEAP_FLAG_NONE, &rtDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
        IID_PPV_ARGS(&target));
    if (FAILED(hr)) return fail("Create offscreen render texture", hr);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = 1;
    ComPtr<ID3D12DescriptorHeap> heap;
    traceDx12Smoke("offscreen: CreateRTVHeap");
    hr = device_->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap));
    if (FAILED(hr)) return fail("Create offscreen RTV heap", hr);
    const auto rtv = heap->GetCPUDescriptorHandleForHeapStart();
    device_->CreateRenderTargetView(target.Get(), nullptr, rtv);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT numRows = 0;
    UINT64 rowBytes = 0, totalBytes = 0;
    device_->GetCopyableFootprints(&rtDesc, 0, 1, 0,
                                   &footprint, &numRows, &rowBytes, &totalBytes);
    if (totalBytes == 0 || totalBytes > static_cast<UINT64>(std::numeric_limits<SIZE_T>::max()))
        return fail("Invalid readback footprint", E_INVALIDARG);

    D3D12_RESOURCE_DESC bufferDesc{};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = totalBytes;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    ComPtr<ID3D12Resource> readback;
    traceDx12Smoke("offscreen: CreateReadbackBuffer");
    hr = device_->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback));
    if (FAILED(hr)) return fail("Create GPU readback resource", hr);

    if (!waitForFrame(0)) return false;
    hr = allocators_[0]->Reset();
    if (FAILED(hr)) return fail("Offscreen allocator reset", hr);
    traceDx12Smoke("offscreen: RecordCommands");
    hr = commandList_->Reset(allocators_[0].Get(), pipeline_.Get());
    if (FAILED(hr)) return fail("Offscreen command-list reset", hr);
    if (!encodeScene(target.Get(), rtv, width, height,
                     D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE))
        return false;

    D3D12_TEXTURE_COPY_LOCATION dst{};
    dst.pResource = readback.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION src{};
    src.pResource = target.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.SubresourceIndex = 0;
    commandList_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    hr = commandList_->Close();
    if (FAILED(hr)) return fail("Offscreen command-list close", hr);
    ID3D12CommandList* lists[] = {commandList_.Get()};
    traceDx12Smoke("offscreen: SubmitAndWait");
    queue_->ExecuteCommandLists(1, lists);
    if (!flush()) return false;

    // Check GPU-produced BGRA pixels, not just API return codes.
    D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes)};
    void* mapped = nullptr;
    hr = readback->Map(0, &readRange, &mapped);
    if (FAILED(hr)) return fail("Readback Map", hr);
    const auto* bytes = static_cast<const unsigned char*>(mapped);
    traceDx12Smoke("offscreen: ReadPixels");
    const SIZE_T middle = static_cast<SIZE_T>(height / 2) *
                              footprint.Footprint.RowPitch +
                          static_cast<SIZE_T>(width / 2) * 4u;
    const bool centerColored =
        bytes[middle + 0] > 160 && bytes[middle + 1] > 130 &&
        bytes[middle + 2] > 10 && bytes[middle + 3] > 200;
    const bool cornerBlack = bytes[0] < 5 && bytes[1] < 5 && bytes[2] < 5;
    D3D12_RANGE writtenRange{0, 0};
    readback->Unmap(0, &writtenRange);
    traceDx12Smoke("offscreen: VerifyResult");
    if (!centerColored || !cornerBlack)
        return fail("DX12 pixel readback mismatch (triangle not rendered)", E_FAIL);
    return true;
}

void Renderer::shutdown() noexcept
{
    if (queue_ && fence_ && fenceEvent_) flush();
    for (auto& uploads : frameUploads_) uploads.clear();
    worldDepth_.Reset();
    worldDepthHeap_.Reset();
    worldPipeline_.Reset();
    worldRootSignature_.Reset();
    worldDepthWidth_ = worldDepthHeight_ = 0;
    for (auto& target : targets_) target.Reset();
    rtvHeap_.Reset();
    swapChain_.Reset();
    pipeline_.Reset();
    rootSignature_.Reset();
    commandList_.Reset();
    for (auto& allocator : allocators_) allocator.Reset();
    fence_.Reset();
    queue_.Reset();
    device_.Reset();
    factory_.Reset();
    if (fenceEvent_)
    {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    frameFenceValues_.fill(0);
    nextFenceValue_ = 1;
    rtvStride_ = 0;
    width_ = height_ = 0;
    allowTearing_ = false;
    adapterName_.clear();
}

// Keep the native Renderer object inside the same compilation unit as its
// implementation. This avoids relying on the viewer PCH's structure packing
// and other C++ ABI settings when constructing and destroying DX12 objects.
// A failure still returns nonzero; no WARP rendering checks are bypassed.
int runEmbeddedSmokeTest(bool warpForTesting, bool testWorldDepth) noexcept
{
    try
    {
        traceDx12Smoke("embedded: entering native renderer");
        Renderer renderer;
        traceDx12Smoke("embedded: renderer constructed");
        if (!renderer.initialize(warpForTesting))
        {
            OutputDebugStringA(renderer.error().c_str());
            return 61;
        }
        if (!renderer.renderOffscreenSmokeTest(128, 128))
        {
            OutputDebugStringA(renderer.error().c_str());
            return 62;
        }
        if (testWorldDepth && !renderer.renderWorldOffscreenTest(128, 128))
        {
            OutputDebugStringA(renderer.error().c_str());
            return 63;
        }
        renderer.shutdown();
        traceDx12Smoke("embedded: all DX12 tests passed");
        return 0;
    }
    catch (...)
    {
        OutputDebugStringA("KAMI native DX12 diagnostic: unexpected C++ exception");
        return 64;
    }
}

} // namespace kami::dx12
