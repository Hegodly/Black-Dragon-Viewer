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
    float3 bodyPhysics; // breast, belly, butt displacement (meters)
};
Texture2D<float> shadowDepth : register(t0);
SamplerComparisonState shadowComparison : register(s0);
Texture2DArray<float4> materialTexture : register(t1);
// Four float4 rows per original KAMI joint. Explicit vector arithmetic
// avoids row/column-major ambiguities and works with DX11-era HLSL profiles.
StructuredBuffer<float4> skinRows : register(t2);
SamplerState materialSampler : register(s1);
StructuredBuffer<float4> materialRows : register(t3);
Texture2DArray<float4> colorTexture : register(t4);
TextureCube<float4> capturedRadiance : register(t5);
TextureCube<float4> filteredRadiance : register(t6);
TextureCube<float4> diffuseIrradiance : register(t7);
Texture2D<float4> environmentBRDF : register(t8);
StructuredBuffer<float4> environmentProbe : register(t9);
struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float2 material : MATERIAL;
    float2 uv : TEXCOORD0;
    float region : TEXCOORD1; // zero unless a local body physics marker
    uint bones : BLENDINDICES;
    uint weights : BLENDWEIGHT;
    float textureLayer : TEXCOORD2;
    uint pbrMaterial : TEXCOORD3;
};
struct Interpolant {
    float4 clip : SV_POSITION;
    float3 position : TEXCOORD0;
    float3 normal : TEXCOORD1;
    float3 albedo : TEXCOORD2;
    float2 material : TEXCOORD3;
    float2 uv : TEXCOORD4;
    nointerpolation float textureLayer : TEXCOORD5;
    nointerpolation uint pbrMaterial : TEXCOORD6;
};
// The same local avatar transformation is used in the depth/shadow and
// main passes; this keeps locomotion and shadows synchronized on the GPU.
// Four-weight linear blend skinning, including shadow pass, so fitted-mesh
// preview geometry visibly reacts to KAMI's independently simulated joints.
void skin_vertex(VertexIn input,out float3 position,out float3 normal) {
    position=input.position;
    normal=input.normal;
    if(input.weights==0U)return; // terrain / unrigged mesh
    const uint4 joint=uint4(input.bones&255U,
                            (input.bones>>8)&255U,
                            (input.bones>>16)&255U,
                            (input.bones>>24)&255U);
    const uint4 encoded=uint4(input.weights&255U,
                              (input.weights>>8)&255U,
                              (input.weights>>16)&255U,
                              (input.weights>>24)&255U);
    const float4 w=float4(encoded)/255.0;
    float3 p=float3(0.0,0.0,0.0);
    float3 n=float3(0.0,0.0,0.0);
    [unroll] for(uint i=0U;i<4U;++i) {
        if(w[i]>0.0) {
            const uint offset=min(joint[i],9U)*4U;
            const float4 xrow=skinRows[offset];
            const float4 yrow=skinRows[offset+1U];
            const float4 zrow=skinRows[offset+2U];
            const float4 trow=skinRows[offset+3U];
            const float3 transformed=
                input.position.x*xrow.xyz+
                input.position.y*yrow.xyz+
                input.position.z*zrow.xyz+trow.xyz;
            const float3 rotated=
                input.normal.x*xrow.xyz+
                input.normal.y*yrow.xyz+
                input.normal.z*zrow.xyz;
            p+=transformed*w[i];
            n+=rotated*w[i];
        }
    }
    position=p;
    normal=normalize(n);
}
float3 avatar_position(float3 local,float region) {
    // Deformation applies only to explicitly-tagged fitted-physics markers.
    // Neutral world/OBJ geometry has region=0 and remains unchanged.
    if(region>2.5) local.y+=bodyPhysics.z;
    else if(region>1.5) local.y+=bodyPhysics.y;
    else if(region>.5) local.y+=bodyPhysics.x;
    const float gait=region<.5?photoViewport.w:0.0;
    const float phase=kamiEffects.w;
    const float lateral=local.x<0.0?1.0:-1.0;
    // Coarse demonstration gait for KAMI's original six-box mannequin.
    // A live SL avatar will require skinned joint transforms and animation assets.
    if(abs(local.x)<0.46 && local.y<1.22) {
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
    float3 skinned,skinnedNormal;
    skin_vertex(input,skinned,skinnedNormal);
    const float3 world=avatar_position(skinned,input.region);
    output.clip = mul(float4(world, 1.0), viewProjection);
    output.position = world;
    output.normal = avatar_normal(skinnedNormal);
    output.albedo = input.color;
    output.material = input.material;
    output.uv = input.uv;
    output.textureLayer = input.textureLayer;
    output.pbrMaterial=input.pbrMaterial;
    return output;
}
struct ShadowInterpolant {
    float4 clip : SV_POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation float textureLayer : TEXCOORD1;
    nointerpolation uint pbrMaterial : TEXCOORD2;
};
float4 sample_material(float2 uv,float sourceLayer) {
    uint width,height,layers;
    materialTexture.GetDimensions(width,height,layers);
    float layer=clamp(round(sourceLayer),0.0,float(layers-1U));
    float4 texel=materialTexture.Sample(materialSampler,float3(uv,layer));
    // Layer zero is KAMI's procedural fallback and is always opaque.
    if(layer>0.5)clip(texel.a-0.0001);
    return texel;
}
ShadowInterpolant shadow_vertex_main(VertexIn input) {
    float3 skinned,skinnedNormal;
    skin_vertex(input,skinned,skinnedNormal);
    ShadowInterpolant output;
    output.clip=mul(float4(avatar_position(skinned,input.region),1.0),lightViewProjection);
    output.uv=input.uv;output.textureLayer=input.textureLayer;output.pbrMaterial=input.pbrMaterial;
    return output;
}
uint material_offset(uint index) {
    uint rows,stride;materialRows.GetDimensions(rows,stride);
    return index>0U&&index<rows/15U?index*15U:0U;
}
float2 pbr_uv(float2 uv,uint offset,uint slot) {
    float4 transform=materialRows[offset+5U+2U*slot];
)"
R"(    float2 shifted=uv*transform.xy;
    float co=cos(transform.z),si=sin(transform.z);
    return float2(co*shifted.x-si*shifted.y,si*shifted.x+co*shifted.y)+
        materialRows[offset+6U+2U*slot].xy;
}
float4 pbr_texel(float2 uv,uint offset,uint slot,float layer) {
    if(layer<0.5)return float4(1.0,1.0,1.0,1.0);
    float3 coord=float3(pbr_uv(uv,offset,slot),layer);
    if(slot==0U||slot==3U)return colorTexture.Sample(materialSampler,coord);
    return materialTexture.Sample(materialSampler,coord);
}
float3 srgb_linear(float3 value) {
    return float3(value.r<=0.04045?value.r/12.92:pow((value.r+0.055)/1.055,2.4),
                  value.g<=0.04045?value.g/12.92:pow((value.g+0.055)/1.055,2.4),
                  value.b<=0.04045?value.b/12.92:pow((value.b+0.055)/1.055,2.4));
}
float4 pbr_base(float2 uv,uint offset,bool front) {
    float4 flags=materialRows[offset+4U];
    if(!front&&flags.y<0.5)clip(-1.0);
    float4 sampled=pbr_texel(uv,offset,0U,materialRows[offset+3U].x);
    float4 base=materialRows[offset];
    base.rgb*=sampled.rgb;base.a*=sampled.a;
    if(flags.x>0.5&&flags.x<1.5)clip(base.a-materialRows[offset+2U].w);
    else if(flags.x>1.5)clip(base.a-0.0001);
    return base;
}
void shadow_pixel_main(ShadowInterpolant input,bool front:SV_IsFrontFace) {
    uint offset=material_offset(input.pbrMaterial);
    if(offset>0U){if(materialRows[offset+4U].x>=0.0)pbr_base(input.uv,offset,front);}
    else sample_material(input.uv,input.textureLayer);
}
float3 mapped_normal(float3 N,float3 position,float2 uv,uint offset) {
    float layer=materialRows[offset+3U].y;if(layer<0.5)return N;
    float2 coord=pbr_uv(uv,offset,1U);
    float3 tangentNormal=materialTexture.Sample(materialSampler,float3(coord,layer)).xyz*2.0-1.0;
    tangentNormal.xy*=materialRows[offset+1U].w;
    float3 px=ddx(position),py=ddy(position);float2 ux=ddx(coord),uy=ddy(coord);
    float det=ux.x*uy.y-ux.y*uy.x;
    if(abs(det)<0.00000001)return N;
    float3 T=(px*uy.y-py*ux.y)/det;
    T-=N*dot(N,T);float lengthT=length(T);
    if(lengthT<0.000001)return N;T/=lengthT;
    float3 rawB=(py*ux.x-px*uy.x)/det;
    float3 B=normalize(cross(N,T))*sign(dot(cross(N,T),rawB));
    return normalize(T*tangentNormal.x+B*tangentNormal.y+N*tangentNormal.z);
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
float3 probe_direction(float3 position,float3 ray) {
    float3 center=environmentProbe[0].xyz,extent=environmentProbe[1].xyz;
    float3 p=position-center;
    if(environmentProbe[2].x>0.5){
        float3 ax=environmentProbe[3].xyz,ay=environmentProbe[4].xyz,az=environmentProbe[5].xyz;
        float3 local=float3(dot(p,ax),dot(p,ay),dot(p,az));
        float3 direction=float3(dot(ray,ax),dot(ray,ay),dot(ray,az));
        direction=float3(abs(direction.x)<0.00001?0.00001:direction.x,
                         abs(direction.y)<0.00001?0.00001:direction.y,
                         abs(direction.z)<0.00001?0.00001:direction.z);
        float3 hit=(sign(direction)*extent-local)/direction;
        float distance=min(hit.x,min(hit.y,hit.z));
        if(distance>0.0)return normalize(p+distance*ray);
    }else{
        float radius=max(extent.x,max(extent.y,extent.z));
        float a=dot(p,ray),disc=a*a-dot(p,p)+radius*radius;
        if(disc>0.0){float t=-a+sqrt(disc);if(t>0.0)return normalize(p+t*ray);}
    }return ray;
}
float3 environment_light(float3 position,float3 N,float3 V,float3 base,
                         float metallic,float roughness,float occlusion){
    float nv=max(dot(N,V),0.001);float3 f0=lerp(float3(.04,.04,.04),base,metallic);
    float3 f=f0+(max(float3(1.0-roughness,1.0-roughness,1.0-roughness),f0)-f0)*pow(1.0-nv,5.0);
    float3 irradiance=diffuseIrradiance.SampleLevel(materialSampler,N,0.0).rgb;
    float3 direction=probe_direction(position,reflect(-V,N));
    float3 radiance=filteredRadiance.SampleLevel(materialSampler,direction,roughness*6.0).rgb;
    float2 brdf=environmentBRDF.SampleLevel(materialSampler,
        clamp(float2(nv,roughness),0.5/64.0,1.0-0.5/64.0),0.0).rg;
    float ambiance=max(environmentProbe[1].w,0.0);
    float3 diffuse=(1.0-f)*(1.0-metallic)*base*irradiance/3.14159265359*ambiance;
    float3 specular=radiance*(f0*brdf.x+brdf.y);
    return (diffuse+specular+base*.075*max(1.0-ambiance,0.0))*occlusion;
}
struct Surface {
    float3 normal,base,emissive;
    float metallic,roughness,occlusion,opacity,unlit;
};
Surface load_surface(Interpolant input,bool front,uint offset,bool terrain,float legacyLayer){
    Surface s;s.normal=normalize(input.normal);s.base=input.albedo;s.emissive=float3(0,0,0);
    s.metallic=saturate(input.material.x);s.roughness=clamp(input.material.y,.07,1.0);
    s.occlusion=1.0;s.opacity=1.0;s.unlit=0.0;
    if(offset>0U){
        float4 flags=materialRows[offset+4U];float4 factors=materialRows[offset+2U];float4 layers=materialRows[offset+3U];
        float4 base;
        if(terrain){base=materialRows[offset];base.rgb*=pbr_texel(input.uv,offset,0U,layers.x).rgb;}
        else base=pbr_base(input.uv,offset,front);
        s.base=base.rgb;s.unlit=flags.z>0.5?1.0:0.0;
        if(!terrain&&!front)s.normal=-s.normal;
        s.metallic=factors.x;s.roughness=factors.y;
        if(frame.z>0.5&&s.unlit<.5){
            float3 orm=pbr_texel(input.uv,offset,2U,layers.z).rgb;
            s.metallic*=orm.b;s.roughness*=orm.g;
            float ao=flags.w>.5?pbr_texel(input.uv,offset,4U,flags.w).r:orm.r;
            s.occlusion=lerp(1.0,ao,factors.z);
            s.normal=mapped_normal(s.normal,input.position,input.uv,offset);
        }
        s.roughness=clamp(s.roughness,.07,1.0);
        s.emissive=materialRows[offset+1U].rgb*pbr_texel(input.uv,offset,3U,layers.w).rgb;
        s.opacity=!terrain&&flags.x>1.5?base.a:1.0;
    }else if(terrain){
)"
R"(        s.base=legacyLayer>.5?colorTexture.Sample(materialSampler,float3(input.uv,legacyLayer)).rgb:input.albedo;
        s.metallic=0.0;s.roughness=.86;
    }else{
        float3 texel=sample_material(input.uv,input.textureLayer).rgb;
        s.base=input.textureLayer<.5?saturate(input.albedo*lerp(.72,1.12,texel)):saturate(input.albedo*texel);
    }return s;
}
float corner_blend(float4 corners,float2 uv){return lerp(lerp(corners.x,corners.z,uv.x),lerp(corners.y,corners.w,uv.x),uv.y);}
float terrain_noise(float2 p){
    float2 grid=floor(p),f=frac(p);f=f*f*(3.0-2.0*f);
    float4 hash=frac(sin(float4(dot(grid,float2(127.1,311.7)),dot(grid+float2(1,0),float2(127.1,311.7)),
        dot(grid+float2(0,1),float2(127.1,311.7)),dot(grid+float2(1,1),float2(127.1,311.7))))*43758.5453);
    return lerp(lerp(hash.x,hash.y,f.x),lerp(hash.z,hash.w,f.x),f.y)*2.0-1.0;
}
float4 pixel_main(Interpolant input,bool front:SV_IsFrontFace) : SV_TARGET {
    uint offset=material_offset(input.pbrMaterial);Surface surface;
    if(offset>0U&&materialRows[offset+4U].x<-.5){
        float3 absolute=input.position+materialRows[offset+4U].yzw;
        float2 position=saturate(absolute.xz/256.0);
        float start=corner_blend(materialRows[offset+2U],position);
        float range=max(corner_blend(materialRows[offset+3U],position),.01);
        float level=clamp((absolute.y+terrain_noise(absolute.xz/16.0)*6.5-start)*4.0/range,0.0,3.0);
        float4 weights=saturate(1.0-abs(float4(0,1,2,3)-level));
        surface.normal=float3(0,0,0);surface.base=float3(0,0,0);surface.emissive=float3(0,0,0);
        surface.metallic=0.0;surface.roughness=0.0;surface.occlusion=0.0;surface.opacity=1.0;surface.unlit=0.0;
        [unroll]for(uint i=0U;i<4U;++i){
            uint layer=material_offset((uint)round(materialRows[offset][i]));
            Surface s=load_surface(input,true,layer,true,materialRows[offset+1U][i]);
            surface.normal+=s.normal*weights[i];surface.base+=s.base*weights[i];surface.emissive+=s.emissive*weights[i];
            surface.metallic+=s.metallic*weights[i];surface.roughness+=s.roughness*weights[i];
            surface.occlusion+=s.occlusion*weights[i];surface.unlit+=s.unlit*weights[i];
        }
        surface.normal=normalize(surface.normal);
    }else surface=load_surface(input,front,offset,false,0.0);
    float3 N=surface.normal,base=surface.base,emissive=surface.emissive;
    float metallic=surface.metallic,roughness=surface.roughness,occlusion=surface.occlusion,opacity=surface.opacity;
    bool unlit=surface.unlit>.999;
    float3 L=normalize(-sunDirectionIntensity.xyz);
    float3 V=normalize(cameraWorldPosition.xyz-input.position);
    float3 H=normalize(L+V);
    float NdotL=saturate(dot(N,L));
    float NdotV=max(saturate(dot(N,V)),0.001);
    float NdotH=saturate(dot(N,H));
    float VdotH=saturate(dot(V,H));
    float visibility=shadow_factor(input.position,N);
    float3 radiance=float3(1.25,1.18,1.08)*sunDirectionIntensity.w;
    float3 color;
    if(unlit)color=base;
    else if(frame.z<0.5) {
        color=base*(0.16+0.84*NdotL*visibility)*sunDirectionIntensity.w;
    } else {
        float3 F0=lerp(float3(0.04,0.04,0.04),base,metallic);
        float3 F=fresnel_schlick(VdotH,F0);
        float a=(roughness+1.0);
        float k=(a*a)/8.0;
        float G=smith_visibility(NdotV,k)*smith_visibility(NdotL,k);
        float D=ggx_distribution(NdotH,roughness);
        float3 specular=D*G*F/max(4.0*NdotV*NdotL,0.001);
        float3 diffuse=(1.0-F)*(1.0-metallic)*base/PI;
        float3 ambient=base*.075*occlusion;
        if(offset>0U&&environmentProbe[0].w>0.5&&photoViewport.z>=-0.5)
            ambient=environment_light(input.position,N,V,base,metallic,roughness,occlusion);
        color=ambient+(diffuse+specular)*radiance*NdotL*visibility;
    }
    color+=emissive;
    // Original KAMI material-space contact shading. This uses existing per-pixel
    // world position/normal, not an incorrectly labelled screen-space AO pass.
    if(kamiEffects.y>0.5) {
        float lower=saturate(input.position.y*0.65);
        float contact=lerp(0.76,1.0,lower);
        color*=lerp(contact,1.0,0.12*saturate(N.y));
    }
    // GPU-native procedural specular microglints: no external overlay or injector.
    if(kamiEffects.z>0.5 && metallic>0.15) {
        float3 R=reflect(-L,N);
        float focused=pow(saturate(dot(R,V)),64.0);
        float sparkle=pow(saturate(sin(input.position.x*94.7+
                            input.position.y*117.3+input.position.z*51.8+
                            frame.x*1.4)*0.5+0.5),24.0);
        color+=float3(0.7,0.82,1.0)*focused*sparkle*
                    saturate(metallic)*0.26;
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
    // Probe captures store scene-linear HDR and exclude IBL feedback.
    if(photoViewport.z<-.5)return float4(max(color,0.0),opacity);
    // Basic filmic output mapping while maintaining SDR UNORM compatibility.
    color=color/(color+float3(1.0,1.0,1.0));
    // The sRGB render-target view encodes output and blends in linear space.
    return float4(saturate(color),opacity);
}

struct SkyVertexOutput {
    float4 clip : SV_POSITION;
    float2 uv : TEXCOORD0;
};
SkyVertexOutput sky_vertex_main(uint id : SV_VertexID) {
    const float2 p=id==0U?float2(-1.0,-1.0):
                   id==1U?float2(-1.0,3.0):float2(3.0,-1.0);
    SkyVertexOutput output;
    output.clip=float4(p,0.99999,1.0);
    output.uv=float2(p.x*0.5+0.5,0.5-p.y*0.5);
    return output;
}
// KAMI-generated atmosphere preview, not the simulator's EEP sky.
float4 sky_pixel_main(SkyVertexOutput input) : SV_TARGET {
    float3 right=float3(viewProjection[0][0],viewProjection[1][0],viewProjection[2][0]);
    float3 up=float3(viewProjection[0][1],viewProjection[1][1],viewProjection[2][1]);
    float3 forward=float3(viewProjection[0][3],viewProjection[1][3],viewProjection[2][3]);
    float2 ndc=float2(input.uv.x*2.0-1.0,1.0-input.uv.y*2.0);
    float3 ray=normalize(forward+right*ndc.x/max(dot(right,right),.00001)+up*ndc.y/max(dot(up,up),.00001));
    float rise=saturate(.5-.5*ray.y);
)"
R"(    float3 color=lerp(float3(.075,.21,.43),float3(.43,.70,.85),rise);
    float horizon=exp(-ray.y*ray.y*25.0);
    color=lerp(color,float3(.72,.79,.77),horizon*.23);
    float alignment=dot(ray,normalize(-sunDirectionIntensity.xyz));
    float distance=sqrt(max(2.0*(1.0-alignment),0.0));
    float disc=1.0-smoothstep(.012,.020,distance),corona=exp(-distance*22.0);
    color+=float3(1.0,.76,.42)*(.36*corona+.55*disc)*saturate(sunDirectionIntensity.w);
    if(photoViewport.z<-.5)return float4(max(color,0.0),1.0);
    color=color/(1.0+color);
    return float4(saturate(color),1.0);
}

