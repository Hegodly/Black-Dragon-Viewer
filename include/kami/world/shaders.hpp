#pragma once
// Original KAMI HLSL shading: per-vertex roughness/metallicity, GGX BRDF,
// and procedural non-networked avatar animation. No third-party shader code.
namespace kami::world {
inline constexpr char kSceneShader[] = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 viewProjection;
    float4 sunDirectionIntensity;
    float4 cameraWorldPosition;
    float4 frame; // x=time seconds, y=avatar enabled, z=PBR enabled
};
struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 material : MATERIAL;
};
struct Interpolant {
    float4 clip : SV_POSITION;
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 albedo : TEXCOORD2;
    float2 material : TEXCOORD3;
};
Interpolant vertex_main(VertexIn input) {
    Interpolant output;
    float3 world=input.position;
    if (frame.y > 0.5)
        world.y += 0.055 * sin(frame.x * 2.0 + input.position.y * 0.25);
    output.clip = mul(float4(world, 1.0), viewProjection);
    output.position = world;
    output.normal = input.normal;
    output.albedo = input.color;
    output.material = input.material;
    return output;
}
static const float PI = 3.14159265359;
float3 fresnel_schlick(float cosine, float3 base) {
    return base+(1.0-base)*pow(saturate(1.0-cosine),5.0);
}
float ggx_distribution(float NdotH, float r) {
    float a=r*r;
    float a2=a*a;
    float d=NdotH*NdotH*(a2-1.0)+1.0;
    return a2/max(PI*d*d,0.0001);
}
float smith_visibility(float NdotX, float k) {
    return NdotX/(NdotX*(1.0-k)+k);
}
float4 pixel_main(Interpolant input) : SV_TARGET {
    float3 N=normalize(input.normal);
    float3 L=normalize(-sunDirectionIntensity.xyz);
    float3 V=normalize(cameraWorldPosition.xyz-input.position);
    float3 H=normalize(L+V);
    float NdotL=saturate(dot(N,L));
    float NdotV=max(saturate(dot(N,V)),0.001);
    float NdotH=saturate(dot(N,H));
    float VdotH=saturate(dot(V,H));
    float3 base=saturate(input.albedo);
    float3 radiance=float3(1.25,1.18,1.08)*sunDirectionIntensity.w;
    float3 color;
    if(frame.z<0.5) {
        color=base*(0.16+0.84*NdotL)*sunDirectionIntensity.w;
    } else {
        float metallic=saturate(input.material.x);
        float roughness=clamp(input.material.y,0.07,1.0);
        float3 F0=lerp(float3(0.04,0.04,0.04),base,metallic);
        float3 F=fresnel_schlick(VdotH,F0);
        float a=(roughness+1.0);
        float k=(a*a)/8.0;
        float G=smith_visibility(NdotV,k)*smith_visibility(NdotL,k);
        float D=ggx_distribution(NdotH,roughness);
        float3 specular=D*G*F/max(4.0*NdotV*NdotL,0.001);
        float3 diffuse=(1.0-F)*(1.0-metallic)*base/PI;
        color=base*0.075+(diffuse+specular)*radiance*NdotL;
    }
    // Basic filmic output mapping while maintaining SDR UNORM compatibility.
    color=color/(color+float3(1.0,1.0,1.0));
    color=pow(max(color,0.0),1.0/2.2);
    return float4(saturate(color),1.0);
}
)";
}
