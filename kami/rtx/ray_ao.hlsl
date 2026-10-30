// Copyright (C) 2026 Kombatant Viewer contributors
// SPDX-License-Identifier: MIT
RaytracingAccelerationStructure scene : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float4> sceneColor : register(t2);
RWTexture2D<float4> shaded : register(u0);
cbuffer Frame : register(b0)
{
    column_major float4x4 inverseViewProjection;
    uint width, height;
    float radius, strength;
    uint frameIndex;
    uint3 padding;
};
float3 positionAt(uint2 pixel)
{
    float2 uv = (float2(pixel) + 0.5) / float2(width, height);
    float4 p = mul(inverseViewProjection, float4(uv.x * 2 - 1, 1 - uv.y * 2, sceneDepth[pixel] * 2 - 1, 1));
    return p.xyz / p.w;
}
float random(inout uint state)
{
    state = state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return float((word >> 22u) ^ word) / 4294967296.0;
}
// A tile includes a one-pixel halo so filtering has no thread-group seams.
groupshared float tileAO[100];
groupshared float3 tilePosition[100];
groupshared float3 tileNormal[100];
groupshared float tileFootprint[100];
float occlusionAt(uint2 pixel, out float3 p, out float3 n, out float footprint)
{
    p = positionAt(pixel); n = 0; footprint = 0;
    float d = sceneDepth[pixel];
    if (d >= 0.999999 || pixel.x+1 >= width || pixel.y+1 >= height) return 0;
    float3 dx = positionAt(pixel + uint2(1,0)) - p;
    float3 dy = positionAt(pixel + uint2(0,1)) - p;
    n = cross(dx,dy);
    if (dot(n,n) < 1e-14 || length(dx) > radius || length(dy) > radius)
    { n = 0; return 0; }
    n = normalize(n);
    footprint = max(length(dx),length(dy));
    float2 uv = (float2(pixel)+0.5)/float2(width,height);
    float4 nearP = mul(inverseViewProjection,float4(uv.x*2-1,1-uv.y*2,-1,1));
    if (dot(n,nearP.xyz/nearP.w-p)<0) n=-n;
    float3 t=normalize(cross(abs(n.z)<0.9?float3(0,0,1):float3(0,1,0),n));
    float3 b=cross(n,t);
    // Stable per-pixel rotation: camera-stationary noise does not sparkle every frame.
    uint seed=pixel.x+pixel.y*width;
    float rotation=random(seed)*6.28318530718;
    float occluded=0;
    [unroll] for(uint sampleIndex=0;sampleIndex<16;sampleIndex++)
    {
        // Stratified cosine hemisphere, rather than four independent random rays.
        float u=(float(sampleIndex)+0.5)/16.0;
        float phi=rotation+float(sampleIndex)*2.39996323;
        float r=sqrt(u);
        RayDesc ray;
        ray.Origin=p+n*0.02;
        ray.Direction=t*(r*cos(phi))+b*(r*sin(phi))+n*sqrt(1-u);
        ray.TMin=0.01;ray.TMax=max(radius,0.05);
        RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE> query;
        query.TraceRayInline(scene,RAY_FLAG_NONE,0xff,ray);
        while(query.Proceed()) {}
        occluded+=query.CommittedStatus()==COMMITTED_TRIANGLE_HIT?1.0:0.0;
    }
    return occluded/16.0;
}
[numthreads(8,8,1)]
void main(uint3 group : SV_GroupID, uint3 local : SV_GroupThreadID)
{
    uint lane=local.y*8+local.x;
    // Every lane participates, including out-of-image lanes in partial tiles.
    for(uint i=lane;i<100;i+=64)
    {
        int2 tilePixel=int2(group.xy*8)+int2(i%10,i/10)-1;
        uint2 pixel=uint2(clamp(tilePixel,int2(0,0),int2(width-1,height-1)));
        float3 p,n;float footprint;
        tileAO[i]=occlusionAt(pixel,p,n,footprint);
        tilePosition[i]=p;tileNormal[i]=n;tileFootprint[i]=footprint;
    }
    GroupMemoryBarrierWithGroupSync();
    uint2 pixel=group.xy*8+local.xy;
    if(pixel.x>=width || pixel.y>=height)return;
    uint center=(local.y+1)*10+local.x+1;
    float4 color=sceneColor[pixel];
    if(tileFootprint[center]<=0){shaded[pixel]=color;return;}
    float total=tileAO[center],weights=1;
    [unroll] for(int y=-1;y<=1;y++)
    [unroll] for(int x=-1;x<=1;x++)
    {
        if(x==0 && y==0)continue;
        int i=int(center)+y*10+x;
        if(tileFootprint[i]<=0)continue;
        float normalWeight=pow(saturate(dot(tileNormal[center],tileNormal[i])),32);
        float planeDistance=abs(dot(tilePosition[i]-tilePosition[center],tileNormal[center]));
        float depthWeight=exp(-planeDistance/max(tileFootprint[center]*0.5,0.0001));
        float spatialWeight=(x==0 || y==0)?0.75:0.5;
        float w=normalWeight*depthWeight*spatialWeight;
        total+=tileAO[i]*w;weights+=w;
    }
    shaded[pixel]=float4(color.rgb*(1-strength*saturate(total/weights)),color.a);
}
