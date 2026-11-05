# KAMI R59 PBR environment, terrain and streaming

Adds HDR scene cubemap capture, GGX roughness prefiltering, diffuse irradiance and split-sum BRDF lookup to the live glTF material path. Captures one face per frame and refreshes one nearest server probe or automatic camera probe; rotated box and sphere parallax correction. Terrain handshake detail IDs resolve to legacy textures or glTF material assets, with four height blended layers and map transforms.

Removes preview bottlenecks: mesh cache 2048 assets / 2 million triangles; draw budgets 24k/80k/180k/500k by preset; GPU images 512, CPU previews 1024, fixed 256 MiB pixel and upload budgets. The scene vertex safety ceiling is 2 million. These remain bounded budgets, not a promise that every asset in a region is resident.

Limitations: native KAMI sky rather than SL EEP; one active probe, no multi-probe blending or realtime planar mirrors. Transparent geometry is excluded from captures. Real avatar loading, teleport, inventory and other incomplete viewer systems are unrelated to PBR. Live simulator visual validation is still required.
