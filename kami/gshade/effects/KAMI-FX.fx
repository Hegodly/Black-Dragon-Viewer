/*
 * KAMI Beyond Reality — Fire Garden FX.
 * Original permissively licensed KAMI shaders. Copyright (c) 2026 KAMI
 * contributors. SPDX-License-Identifier: MIT
 * This is NOT GShade or a replica of any paid/proprietary shader pack.
 * All techniques are optional; the default shipped preset enables none.
 */
#define KAMI_PX float2(BUFFER_RCP_WIDTH, BUFFER_RCP_HEIGHT)

texture KAMIBack : COLOR;
sampler KAMISource { Texture = KAMIBack; MinFilter = LINEAR; MagFilter = LINEAR; AddressU = CLAMP; AddressV = CLAMP; };
texture KAMIDepth : DEPTH;
sampler KAMIDepthSampler { Texture = KAMIDepth; MinFilter = POINT; MagFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; };

uniform float KAMIExposure < ui_type = "slider"; ui_label="Exposure"; ui_min=-2.0; ui_max=2.0; ui_step=0.05; > = 0.0;
uniform float KAMIVibrance < ui_type = "slider"; ui_label="Vibrance"; ui_min=-1.0; ui_max=1.0; ui_step=0.05; > = 0.20;
uniform float KAMIContrast < ui_type = "slider"; ui_label="Contrast"; ui_min=0.6; ui_max=1.6; ui_step=0.02; > = 1.06;
uniform float KAMIBrightness < ui_type = "slider"; ui_label="Brightness"; ui_min=-0.3; ui_max=0.3; ui_step=0.01; > = 0.0;
uniform float KAMISaturation < ui_type = "slider"; ui_label="Color saturation"; ui_min=0.0; ui_max=2.0; ui_step=0.05; > = 1.0;
uniform float3 KAMICast < ui_type = "color"; ui_label="Color tint"; > = float3(1.0,1.0,1.0);
uniform float KAMIVignette < ui_type = "slider"; ui_label="Vignette"; ui_min=0.0; ui_max=1.0; ui_step=0.02; > = 0.28;
uniform float KAMIGrain < ui_type = "slider"; ui_label="Film grain"; ui_min=0.0; ui_max=0.15; ui_step=0.005; > = 0.025;
uniform float KAMISharpness < ui_type = "slider"; ui_label="Sharpen amount"; ui_min=0.0; ui_max=1.5; ui_step=0.05; > = 0.40;
uniform float KAMIGlow < ui_type = "slider"; ui_label="Soft bloom / glow"; ui_min=0.0; ui_max=1.5; ui_step=0.05; > = 0.40;
uniform float KAMISoftness < ui_type = "slider"; ui_label="Softness"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.15;
uniform float KAMIChromatic < ui_type = "slider"; ui_label="Chromatic split (subtle)"; ui_min=0.0; ui_max=2.0; ui_step=0.1; > = 0.2;
uniform float KAMIClarity < ui_type = "slider"; ui_label="Local contrast"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.30;
uniform float KAMISepiaAmount < ui_type = "slider"; ui_label="Sepia strength"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.55;
uniform float KAMIMonoAmount < ui_type = "slider"; ui_label="Monochrome strength"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 1.0;
uniform float KAMILetterbox < ui_type = "slider"; ui_label="Cinematic bars"; ui_min=0.0; ui_max=0.20; ui_step=0.01; > = 0.08;
uniform float KAMIPosterize < ui_type = "slider"; ui_label="Posterize levels"; ui_min=3.0; ui_max=32.0; ui_step=1.0; > = 12.0;
uniform float KAMIDitherAmount < ui_type = "slider"; ui_label="Dither amount"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.2;
uniform float KAMIOutline < ui_type = "slider"; ui_label="Edge amount"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.20;
uniform float KAMI_FogStrength < ui_type = "slider"; ui_label="Depth fog amount (requires valid depth)"; ui_min=0.0; ui_max=1.0; ui_step=0.05; > = 0.25;

