// Copyright (C) 2026 Kombatant Viewer contributors
// SPDX-License-Identifier: MIT
// AMD FSR and Intel XeSS are optional SDK-backed temporal upscalers.
// This module only records compute commands into the caller's D3D12 command list.
#pragma once
#include <windows.h>
#include <d3d12.h>
#include <filesystem>
#include <string>
#include <sstream>
#include <stdint.h>
#include "ffx_api.h"
#include "dx12/ffx_api_dx12.h"
#include "ffx_upscale.h"
#include "xess/xess_d3d12.h"

class KombatantAdditionalUpscalers final
{
    HMODULE amdLoader = nullptr, intelLoader = nullptr;
    PfnFfxCreateContext amdCreate = nullptr;
    PfnFfxDestroyContext amdDestroy = nullptr;
    PfnFfxDispatch amdDispatch = nullptr;
    ffxContext amdContext = nullptr;
    decltype(&xessD3D12CreateContext) intelCreate = nullptr;
    decltype(&xessD3D12Init) intelInit = nullptr;
    decltype(&xessD3D12Execute) intelExecute = nullptr;
    decltype(&xessDestroyContext) intelDestroy = nullptr;
    decltype(&xessSetVelocityScale) intelVelocity = nullptr;
    xess_context_handle_t intelContext = nullptr;
    uint32_t currentProvider = 0, currentMode = 0;
    uint32_t currentOutputWidth = 0, currentOutputHeight = 0;
    std::string diagnostic = "FSR/XeSS idle";

