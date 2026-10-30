// Copyright (C) 2026 KAMI contributors. SPDX-License-Identifier: LGPL-2.1-only
// Uses the BSD-3-Clause ReShade API by Patrick Mours. No MXAO source is bundled.
#pragma once
#include "../third_party/reshade/include/reshade.hpp"
#include <filesystem>
namespace KAMIMXAO {
inline HMODULE module=nullptr;
inline HWND window=nullptr;
inline reshade::api::effect_runtime* runtime=nullptr;
inline bool attempted=false;
inline std::string status="Private MXAO runtime not installed.";
inline void found(reshade::api::effect_runtime* value) {
    if(value->get_hwnd()!=window) return;
    if(runtime!=value) {runtime=value;runtime->set_effects_state(false);}
}
inline void destroyed(reshade::api::effect_runtime* value) {
    if(runtime==value){runtime=nullptr;status="Waiting for OpenGL context.";}
}
inline void initialize() {
    if(attempted)return;attempted=true;
    if(GetEnvironmentVariableW(L"KAMI_DISABLE_MXAO",nullptr,0))return;
    wchar_t executable[32768]={};
    DWORD count=GetModuleFileNameW(nullptr,executable,32768);
    if(!count||count>=32768)return;
    const auto base=std::filesystem::path(executable).parent_path();
    std::error_code ec;
    // Explicit personal-install marker: normal builds never load any hooks.
    if(!std::filesystem::is_regular_file(base/L"KAMI_MXAO_PRIVATE.enabled",ec))return;
    if(std::filesystem::exists(base/L"opengl32.dll",ec)) {
        status="External ReShade/GShade detected. Use its MXAO; private runtime skipped.";return;
    }
    module=LoadLibraryExW((base/L"KAMIMXAO64.dll").c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){status="KAMIMXAO64.dll failed to load; native rendering retained.";return;}
    if(!reshade::register_addon(GetModuleHandleW(nullptr),module)) {
        status="Private MXAO runtime/API mismatch; native rendering retained.";return;
    }
    reshade::register_event<reshade::addon_event::init_effect_runtime>(found);
    reshade::register_event<reshade::addon_event::reshade_present>(found);
    reshade::register_event<reshade::addon_event::destroy_effect_runtime>(destroyed);
    status="Private runtime loaded; waiting for first OpenGL presentation.";
    // Never unload an active hook DLL. Process teardown owns its lifetime.
}
inline std::string configure(bool enabled,float amount,float radius,int quality,bool overlay) {
    if(!runtime)return status;
    if(overlay)runtime->open_overlay(true,reshade::api::input_source::mouse);
    if(!enabled){runtime->set_effects_state(false);return status="MXAO off.";}
    auto technique=runtime->find_technique("MartysMods_MXAO.fx","MartysMods_MXAO");
    if(!technique.handle){runtime->set_effects_state(false);
        return status="MXAO unavailable or compiling. Home overlay shows compiler/depth diagnostics.";}
    auto intensity=runtime->find_uniform_variable("MartysMods_MXAO.fx","MXAO_SSAO_AMOUNT");
    auto distance=runtime->find_uniform_variable("MartysMods_MXAO.fx","MXAO_SAMPLE_RADIUS");
    auto samples=runtime->find_uniform_variable("MartysMods_MXAO.fx","MXAO_GLOBAL_SAMPLE_QUALITY_PRESET");
    if(!intensity.handle||!distance.handle||!samples.handle){runtime->set_effects_state(false);
        return status="MXAO uniform contract mismatch; effect disabled.";}
    runtime->set_uniform_value_float(intensity,&amount,1);
    runtime->set_uniform_value_float(distance,&radius,1);
    int32_t q=quality;runtime->set_uniform_value_int(samples,&q,1);
    runtime->set_technique_state(technique,true);
    runtime->set_effects_state(true);
    return status="MXAO enabled. Verify correct depth and image quality in-world (Home overlay).";
}
}
