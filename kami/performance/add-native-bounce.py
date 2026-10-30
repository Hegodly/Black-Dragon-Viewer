#!/usr/bin/env python3
"""Optional native near-field indirect color-bounce prototype for PBR.

This is intentionally NOT claimed as full SSGI. It integrates a small-screen
neighborhood geometry-aware irradiance estimate into the existing PBR lighting
pass; quality is experimental and it is disabled by default.
"""
from pathlib import Path
R=Path(__file__).resolve().parents[2]

def edit(path, before, after):
    p=R/path
    s=p.read_text(encoding="utf-8-sig")
    if s.count(before)!=1:
        raise RuntimeError(f"{path}: expected 1 match, got {s.count(before)} for {before[:80]!r}")
    p.write_text(s.replace(before,after,1),encoding="utf-8")
    print("Updated",path)

settings="indra/newview/app_settings/settings_blackdragon.xml"
key='''  <key>KAMINearFieldGI</key>
  <map><key>Comment</key><string>Experimental PBR-only screen-space near-field color-bounce approximation; not full SSGI. Off by default.</string><key>Persist</key><integer>1</integer><key>Type</key><string>Boolean</string><key>Value</key><integer>0</integer></map>
  <key>KAMINearFieldGIStrength</key>
  <map><key>Comment</key><string>Strength of experimental native PBR near-field color bounce, 0 to 0.5.</string><key>Persist</key><integer>1</integer><key>Type</key><string>F32</string><key>Value</key><real>0.15</real></map>
'''
edit(settings,'  <key>KAMIDynamicResolution</key>',key+'  <key>KAMIDynamicResolution</key>')

shader="indra/newview/app_settings/shaders/class3/deferred/softenLightF.glsl"
func='''// Optional four-tap near-field screen-space indirect color estimate.
// This is a bounded PBR-only approximation and not a ray-traced SSGI solution.
// Occlusion, temporal stability, screen edges and behind-camera sources remain
// limitations; keep disabled outside photography / explicit experiments.
uniform float kami_near_field_gi;
vec3 kamiNearFieldBounce(vec2 uv, vec3 here, vec3 normal)
{
    if (kami_near_field_gi < 0.001) return vec3(0.0);
    // 4 pixels in each direction; avoids unbounded texture fetches.
    const vec2 directions[4] = vec2[4](
       vec2(-1.0, 0.0), vec2(1.0, 0.0),
       vec2(0.0, -1.0), vec2(0.0, 1.0));
    vec3 bounce = vec3(0.0);
    float weight = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        vec2 sampleUV = uv + directions[i] * 10.0 / screen_res;
        if (any(lessThanEqual(sampleUV, vec2(0.002))) ||
            any(greaterThanEqual(sampleUV, vec2(0.998)))) continue;
        float neighborDepth = getDepth(sampleUV);
        if (neighborDepth >= 0.999) continue;
        vec3 neighborPos = getPositionWithDepth(sampleUV, neighborDepth).xyz;
        vec3 delta = neighborPos - here;
        float dist = length(delta);
        if (dist < 0.015 || dist > 2.5) continue;
        vec3 dir = delta / dist;
        GBufferInfo sampleMaterial = getGBuffer(sampleUV);
        // Do not bounce colors from sky/unlit or blank GBuffer pixels.
        if (!GET_GBUFFER_FLAG(sampleMaterial.gbufferFlag, GBUFFER_FLAG_HAS_PBR)) continue;
        float facing = max(dot(normal, dir), 0.0) *
                       max(dot(sampleMaterial.normal, -dir), 0.0);
        float w = facing / (1.0 + 4.0 * dist * dist);
        bounce += max(sampleMaterial.albedo.rgb, vec3(0.0)) * w;
        weight += w;
    }
    // Bounded added irradiance: avoids heavily tinting skin or white walls.
    return clamp(bounce / max(weight, 2.0), vec3(0.0), vec3(0.2)) *
           clamp(kami_near_field_gi, 0.0, 0.5);
}
'''
edit(shader,'void main()\n{',func+'\nvoid main()\n{')
edit(shader,'        adjustIrradiance(irradiance, ambocc);\n\n        vec3 diffuseColor;',
            '        adjustIrradiance(irradiance, ambocc);\n        irradiance += kamiNearFieldBounce(tc, pos.xyz, gb.normal);\n\n        vec3 diffuseColor;')

# Bind user setting once per existing native deferred-lighting pass.
renderer="indra/newview/pipeline.cpp"
before='''            soften_shader.uniform1f(ssao_scale_str, ssao_scale);
            soften_shader.uniform1f(ssao_max_str, ssao_max);
'''
after=before+'''            static LLStaticHashedString kami_bounce_uniform("kami_near_field_gi");
            static LLCachedControl<bool> kami_bounce_enabled(gSavedSettings, "KAMINearFieldGI", false);
            static LLCachedControl<F32> kami_bounce_strength(gSavedSettings, "KAMINearFieldGIStrength", 0.15f);
            // Native shader path. The experimental bounce cannot run in FPS mode.
            const F32 bounce = kami_bounce_enabled && !gSavedSettings.getBOOL("KAMIPerformanceMode")
                ? llclamp((F32)kami_bounce_strength, 0.f, 0.5f) : 0.f;
            soften_shader.uniform1f(kami_bounce_uniform, bounce);
'''
edit(renderer,before,after)

# Expose experiment next to actual native lighting controls, not as "SSGI".
ui="indra/newview/skins/default/xui/en/floater_kombatant_effects.xml"
p=R/ui
s=p.read_text(encoding="utf-8-sig")
old=' <text name="kami_gfx_warning" left="24" top="668" width="512" height="46" wrap="true">HBAO requires ambient occlusion enabled. Realtime reflection probes and HQ DoF are expensive; disable them for gameplay FPS.</text>'
new=''' <text name="kami_gfx_warning" left="24" top="668" width="512" height="46" wrap="true">HBAO needs AO enabled; realtime probes and HQ DoF cost significant FPS.</text>
 <check_box name="kami_bounce_toggle" label="Experimental native color bounce (PBR only)" control_name="KAMINearFieldGI" left="24" top="715" width="512" height="22" />
 <slider name="kami_bounce_strength" label="Color bounce strength" control_name="KAMINearFieldGIStrength" left="24" top="746" width="512" height="20" label_width="190" min_val="0" max_val="0.5" increment="0.025" decimal_digits="3" />
 <text name="kami_bounce_warning" left="24" top="779" width="512" height="54" wrap="true">Early 4-tap indirect-light approximation. May cause light leaks and additional GPU cost. Not full SSGI, not enabled by default.</text>'''
if s.count(old)!=1:raise RuntimeError("Advanced lighting panel anchor moved")
s=s.replace(old,new,1).replace('height="740"','height="860"',1)
p.write_text(s,encoding="utf-8")

assert 'kamiNearFieldBounce' in (R/shader).read_text(encoding="utf-8")
assert 'kami_bounce_uniform' in (R/renderer).read_text(encoding="utf-8")
print("KAMI: four-tap native PBR near-field bounce experiment (off by default) integrated.")
