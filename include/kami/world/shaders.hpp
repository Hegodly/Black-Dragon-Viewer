#pragma once
// Original KAMI HLSL scene shader, shared by runtime and Windows validation.
namespace kami::world {
inline constexpr char kSceneShader[] = R"(
cbuffer Frame : register(b0) {
    row_major float4x4 viewProjection;
    float4 sunDirection;
};
struct VertexIn {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 color : COLOR;
};
struct Interpolant {
    float4 clip : SV_POSITION;
    float3 normal : NORMAL;
    float3 albedo : COLOR;
};
Interpolant vertex_main(VertexIn input) {
    Interpolant output;
    output.clip = mul(float4(input.position, 1.0), viewProjection);
    output.normal = input.normal;
    output.albedo = input.color;
    return output;
}
float4 pixel_main(Interpolant input) : SV_TARGET {
    float3 n = normalize(input.normal);
    float diffuse = saturate(dot(n, normalize(-sunDirection.xyz)));
    float3 lit = input.albedo * (0.24 + diffuse * 0.85);
    float3 ambient = float3(0.025, 0.035, 0.06);
    return float4(saturate(lit + ambient), 1.0);
}
)";
}