void KAMIFullscreenVS(uint id : SV_VertexID, out float4 position : SV_Position, out float2 texcoord : TEXCOORD)
{
    texcoord=float2((id<<1)&2,id&2);
    position=float4(texcoord*float2(2.0,-2.0)+float2(-1.0,1.0),0.0,1.0);
}
float3 KAMIRead(float2 uv) { return tex2D(KAMISource,clamp(uv,0.0,1.0)).rgb; }
float3 KAMILuma(float3 c) { float l=dot(c,float3(0.2126,0.7152,0.0722)); return float3(l,l,l); }
float3 KAMIBlur(float2 uv)
{
    float2 px=KAMI_PX;
    return (KAMIRead(uv)*4.0+KAMIRead(uv+float2(px.x,0.0))*2.0+
        KAMIRead(uv-float2(px.x,0.0))*2.0+KAMIRead(uv+float2(0.0,px.y))*2.0+
        KAMIRead(uv-float2(0.0,px.y))*2.0+
        KAMIRead(uv+px)+KAMIRead(uv-px)+
        KAMIRead(uv+float2(px.x,-px.y))+KAMIRead(uv+float2(-px.x,px.y)))/16.0;
}
float4 PExposure(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target { return float4(saturate(KAMIRead(uv)*exp2(KAMIExposure)),1); }
float4 PVibrance(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);float l=dot(c,float3(0.2126,0.7152,0.0722));
    float peak=max(max(c.r,c.g),c.b), low=min(min(c.r,c.g),c.b);
    return float4(saturate(lerp(l.xxx,c,1.0+KAMIVibrance*(1.0-(peak-low)))),1);
}
float4 PContrast(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);
    return float4(saturate((c-0.5)*KAMIContrast+0.5+KAMIBrightness),1);
}
float4 PSaturation(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);return float4(saturate(lerp(KAMILuma(c),c,KAMISaturation)),1);
}
float4 PColorTint(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target { return float4(saturate(KAMIRead(uv)*KAMICast),1); }
float4 PFilmic(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=max(KAMIRead(uv),0.0);
    c=(c*(2.51*c+0.03))/(c*(2.43*c+0.59)+0.14);
    return float4(saturate(c),1);
}
float4 PVignette(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float2 p=(uv-0.5)*float2(BUFFER_WIDTH*BUFFER_RCP_HEIGHT,1.0);
    float shadow=smoothstep(0.06,0.50,dot(p,p));
    return float4(KAMIRead(uv)*(1.0-shadow*KAMIVignette),1);
}
float4 PGrain(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);
    float seed=frac(sin(dot(uv*float2(BUFFER_WIDTH,BUFFER_HEIGHT),float2(12.9898,78.233)))*43758.5453);
    return float4(saturate(c+(seed-0.5)*KAMIGrain),1);
}
float4 PSharpen(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv),blur=KAMIBlur(uv);
    return float4(saturate(c+(c-blur)*KAMISharpness),1);
}
float4 PSoftness(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    return float4(lerp(KAMIRead(uv),KAMIBlur(uv),KAMISoftness),1);
}
float4 PGlow(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv),b=KAMIBlur(uv);
    return float4(saturate(c+max(b-0.60,0.0)*KAMIGlow),1);
}
float4 PChromatic(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float2 p=(uv-0.5)*KAMIChromatic*KAMI_PX*3.0;
    float3 c=float3(KAMIRead(uv+p).r,KAMIRead(uv).g,KAMIRead(uv-p).b);
    return float4(c,1);
}
float4 PClarity(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv),blur=KAMIBlur(uv);
    float3 local=(c-blur)*KAMIClarity;
    return float4(saturate(c+local),1);
}
float4 PSepia(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv),warm=float3(
      dot(c,float3(0.393,0.769,0.189)),
      dot(c,float3(0.349,0.686,0.168)),
      dot(c,float3(0.272,0.534,0.131)));
    return float4(saturate(lerp(c,warm,KAMISepiaAmount)),1);
}
float4 PMonochrome(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);return float4(lerp(c,KAMILuma(c),KAMIMonoAmount),1);
}
float4 PLetterbox(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float inside=step(KAMILetterbox,uv.y)*step(KAMILetterbox,1.0-uv.y);
    return float4(KAMIRead(uv)*inside,1);
}
float4 PPosterize(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float n=max(3.0,KAMIPosterize),c=0.5/n;
    return float4(floor(KAMIRead(uv)*n)*1.0/n+c,1);
}
float4 PDither(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv);
    float n=frac(sin(dot(floor(uv*float2(BUFFER_WIDTH,BUFFER_HEIGHT)),float2(57.0,113.0)))*438.5)-0.5;
    return float4(saturate(c+n*KAMIDitherAmount/255.0),1);
}
float4 POutline(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    float3 c=KAMIRead(uv),blur=KAMIBlur(uv);
    float edge=saturate(length(c-blur)*KAMIOutline*4.0);
    return float4(saturate(c*(1.0-edge)),1);
}
float4 PDepthFog(float4 pos:SV_Position,float2 uv:TEXCOORD):SV_Target
{
    // Diagnostic approximation: depth conventions vary by OpenGL game.
    // Disabled by default; manually verify depth in Home overlay.
    float depth=tex2D(KAMIDepthSampler,uv).r;
    float fog=saturate(depth*KAMI_FogStrength);
    return float4(lerp(KAMIRead(uv),float3(0.35,0.42,0.50),fog),1);
}

// Every feature is independent; user controls activation in the Home overlay.
// Depth effects are never auto-enabled; render latency depends on number of passes.
#define KAMI_PASS(NAME,PS) technique NAME { pass KAMIPass { VertexShader = KAMIFullscreenVS; PixelShader = PS; } }
KAMI_PASS(KAMI_Exposure,PExposure)
KAMI_PASS(KAMI_Vibrance,PVibrance)
KAMI_PASS(KAMI_Contrast,PContrast)
KAMI_PASS(KAMI_Saturation,PSaturation)
KAMI_PASS(KAMI_ColorTint,PColorTint)
KAMI_PASS(KAMI_FilmicToneCurve,PFilmic)
KAMI_PASS(KAMI_Vignette,PVignette)
KAMI_PASS(KAMI_FilmGrain,PGrain)
KAMI_PASS(KAMI_Sharpen,PSharpen)
KAMI_PASS(KAMI_SoftFocus,PSoftness)
KAMI_PASS(KAMI_SoftBloom,PGlow)
KAMI_PASS(KAMI_ChromaticAberration,PChromatic)
KAMI_PASS(KAMI_Clarity,PClarity)
KAMI_PASS(KAMI_Sepia,PSepia)
KAMI_PASS(KAMI_Monochrome,PMonochrome)
KAMI_PASS(KAMI_Letterbox,PLetterbox)
KAMI_PASS(KAMI_Posterize,PPosterize)
KAMI_PASS(KAMI_Dither,PDither)
KAMI_PASS(KAMI_Outline,POutline)
KAMI_PASS(KAMI_DepthFog_Experimental,PDepthFog)
