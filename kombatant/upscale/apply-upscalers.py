#!/usr/bin/env python3
"""Integrate optional AMD FSR and Intel XeSS after the baseline Kombatant patch.
Hard fail if source diverges, so releases never silently ship decorative controls.
"""
from pathlib import Path

def edit(file, before, after, count=1):
    p=Path(file)
    t=p.read_text(encoding="utf-8")
    if t.count(before)!=count:
        raise RuntimeError(f"{file}: expected {count}, found {t.count(before)} matches for {before[:70]!r}")
    p.write_text(t.replace(before,after),encoding="utf-8")

settings="indra/newview/app_settings/settings_blackdragon.xml"
edit(settings,'<key>KombatantDLSSMode</key><map><key>Comment</key><string>Experimental DLSS Super Resolution: 0 off, 1 quality, 2 balanced, 3 performance. Session only.</string><key>Persist</key><integer>0</integer><key>Type</key><string>U32</string><key>Value</key><integer>0</integer></map>',
'<key>KombatantUpscalerProvider</key><map><key>Comment</key><string>0 native, 1 DLSS, 2 FSR 3.1, 3 XeSS, defaults off.</string><key>Persist</key><integer>0</integer><key>Type</key><string>U32</string><key>Value</key><integer>0</integer></map>\n'
'  <key>KombatantUpscaleQuality</key><map><key>Comment</key><string>1 quality, 2 balanced, 3 performance, 4 ultra performance.</string><key>Persist</key><integer>0</integer><key>Type</key><string>U32</string><key>Value</key><integer>1</integer></map>\n'
'  <key>KombatantAutoPerformanceGuard</key><map><key>Comment</key><string>Automatically restore native rendering after repeated slow transfers.</string><key>Persist</key><integer>1</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>1</integer></map>')

ui="indra/newview/skins/default/xui/en/panel_preferences_graphics1.xml"
p=Path(ui)
t=p.read_text(encoding="utf-8")
a=t.index('       <text name="kd_rtx_heading"')
b=t.rfind('\n',0,t.index('</layout_panel>',a))+1
block='''       <text name="kd_rtx_heading" left="12" top="1224" width="465" height="24" font="SansSerifLarge">Experimental temporal upscaling and hardware AO</text>
       <combo_box name="kd_upscaler" control_name="KombatantUpscalerProvider" left="12" top="1254" width="465" height="24">
        <combo_box.item label="Native rendering (off by default)" value="0" />
        <combo_box.item label="NVIDIA DLSS Super Resolution" value="1" />
        <combo_box.item label="AMD FidelityFX Super Resolution 3.1" value="2" />
        <combo_box.item label="Intel XeSS Super Resolution" value="3" />
       </combo_box>
       <combo_box name="kd_upscale_quality" control_name="KombatantUpscaleQuality" left="12" top="1286" width="465" height="24">
        <combo_box.item label="Quality" value="1" />
        <combo_box.item label="Balanced" value="2" />
        <combo_box.item label="Performance" value="3" />
        <combo_box.item label="Ultra Performance (experimental)" value="4" />
       </combo_box>
       <check_box name="kd_guard" label="Automatically disable slow upscaling" control_name="KombatantAutoPerformanceGuard" left="12" top="1318" width="465" height="22" />
       <check_box name="kd_ray_ao" label="Hardware ray-traced ambient occlusion" control_name="KombatantRayAO" left="12" top="1350" width="465" height="22" />
       <slider name="kd_ray_radius" label="Ray AO distance (m)" control_name="KombatantRayAORadius" left="12" top="1380" width="465" height="20" label_width="190" min_val="0.05" max_val="10" increment="0.05" decimal_digits="2" />
       <slider name="kd_ray_strength" label="Ray AO strength" control_name="KombatantRayAOStrength" left="12" top="1408" width="465" height="20" label_width="190" min_val="0" max_val="1" increment="0.05" decimal_digits="2" />
       <text name="kd_rtx_status" left="12" top="1438" width="465" height="54" wrap="true">Native rendering active.</text>
       <text name="kd_rtx_help" left="12" top="1504" width="465" height="110" wrap="true">These SDK upscalers are optional experiments, not a guaranteed FPS boost. OpenGL/DirectX frame copies may cost substantial FPS. Enable the guard to revert automatically after repeated slow frames. 4096px output limit. Ray AO covers nearby opaque world meshes, not avatars or terrain.</text>
'''
t=t[:a]+block+t[b:]
assert 'height="1558" auto_resize="false" width="515"' in t
p.write_text(t.replace('height="1558" auto_resize="false" width="515"','height="1620" auto_resize="false" width="515"'),encoding="utf-8")
edit("indra/newview/llviewercontrol.cpp",'setting_setup_signal_listener(gSavedSettings, "KombatantDLSSMode", handleRenderResolutionDivisorChanged);',
'setting_setup_signal_listener(gSavedSettings, "KombatantUpscalerProvider", handleRenderResolutionDivisorChanged);\n    setting_setup_signal_listener(gSavedSettings, "KombatantUpscaleQuality", handleRenderResolutionDivisorChanged);')
state="indra/newview/kombatantrtxstate.h"
edit(state,'wantsDLSS()','wantsUpscaler()')
edit(state,'getU32("KombatantDLSSMode")!=0','getU32("KombatantUpscalerProvider")!=0')
for file in ["indra/newview/pipeline.cpp","indra/newview/llviewerdisplay.cpp","indra/newview/llviewercamera.cpp"]:
    p=Path(file)
    p.write_text(p.read_text(encoding="utf-8").replace("KombatantRTXState::wantsDLSS()","KombatantRTXState::wantsUpscaler()"),encoding="utf-8")
