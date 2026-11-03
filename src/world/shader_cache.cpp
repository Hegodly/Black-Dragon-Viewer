#include "kami/world/shader_cache.hpp"
#include "kami/world/shaders.hpp"
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <system_error>

namespace kami::world {
namespace {
constexpr DXGI_FORMAT kColorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
constexpr DXGI_FORMAT kDepthFormat = DXGI_FORMAT_D32_FLOAT;
constexpr UINT kCompilerFlags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;
constexpr std::size_t kMaxShaderBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaxPipelineBytes = 32U * 1024U * 1024U;
constexpr std::string_view kBuildVersion = "kami-dx12-pso-v2-sm5-opt3";
namespace fs = std::filesystem;

std::uint64_t fnv(std::string_view data, std::uint64_t h = 14695981039346656037ULL) noexcept {
    for(const unsigned char c : data) { h ^= c; h *= 1099511628211ULL; }
    return h;
}
std::wstring hex(std::uint64_t value) {
    constexpr wchar_t chars[]=L"0123456789abcdef";
    std::wstring result(16, L'0');
    for(int i=15;i>=0;--i) {
        result[static_cast<std::size_t>(i)] = chars[value & 15ULL];
        value >>= 4;
    }
    return result;
}
fs::path cache_path(ID3D12Device* device, ShaderPreparation& report) {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(factory.GetAddressOf())))) return {};
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    const LUID luid = device->GetAdapterLuid();
    if(FAILED(factory->EnumAdapterByLuid(luid,IID_PPV_ARGS(adapter.GetAddressOf()))))
        return {};
    DXGI_ADAPTER_DESC1 desc{};
    if(FAILED(adapter->GetDesc1(&desc))) return {};
    report.gpu_name=desc.Description;

    // DXGI reports the UMD version through IDXGIDevice; on modern WDDM
    // D3D9/11/12 binaries from a driver package share a version number.
    LARGE_INTEGER driver{};
    const bool driver_known=SUCCEEDED(adapter->CheckInterfaceSupport(
        __uuidof(IDXGIDevice),&driver));
    const auto version=ShaderCache::source_version();
    std::wstring key=L"gpu-"+hex((std::uint64_t(desc.VendorId)<<32)|desc.DeviceId)
      +L"-luid-"+hex((std::uint64_t(std::uint32_t(luid.HighPart))<<32)|luid.LowPart)
      +L"-source-"+hex(version);
    if(driver_known) key+=L"-driver-"+hex(static_cast<std::uint64_t>(driver.QuadPart));

    PWSTR local=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,
                                  nullptr,&local))) return {};
    fs::path root(local);
    CoTaskMemFree(local);
    root/=L"KAMI";
    root/=L"ShaderCache";
    root/=key;
    std::error_code ec;
    fs::create_directories(root,ec);
    return ec ? fs::path{} : root;
}
std::vector<char> read(const fs::path& path, std::size_t max) {
    std::error_code ec;
    auto bytes=fs::file_size(path,ec);
    if(ec || bytes==0 || bytes>max) return {};
    std::ifstream stream(path,std::ios::binary);
    if(!stream) return {};
    std::vector<char> buffer(static_cast<std::size_t>(bytes));
    if(!stream.read(buffer.data(),static_cast<std::streamsize>(buffer.size())))
        return {};
    return buffer;
}
bool write(const fs::path& path,const void* data,std::size_t size) {
    if(!data || size==0) return false;
    auto tmp=path;
    tmp+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+
         L"-"+std::to_wstring(GetCurrentThreadId());
    {
        std::ofstream file(tmp,std::ios::binary|std::ios::trunc);
        if(!file) return false;
        file.write(static_cast<const char*>(data),static_cast<std::streamsize>(size));
        file.flush();
        if(!file) { std::error_code ec;fs::remove(tmp,ec);return false; }
    }
    if(!MoveFileExW(tmp.c_str(),path.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;fs::remove(tmp,ec);return false;
    }
    return true;
}
bool compile(const char* entry,const char* profile,std::vector<char>& output,
             std::wstring& error) {
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode,diagnostic;
    const HRESULT hr=D3DCompile(kSceneShader,sizeof(kSceneShader)-1,
        "KAMI_original_scene.hlsl",nullptr,nullptr,
        entry,profile,kCompilerFlags,0,
        bytecode.GetAddressOf(),diagnostic.GetAddressOf());
    if(FAILED(hr) || !bytecode) {
        error = L"Failed to compile KAMI scene shader";
        return false;
    }
    const auto bytes=bytecode->GetBufferSize();
    if(bytes==0 || bytes>kMaxShaderBytes) {
        error=L"Unexpected shader bytecode size";
        return false;
    }
    const auto* begin=static_cast<const char*>(bytecode->GetBufferPointer());
    output.assign(begin,begin+bytes);
    return true;
}
bool root_signature(ID3D12Device* device,
                    Microsoft::WRL::ComPtr<ID3D12RootSignature>& signature) {
    D3D12_ROOT_PARAMETER constants{};
    constants.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    constants.Constants.ShaderRegister=0;
    constants.Constants.Num32BitValues=20;
    constants.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC root{};
    root.NumParameters=1;
    root.pParameters=&constants;
    root.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    if(FAILED(D3D12SerializeRootSignature(&root,D3D_ROOT_SIGNATURE_VERSION_1,
        blob.GetAddressOf(),nullptr))) return false;
    return SUCCEEDED(device->CreateRootSignature(
        0,blob->GetBufferPointer(),blob->GetBufferSize(),
        IID_PPV_ARGS(signature.ReleaseAndGetAddressOf())));
}
D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline_desc(
    ID3D12RootSignature* root,
    const std::vector<char>& vs,const std::vector<char>& ps,
    const D3D12_INPUT_ELEMENT_DESC* input) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature=root;
    desc.VS={vs.data(),vs.size()};
    desc.PS={ps.data(),ps.size()};
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
    return desc;
}
} // namespace

