#pragma once
// Original KAMI HLSL shading: per-vertex roughness/metallicity, GGX BRDF,
// and procedural non-networked avatar animation. No third-party shader code.
namespace kami::world {
inline constexpr char kSceneShader[] = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 viewProjection;
    float4 sunDirectionIntensity;
    float4 cameraWorldPosition;
    float4 frame; // x=time seconds, y=avatar enabled, z=PBR enabled, w=shadows
    row_major float4x4 lightViewProjection;
    float4 kamiEffects; // x=haze, y=contact shading, z=metallic microglints
    float4 photoGrade; // x=exposure EV, y=contrast, z=saturation, w=vignette
    float4 photoViewport; // x=width, y=height, z=grading enabled, w=avatar gait
    float4 avatarWorld; // local test avatar feet: world x/y/z and heading
};
Texture2D<float> shadowDepth : register(t0);
SamplerComparisonState shadowComparison : register(s0);
Texture2D<float4> materialTexture : register(t1);
SamplerState materialSampler : register(s1);
struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 material : MATERIAL;
    float2 uv : TEXCOORD0;
};
struct Interpolant {
    float4 clip : SV_POSITION;
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 albedo : TEXCOORD2;
    float2 material : TEXCOORD3;
    float2 uv : TEXCOORD4;
};
// The same local avatar transformation is used in the depth/shadow and
// main passes; this keeps locomotion and shadows synchronized on the GPU.
float3 avatar_position(float3 local) {
    const float gait=photoViewport.w;
    const float phase=kamiEffects.w;
    const float lateral=local.x<0.0?1.0:-1.0;
    // Coarse demonstration gait for KAMI's original six-box mannequin.
    // A live SL avatar will require skinned joint transforms and animation assets.
    if(abs(local.x)<0.51 && local.y<1.22) {
        const float swing=sin(phase)*lateral*gait;
        local.z+=0.27*swing*saturate((1.32-local.y)/1.32);
        local.y+=0.07*abs(swing);
    } else if(abs(local.x)>0.53 && local.y>0.95 && local.y<2.5) {
        local.z-=0.24*sin(phase)*lateral*gait;
    }
    const float co=cos(avatarWorld.w),si=sin(avatarWorld.w);
    const float3 result=float3(
        co*local.x+si*local.z+avatarWorld.x,
        local.y+avatarWorld.y+frame.y,
        -si*local.x+co*local.z+avatarWorld.z);
    return result;
}
float3 avatar_normal(float3 n) {
    const float co=cos(avatarWorld.w),si=sin(avatarWorld.w);
    return float3(co*n.x+si*n.z,n.y,-si*n.x+co*n.z);
}
Interpolant vertex_main(VertexIn input) {
    Interpolant output;
    const float3 world=avatar_position(input.position);
    output.clip = mul(float4(world, 1.0), viewProjection);
    output.position = world;
    output.normal = avatar_normal(input.normal);
    output.albedo = input.color;
    output.material = input.material;
    output.uv = input.uv;
    return output;
}
float4 shadow_vertex_main(VertexIn input) : SV_POSITION {
    return mul(float4(avatar_position(input.position),1.0),lightViewProjection);
}
float shadow_factor(float3 position,float3 normal) {
    if(frame.w<0.5) return 1.0;
    const float4 clip=mul(float4(position,1.0),lightViewProjection);
    if(clip.w<=0.001) return 1.0;
    const float3 ndc=clip.xyz/clip.w;
    float2 uv=float2(ndc.x*0.5+0.5,0.5-ndc.y*0.5);
    if(uv.x<=0.0||uv.y<=0.0||uv.x>=1.0||uv.y>=1.0||
       ndc.z<=0.0||ndc.z>=1.0) return 1.0;
    const float normalLight=saturate(dot(normalize(normal),
                                       normalize(-sunDirectionIntensity.xyz)));
    const float bias=0.0015+0.003*(1.0-normalLight);
    float total=0.0;
    [unroll] for(int yy=-1;yy<=1;++yy)
        [unroll] for(int xx=-1;xx<=1;++xx)
            total+=shadowDepth.SampleCmpLevelZero(
                shadowComparison,uv+float2(xx,yy)/1024.0,ndc.z-bias);
    return total/9.0;
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
    // This is a real sampled GPU texture, procedurally uploaded by KAMI.
    // Later SL texture assets will enter through the same resource interface.
    float3 texel=materialTexture.Sample(materialSampler,input.uv).rgb;
    float3 base=saturate(input.albedo*lerp(0.72,1.12,texel));
    float visibility=shadow_factor(input.position,N);
    float3 radiance=float3(1.25,1.18,1.08)*sunDirectionIntensity.w;
    float3 color;
    if(frame.z<0.5) {
        color=base*(0.16+0.84*NdotL*visibility)*sunDirectionIntensity.w;
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
        color=base*0.075+(diffuse+specular)*radiance*NdotL*visibility;
    }
    // Original KAMI material-space contact shading. This uses existing per-pixel
    // world position/normal, not an incorrectly labelled screen-space AO pass.
    if(kamiEffects.y>0.5) {
        float lower=saturate(input.position.y*0.65);
        float contact=lerp(0.76,1.0,lower);
        color*=lerp(contact,1.0,0.12*saturate(N.y));
    }
    // GPU-native procedural specular microglints: no external overlay or injector.
    if(kamiEffects.z>0.5 && input.material.x>0.15) {
        float3 R=reflect(-L,N);
        float focused=pow(saturate(dot(R,V)),64.0);
        float sparkle=pow(saturate(sin(input.position.x*94.7+
                            input.position.y*117.3+input.position.z*51.8+
                            frame.x*1.4)*0.5+0.5),24.0);
        color+=float3(0.7,0.82,1.0)*focused*sparkle*
                    saturate(input.material.x)*0.26;
    }
    // KAMI distance-based atmosphere. Not volumetric ray marching.
    if(kamiEffects.x>0.5) {
        float distanceToCamera=length(cameraWorldPosition.xyz-input.position);
        float haze=1.0-exp(-distanceToCamera*0.016);
        color=lerp(color,float3(0.34,0.44,0.51),saturate(haze)*0.36);
    }
    // Entirely native DX12 KAMI Photo Studio color pipeline, scene linear.
    if(photoViewport.z>0.5) {
        color *= exp2(photoGrade.x);
        color=(color-0.18)*photoGrade.y+0.18;
        const float neutral=dot(color,float3(0.2126,0.7152,0.0722));
        color=lerp(float3(neutral,neutral,neutral),color,photoGrade.z);
        const float2 normalized=input.clip.xy/max(photoViewport.xy,float2(1.0,1.0));
        const float2 centered=(normalized-0.5)*2.0;
        const float radius=dot(centered,centered)*0.5;
        color*=1.0-saturate(radius)*photoGrade.w;
    }
    // Basic filmic output mapping while maintaining SDR UNORM compatibility.
    color=color/(color+float3(1.0,1.0,1.0));
    color=pow(max(color,0.0),1.0/2.2);
    return float4(saturate(color),1.0);
}
)";
}