    static std::string code(const char* operation, uint32_t result)
    {
        std::ostringstream out;
        out << operation << " returned 0x" << std::hex << result;
        return out.str();
    }
    template<typename T> static T symbol(HMODULE module, const char* name)
    {
        return reinterpret_cast<T>(module ? GetProcAddress(module, name) : nullptr);
    }
    static HMODULE openLibrary(const std::filesystem::path& dir, const wchar_t* name)
    {
        const auto path = dir / name;
        return LoadLibraryExW(path.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    }
    void clearContexts()
    {
        // Called only between frames, after the viewer's GPU fence completed.
        if (amdContext && amdDestroy) amdDestroy(&amdContext, nullptr);
        amdContext = nullptr;
        if (intelContext && intelDestroy) intelDestroy(intelContext);
        intelContext = nullptr;
        currentProvider = currentMode = 0;
    }
public:
    const std::string& status() const { return diagnostic; }

    // provider: 2=AMD FSR, 3=Intel XeSS; 1=DLSS handled by the existing backend.
    bool prepare(ID3D12Device* device, const std::filesystem::path& dir,
                 uint32_t provider, uint32_t mode, uint32_t inputWidth,
                 uint32_t inputHeight, uint32_t outputWidth, uint32_t outputHeight)
    {
        if (provider == currentProvider && mode == currentMode &&
            currentOutputWidth == outputWidth && currentOutputHeight == outputHeight &&
            ((provider == 2 && amdContext) || (provider == 3 && intelContext)))
            return true;

        clearContexts();
        currentOutputWidth = outputWidth;
        currentOutputHeight = outputHeight;
        if (provider == 2)
        {
            if (!amdLoader) amdLoader = openLibrary(dir, L"amd_fidelityfx_loader_dx12.dll");
            if (!amdLoader) { diagnostic = "AMD FSR loader DLL missing"; return false; }
            amdCreate = symbol<PfnFfxCreateContext>(amdLoader, "ffxCreateContext");
            amdDestroy = symbol<PfnFfxDestroyContext>(amdLoader, "ffxDestroyContext");
            amdDispatch = symbol<PfnFfxDispatch>(amdLoader, "ffxDispatch");
            if (!amdCreate || !amdDestroy || !amdDispatch)
            { diagnostic = "AMD FSR API exports missing"; return false; }

            ffxCreateBackendDX12Desc backend{};
            backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
            backend.device = device;
            ffxCreateContextDescUpscale desc{};
            desc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
            desc.header.pNext = &backend.header;
            desc.flags = FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
            desc.maxRenderSize = {inputWidth, inputHeight};
            desc.maxUpscaleSize = {outputWidth, outputHeight};
            const auto result = amdCreate(&amdContext, &desc.header, nullptr);
            if (result != FFX_API_RETURN_OK)
            { diagnostic = code("AMD FSR context creation", result); return false; }
        }
        else if (provider == 3)
        {
            if (!intelLoader) intelLoader = openLibrary(dir, L"libxess.dll");
            if (!intelLoader) { diagnostic = "Intel XeSS runtime DLL missing"; return false; }
            intelCreate = symbol<decltype(intelCreate)>(intelLoader, "xessD3D12CreateContext");
            intelInit = symbol<decltype(intelInit)>(intelLoader, "xessD3D12Init");
            intelExecute = symbol<decltype(intelExecute)>(intelLoader, "xessD3D12Execute");
            intelDestroy = symbol<decltype(intelDestroy)>(intelLoader, "xessDestroyContext");
            intelVelocity = symbol<decltype(intelVelocity)>(intelLoader, "xessSetVelocityScale");
            if (!intelCreate || !intelInit || !intelExecute || !intelDestroy || !intelVelocity)
            { diagnostic = "Intel XeSS API exports missing"; return false; }
            auto result = intelCreate(device, &intelContext);
            if (result != XESS_RESULT_SUCCESS)
            { diagnostic = code("Intel XeSS context creation", result); return false; }
            xess_d3d12_init_params_t init{};
            init.outputResolution = {outputWidth, outputHeight};
            init.qualitySetting = mode == 4 ? XESS_QUALITY_SETTING_ULTRA_PERFORMANCE :
                mode == 3 ? XESS_QUALITY_SETTING_PERFORMANCE :
                mode == 2 ? XESS_QUALITY_SETTING_BALANCED : XESS_QUALITY_SETTING_QUALITY;
            init.initFlags = XESS_INIT_FLAG_LDR_INPUT_COLOR | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;
            result = intelInit(intelContext, &init);
            if (result != XESS_RESULT_SUCCESS)
            { diagnostic = code("Intel XeSS initialization", result); clearContexts(); return false; }
            result = intelVelocity(intelContext, 1.0f, 1.0f);
            if (result != XESS_RESULT_SUCCESS)
            { diagnostic = code("Intel XeSS motion scaling", result); clearContexts(); return false; }
        }
        else { diagnostic = "Unknown FSR/XeSS provider"; return false; }
        currentProvider = provider; currentMode = mode;
        diagnostic = provider == 2 ? "AMD FSR initialized" : "Intel XeSS initialized";
        return true;
    }

    bool evaluate(uint32_t provider, ID3D12GraphicsCommandList* commandList,
                  ID3D12Resource* inputColor, ID3D12Resource* depth,
                  ID3D12Resource* motion, ID3D12Resource* output,
                  uint32_t width, uint32_t height, uint32_t targetWidth,
                  uint32_t targetHeight, float jitterX, float jitterY,
                  float elapsedMilliseconds, bool reset)
    {
        if (provider == 2 && amdContext && amdDispatch)
        {
            ffxDispatchDescUpscale desc{};
            desc.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
            desc.commandList = commandList;
            desc.color = ffxApiGetResourceDX12(inputColor);
            desc.depth = ffxApiGetResourceDX12(depth);
            desc.motionVectors = ffxApiGetResourceDX12(motion);
            desc.output = ffxApiGetResourceDX12(output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
            desc.renderSize = {width, height};
            desc.upscaleSize = {targetWidth, targetHeight};
            desc.jitterOffset = {jitterX, jitterY};
            desc.motionVectorScale = {1.f, 1.f}; // Source vectors are already in pixels.
            desc.frameTimeDelta = elapsedMilliseconds;
            desc.preExposure = 1.f;
            desc.reset = reset;
            desc.cameraNear = 0.1f;
            desc.cameraFar = 1024.f;
            desc.cameraFovAngleVertical = 1.0f;
            desc.viewSpaceToMetersFactor = 1.f;
            desc.flags = FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB;
            const auto result = amdDispatch(&amdContext, &desc.header);
            if (result != FFX_API_RETURN_OK)
            { diagnostic = code("AMD FSR dispatch", result); return false; }
            diagnostic = "AMD FSR running";
            return true;
        }
        if (provider == 3 && intelContext && intelExecute)
        {
            xess_d3d12_execute_params_t desc{};
            desc.pColorTexture = inputColor;
            desc.pDepthTexture = depth;
            desc.pVelocityTexture = motion;
            desc.pOutputTexture = output;
            desc.jitterOffsetX = jitterX;
            desc.jitterOffsetY = jitterY;
            desc.inputWidth = width;
            desc.inputHeight = height;
            desc.exposureScale = 1.f;
            desc.resetHistory = reset ? 1u : 0u;
            const auto result = intelExecute(intelContext, commandList, &desc);
            if (result != XESS_RESULT_SUCCESS)
            { diagnostic = code("Intel XeSS execute", result); return false; }
            diagnostic = "Intel XeSS running";
            return true;
        }
        diagnostic = "Selected upscaler not initialized";
        return false;
    }
};
