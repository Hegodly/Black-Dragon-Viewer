// KAMI native diamond glints. No screen-space sprite, no randomized twinkle.
// The response follows real normal-mapped PBR facets, light and view direction.
// Material eligibility is a conservative heuristic, NOT a gem identifier.
vec3 kamiDiamondGlints(vec3 normal, vec3 viewDir, vec3 lightDir,
                       vec3 baseColor, float roughness, float metallic,
                       float cameraDistance, vec3 directLight,
                       float shadowVisibility)
{
    if (kami_diamond_glint_strength <= 0.001 || cameraDistance > 24.0)
        return vec3(0.0);

    float neutral = 1.0 - smoothstep(0.12, 0.28,
        max(max(baseColor.r, baseColor.g), baseColor.b) -
        min(min(baseColor.r, baseColor.g), baseColor.b));
    float lightness = smoothstep(0.42, 0.74,
        dot(baseColor, vec3(0.2126, 0.7152, 0.0722)));
    float smoothDielectric = (1.0 - smoothstep(0.14, 0.32, roughness)) *
                             (1.0 - smoothstep(0.12, 0.33, metallic));
    float eligible = neutral * lightness * smoothDielectric;
    if (eligible <= 0.001)
        return vec3(0.0);

    vec3 n = normalize(normal);
    vec3 l = normalize(lightDir);
    vec3 v = normalize(viewDir);
    vec3 sumLV = l + v;
    float len2 = dot(sumLV, sumLV);
    if (len2 < 0.0001)
        return vec3(0.0);
    vec3 h = sumLV * inversesqrt(len2);
    float nDotL = max(dot(n, l), 0.0);
    float nDotV = max(dot(n, v), 0.0);
    if (nDotL <= 0.0 || nDotV <= 0.0)
        return vec3(0.0);

    // Very narrow extra specular lobe. Faceted geometry/normal maps produce
    // independent moving highlights; smooth surfaces won't gain fake stones.
    float nDotH = max(dot(n, h), 0.0);
    float exponent = mix(220.0, 680.0, 1.0 - clamp(roughness * 3.0, 0.0, 1.0));
    float flash = pow(nDotH, exponent);
    float distanceFade = 1.0 - smoothstep(7.0, 24.0, cameraDistance);
    float intensity = clamp(kami_diamond_glint_strength, 0.0, 2.0) *
                      eligible * nDotL * nDotV * flash * distanceFade *
                      clamp(shadowVisibility, 0.0, 1.0);

    vec3 fireTint = vec3(1.0);
    if (kami_diamond_glint_fire > 0.5)
    {
        // Small view-dependent chromatic shift, not physical ray-traced
        // diamond dispersion. No frame-time dependence or temporal noise.
        float angle = dot(n, vec3(0.61, 0.29, 0.74)) * 8.0 +
                      dot(h, vec3(0.23, 0.79, 0.57)) * 5.0;
        fireTint += 0.12 * vec3(sin(angle), sin(angle + 2.0944),
                                sin(angle + 4.1888));
    }
    return max(directLight, vec3(0.0)) * fireTint * intensity;
}