edit("indra/newview/pipeline.cpp",'const U32 mode=gSavedSettings.getU32("KombatantDLSSMode");\n        const F64 scale=mode==3?0.5:mode==2?0.58:2.0/3.0;',
'const U32 mode=gSavedSettings.getU32("KombatantUpscaleQuality");\n        const F64 scale=mode==4?1.0/3.0:mode==3?0.5:mode==2?0.58:2.0/3.0;')
edit("kombatant/rtx/rtx_api.h","#define KOMBATANT_RTX_ABI 1u","#define KOMBATANT_RTX_ABI 2u")
edit("kombatant/rtx/rtx_api.h","uint32_t dlss_mode; // 0 off, 1 quality, 2 balanced, 3 performance",
"uint32_t dlss_mode; // quality: 0 off, 1 quality, 2 balanced, 3 performance, 4 ultra\n    uint32_t upscaler_provider; // 0 native, 1 DLSS, 2 AMD FSR, 3 Intel XeSS")
edit("kombatant/rtx/rtx_math.h","f->dlss_mode<=3 &&","f->dlss_mode<=4 && f->upscaler_provider<=3 &&")
edit("kombatant/rtx/rtx_math_test.cpp","frame.dlss_mode=4;assert(!validFrame(&frame));frame.dlss_mode=1;assert(validFrame(&frame));",
"frame.dlss_mode=5;assert(!validFrame(&frame));frame.dlss_mode=4;assert(validFrame(&frame));frame.upscaler_provider=4;assert(!validFrame(&frame));frame.upscaler_provider=2;assert(validFrame(&frame));frame.upscaler_provider=1;")
edit("kombatant/rtx/rtx_smoke_test.cpp","frame.target_width=frame.target_height=64;frame.dlss_mode=1;frame.reset=1;",
"frame.target_width=frame.target_height=64;frame.dlss_mode=1;frame.upscaler_provider=1;frame.reset=1;")
edit("kombatant/rtx/Start-Without-RTX.cmd","--set KombatantDLSSMode 0","--set KombatantUpscalerProvider 0")
d=chr(36)
cmake="kombatant/rtx/CMakeLists.txt"
old='target_include_directories(KombatantRTX64 PRIVATE "'+d+'{NGX_ROOT}/include")'
new='target_include_directories(KombatantRTX64 PRIVATE "'+d+'{NGX_ROOT}/include" "'+d+'{FSR_ROOT}/Kits/FidelityFX/api/include" "'+d+'{FSR_ROOT}/Kits/FidelityFX/upscalers/include" "'+d+'{XESS_ROOT}/inc")'
edit(cmake,old,new)
edit("kombatant/build-rtx.ps1",'"-DNGX_ROOT=$sdk"','"-DNGX_ROOT=$sdk" "-DFSR_ROOT=$root/.fsr-sdk" "-DXESS_ROOT=$root/.xess-sdk"')
backend="kombatant/rtx/rtx_backend.cpp"
edit(backend,'#include "nvsdk_ngx_helpers.h"','#include "nvsdk_ngx_helpers.h"\n#include "../upscale/additional_upscalers.h"')
edit(backend,'uint32_t mode=0, iw=0, ih=0, ow=0, oh=0, geometryRevision=~0u;',
'uint32_t mode=0, provider=0, iw=0, ih=0, ow=0, oh=0, geometryRevision=~0u;\n    KombatantAdditionalUpscalers additional;')
edit(backend,'if(desc.VendorId!=0x10de || (desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)) continue;','if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) continue;')
edit(backend,'No NVIDIA D3D12 adapter found','No D3D12 adapter found')
edit(backend,'if(iw==f.width && ih==f.height && ow==f.target_width && oh==f.target_height && mode==f.dlss_mode) return;',
'if(iw==f.width && ih==f.height && ow==f.target_width && oh==f.target_height && mode==f.dlss_mode && provider==f.upscaler_provider) return;')
edit(backend,'iw=f.width;ih=f.height;ow=f.target_width;oh=f.target_height;mode=f.dlss_mode;',
'iw=f.width;ih=f.height;ow=f.target_width;oh=f.target_height;mode=f.dlss_mode;provider=f.upscaler_provider;')
edit(backend,'if(!mode || !dlssAvailable) return;','if(!mode || provider!=1 || !dlssAvailable) return;')
edit(backend,'params.Feature.InPerfQualityValue=mode==3?NVSDK_NGX_PerfQuality_Value_MaxPerf:',
'params.Feature.InPerfQualityValue=mode==4?NVSDK_NGX_PerfQuality_Value_UltraPerformance:mode==3?NVSDK_NGX_PerfQuality_Value_MaxPerf:')
edit(backend,'configure(f);begin();\n            uploadTexture(color,f.color);',
'''configure(f);
            bool extraReady=true;
            if(f.upscaler_provider>=2 && f.dlss_mode)
                extraReady=additional.prepare(device.Get(),modulePath,f.upscaler_provider,f.dlss_mode,f.width,f.height,f.target_width,f.target_height);
            begin();
            uploadTexture(color,f.color);''')
