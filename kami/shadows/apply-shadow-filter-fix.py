#!/usr/bin/env python3
"""Wire shadow-only filtering and sample controls into the deferred light pass."""
from pathlib import Path
R=Path(__file__).resolve().parents[2]
def edit(path,a,b):
 p=R/path;s=p.read_text(encoding='utf-8-sig');assert s.count(a)==1,(path,s.count(a));p.write_text(s.replace(a,b),encoding='utf-8')
p='indra/newview/pipeline.cpp'
edit(p,'if (RenderDeferredSSAO && !gCubeSnapshot)\n        {\n            // soften direct lighting lightmap','if (!gCubeSnapshot && (RenderDeferredSSAO || (RenderDeferredBlurLight && RenderShadowDetail > 0)))\n        {\n            // soften direct lighting lightmap')
edit(p,'const U32 kern_length = 4;','const U32 kern_length = llclamp(gSavedSettings.getU32("RenderShadowBlurSamples"), 2U, 8U);')
edit(p,'                x += 1.f;','                x += 3.f / (kern_length - 1);')
edit(p,'gDeferredBlurLightProgram.uniform1f(sKernScale, blur_size * (kern_length / 2.f - 0.5f));','''gDeferredBlurLightProgram.uniform1f(sKernScale, blur_size * 1.5f);
            static LLStaticHashedString kern_count("kern_count");
            static LLStaticHashedString soften_shadows("soften_shadows");
            gDeferredBlurLightProgram.uniform1i(kern_count, kern_length);
            gDeferredBlurLightProgram.uniform1i(soften_shadows, RenderDeferredBlurLight ? 1 : 0);''')
p='indra/newview/app_settings/shaders/class1/deferred/blurLightF.glsl'
edit(p,'uniform vec3 kern[4];','uniform vec3 kern[8];\nuniform int kern_count;\nuniform int soften_shadows;')
f=R/p;s=f.read_text();start=s.index('    // TODO: move this to kern');end=s.index('    for (int i = 1; i < 7; i++)',start)
s=s[:start]+'''    // Interpolate the uploaded kernel while retaining the legacy four-tap shape.
    vec3 k[15];
    for (int i = 0; i < 15; ++i)
    {
        int index = i / 2;
        k[i] = (i % 2 == 0) ? kern[index] : (kern[index] + kern[index+1]) * 0.5;
    }

'''+s[end:];s=s.replace('i < 7;','i < kern_count * 2 - 1;');s=s.replace('    frag_color = max(col, vec4(0));','    // AO still gets its existing smoothing when shadow softening is disabled.\n    if (soften_shadows == 0) col.r = ccol.r;\n    frag_color = max(col, vec4(0));');f.write_text(s,encoding='utf-8')
print('Shadow-only filtering, enabled-state semantics and 2-8 kernel sample controls wired.')