std::uint64_t ShaderCache::source_version() noexcept {
    auto hash=fnv(kBuildVersion);
    hash=fnv({kSceneShader,sizeof(kSceneShader)-1},hash);
    return hash;
}
bool ShaderCache::prepare(
    ID3D12Device* device,
    Microsoft::WRL::ComPtr<ID3D12RootSignature>& signature,
    Microsoft::WRL::ComPtr<ID3D12PipelineState>& pipeline,
    ShaderPreparation& report,const Progress& progress) {
    report={};
    if(!device) { report.error=L"No DirectX 12 device";return false; }
    auto notify=[&](unsigned percent,const wchar_t* label) {
        if(progress) progress(percent,label);
    };
    notify(5,L"Detecting GPU and driver");
    const fs::path dir=cache_path(device,report);
    if(report.gpu_name.empty()) report.gpu_name=L"DirectX 12 device";
    const auto vs_path=dir.empty()?fs::path{}:dir/L"vertex.cso";
    const auto ps_path=dir.empty()?fs::path{}:dir/L"pixel.cso";
    const auto pso_path=dir.empty()?fs::path{}:dir/L"scene.pso";

    if(!root_signature(device,signature)) {
        report.error=L"Could not create DX12 scene root signature";
        return false;
    }
    std::vector<char> vs=dir.empty()?std::vector<char>{}:read(vs_path,kMaxShaderBytes);
    const bool has_vs=!vs.empty();
    notify(24,L"Optimizing scene vertex shader");
    if(vs.empty() && !compile("vertex_main","vs_5_0",vs,report.error)) return false;

    std::vector<char> ps=dir.empty()?std::vector<char>{}:read(ps_path,kMaxShaderBytes);
    const bool has_ps=!ps.empty();
    notify(49,L"Optimizing scene pixel shader");
    if(ps.empty() && !compile("pixel_main","ps_5_0",ps,report.error)) return false;

    static constexpr D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
    };
    auto desc=pipeline_desc(signature.Get(),vs,ps,input);
    notify(71,L"Preparing hardware-optimized graphics pipeline");
    std::vector<char> cached=dir.empty()?std::vector<char>{}:read(pso_path,kMaxPipelineBytes);
    if(!cached.empty()) {
        desc.CachedPSO={cached.data(),cached.size()};
        const HRESULT hit=device->CreateGraphicsPipelineState(
            &desc,IID_PPV_ARGS(pipeline.ReleaseAndGetAddressOf()));
        if(SUCCEEDED(hit)) {
            report.shader_cache_hit=has_vs && has_ps;
            report.pipeline_cache_hit=true;
            notify(100,L"Shader optimization cache ready");
            return true;
        }
        // Driver changes, a different GPU, or damaged blobs must never
        // prevent startup. Discard the driver-specific bytecode and rebuild.
        report.driver_cache_invalidated=true;
        pipeline.Reset();
        desc.CachedPSO={};
    }
    if(FAILED(device->CreateGraphicsPipelineState(
            &desc,IID_PPV_ARGS(pipeline.ReleaseAndGetAddressOf())))) {
        // If even the uncached PSO failed, a cached shader may be corrupted.
        if(has_vs || has_ps) {
            if(!compile("vertex_main","vs_5_0",vs,report.error) ||
               !compile("pixel_main","ps_5_0",ps,report.error)) return false;
            desc=pipeline_desc(signature.Get(),vs,ps,input);
            if(FAILED(device->CreateGraphicsPipelineState(
                &desc,IID_PPV_ARGS(pipeline.ReleaseAndGetAddressOf())))) {
                report.error=L"DirectX 12 graphics pipeline compilation failed";
                return false;
            }
        } else {
            report.error=L"DirectX 12 graphics pipeline compilation failed";
            return false;
        }
    }
    notify(88,L"Saving GPU-specific shader cache");
    if(!dir.empty()) {
        bool persisted=true;
        if(!has_vs || report.driver_cache_invalidated)
            persisted=write(vs_path,vs.data(),vs.size()) && persisted;
        if(!has_ps || report.driver_cache_invalidated)
            persisted=write(ps_path,ps.data(),ps.size()) && persisted;
        Microsoft::WRL::ComPtr<ID3DBlob> pso_blob;
        if(SUCCEEDED(pipeline->GetCachedBlob(pso_blob.GetAddressOf())) &&
           pso_blob && pso_blob->GetBufferSize()>0 &&
           pso_blob->GetBufferSize()<=kMaxPipelineBytes) {
            persisted=write(pso_path,pso_blob->GetBufferPointer(),
                            pso_blob->GetBufferSize()) && persisted;
        } else persisted=false;
        report.written_to_disk=persisted;
    }
    notify(100,L"Shader optimization complete");
    return true;
}
} // namespace kami::world