edit(backend,'if(f.dlss_mode && dlss)','if(f.dlss_mode && f.upscaler_provider==1 && dlss)')
edit(backend,'if(result) copyResult(*result);',
'''if(f.dlss_mode && f.upscaler_provider>=2 && extraReady)
            {
                transition(upscaled,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                if(additional.evaluate(f.upscaler_provider,commands.Get(),input->gpu.Get(),depth.gpu.Get(),motion.gpu.Get(),upscaled.gpu.Get(),iw,ih,ow,oh,f.jitter_x,f.jitter_y,f.frame_ms,f.reset!=0))
                { result=&upscaled;f.executed|=1;barrier(upscaled.gpu.Get()); }
            }
            if(result) copyResult(*result);''')
edit(backend,'s<<(f.executed&1?"DLSS running":dlssDiagnostic)<<" | ";',
's<<(f.executed&1?(f.upscaler_provider==1?"DLSS running":additional.status().c_str()):f.upscaler_provider==1?dlssDiagnostic:f.upscaler_provider>=2?additional.status():"Native rendering")<<" | ";')
viewer="indra/newview/kombatantrtx.h"
edit(viewer,'const U32 mode=gSavedSettings.getU32("KombatantDLSSMode");',
'const U32 provider=gSavedSettings.getU32("KombatantUpscalerProvider");\n        const U32 mode=provider?gSavedSettings.getU32("KombatantUpscaleQuality"):0;')
edit(viewer,'bool reset=!previousWidth || (mode && !hadDLSS) || mode!=previousMode',
'const U32 identity=provider*10+mode;\n            bool reset=!previousWidth || (mode && !hadDLSS) || identity!=previousMode')
edit(viewer,'frame.dlss_mode=mode;frame.ray_ao=ray;',
'frame.dlss_mode=mode;frame.upscaler_provider=provider;frame.ray_ao=ray;')
edit(viewer,'hadDLSS=ok && (frame.executed&1);previousMode=mode;',
'hadDLSS=ok && (frame.executed&1);previousMode=identity;')
p=Path(viewer)
t=p.read_text(encoding="utf-8").replace('KombatantDLSSMode','KombatantUpscalerProvider')
t=t.replace('inline bool hadDLSS=false;','inline bool hadDLSS=false;\n    inline U32 slowFrameCount=0;')
t=t.replace('auto now=std::chrono::steady_clock::now();','auto now=std::chrono::steady_clock::now();\n            const auto transferStart=now;')
needle='char diagnostic[1024]{};const int ok=process(&frame,diagnostic,sizeof(diagnostic));status(diagnostic);'
assert t.count(needle)==1
t=t.replace(needle,needle+'''
            // Disable repeated expensive GPU/CPU frame transfers only while upscaling.
            const float extraMs=std::chrono::duration<float,std::milli>(std::chrono::steady_clock::now()-transferStart).count();
            if(provider && gSavedSettings.getBOOL("KombatantAutoPerformanceGuard"))
            {
                slowFrameCount=extraMs>35.f?llmin(slowFrameCount+1,20U):0;
                if(slowFrameCount>=12)
                {
                    status("Upscaling disabled: repeated costly frame transfers. Native rendering restored.");
                    gSavedSettings.setU32("KombatantUpscalerProvider",0);
                    slowFrameCount=0;
                    return source;
                }
            }
            else slowFrameCount=0;''')
p.write_text(t,encoding="utf-8")
for f in (settings,ui,viewer,state,"indra/newview/pipeline.cpp"):
    assert "KombatantDLSSMode" not in Path(f).read_text(encoding="utf-8"),f
assert "KombatantAdditionalUpscalers additional" in Path(backend).read_text(encoding="utf-8")
print("Kombatant FSR/XeSS native SDK wiring and performance guard applied.")