float2 sequence_sample(uint i,uint count){return float2(float(i)/float(count),float(reversebits(i))*2.3283064365386963e-10);}
float3 tangent_direction(float3 local,float3 N){
    float3 up=abs(N.z)<.999?float3(0,0,1):float3(1,0,0);
    float3 T=normalize(cross(up,N)),B=cross(N,T);
    return normalize(T*local.x+B*local.y+N*local.z);
}
float3 ggx_sample(float2 xi,float3 N,float roughness){
    float a=roughness*roughness;
    float co=sqrt((1.0-xi.y)/max(1.0+(a*a-1.0)*xi.y,0.000001));
    float si=sqrt(max(1.0-co*co,0.0)),phi=6.28318530718*xi.x;
    return tangent_direction(float3(cos(phi)*si,sin(phi)*si,co),N);
}
float3 cube_direction(float2 uv,uint face){
    float2 p=uv*2.0-1.0;
    if(face==0U)return normalize(float3(1,-p.y,-p.x));
    if(face==1U)return normalize(float3(-1,-p.y,p.x));
    if(face==2U)return normalize(float3(p.x,1,p.y));
    if(face==3U)return normalize(float3(p.x,-1,-p.y));
    if(face==4U)return normalize(float3(p.x,-p.y,1));
    return normalize(float3(-p.x,-p.y,-1));
}
float4 environment_filter_pixel(SkyVertexOutput input):SV_TARGET {
    uint mode=(uint)round(photoGrade.x),face=(uint)round(photoGrade.z);
    if(mode==2U){
        float nv=clamp(input.uv.x,.001,.999),roughness=clamp(input.uv.y,.001,1.0);
        float3 N=float3(0,0,1),V=float3(sqrt(1.0-nv*nv),0,nv);float2 sum=float2(0,0);
        [loop]for(uint i=0U;i<128U;++i){
            float3 H=ggx_sample(sequence_sample(i,128U),N,roughness);
            float3 L=normalize(2.0*dot(V,H)*H-V);
            float nl=saturate(L.z),nh=saturate(H.z),vh=saturate(dot(V,H));
            if(nl>0.0){
                float k=roughness*roughness*.5;
                float g=(nv/(nv*(1.0-k)+k))*(nl/(nl*(1.0-k)+k));
                float visibility=g*vh/max(nh*nv,.00001),f=pow(1.0-vh,5.0);
                sum+=float2(1.0-f,f)*visibility;
            }
        }return float4(sum/128.0,0,1);
    }
    float3 N=cube_direction(input.uv,face),sum=float3(0,0,0);float total=0.0;
    float roughness=photoGrade.y;
    if(mode==0U&&roughness<.001)return capturedRadiance.SampleLevel(materialSampler,N,0.0);
    [loop]for(uint i=0U;i<64U;++i){
        float2 xi=sequence_sample(i,64U);float3 L;
        if(mode==1U){float r=sqrt(xi.y),phi=6.28318530718*xi.x;
            L=tangent_direction(float3(cos(phi)*r,sin(phi)*r,sqrt(1.0-xi.y)),N);}
        else {float3 H=ggx_sample(xi,N,roughness);L=normalize(2.0*dot(N,H)*H-N);}
        float weight=mode==1U?1.0:max(dot(N,L),0.0);
        sum+=capturedRadiance.SampleLevel(materialSampler,L,0.0).rgb*weight;total+=weight;
    }
    return float4(sum/max(total,.00001)*(mode==1U?3.14159265359:1.0),1.0);
}
)";
}

