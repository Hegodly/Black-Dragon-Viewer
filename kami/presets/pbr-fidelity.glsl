// KAMI graphics-preset PBR fidelity. Low and Medium simplify the glossy
// appearance through roughness floor and cheaper reflection contribution.
// Higher tiers retain authored gloss and more IBL. No postprocess, no
// GShade interference, and no modification to creators' PBR textures.
float kamiQualityRoughnessFloor(float tier)
{
    if (tier < 0.5) return 0.34;
    if (tier < 1.5) return 0.22;
    if (tier < 2.5) return 0.13;
    if (tier < 3.5) return 0.07;
    return 0.02;
}

float kamiQualityProbeWeight(float tier)
{
    if (tier < 0.5) return 0.25;
    if (tier < 1.5) return 0.55;
    if (tier < 2.5) return 0.85;
    if (tier < 3.5) return 1.0;
    return 1.10;
}
