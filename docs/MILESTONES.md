# KAMI Independent Development Gates

1. Original Windows application shell, native graphics UI, responsive light/dark theme and accessibility.
2. Independently implement Second Life authentication, TLS identity and session management.
3. Independently implement region and capability protocols, object updates and scene state.
4. Create original **DirectX 12-only** world renderer for geometry, materials, terrain, avatars and animations. Establish native D3D12 device/graphics queue first, then swap chain, resource heaps, shaders, PBR, and scene integration with measured frame times. Do not implement an OpenGL or D3D11 fallback. Direct2D/DirectWrite is allowed solely for the desktop UI.
5. Implement interoperable inventory and outfit synchronization with server permissions.
6. Implement real IM and social presence, opt-in negotiated receipts, and privacy controls.
7. Implement creator workflow and server-valid mesh uploads, previews and LOD.
8. Verify secure releases, signed updates, rollback and Windows accessibility/performance.

No feature may be represented as working until its subsystem passes end-to-end testing. The original project is not a reskin or code fork of a viewer. The old experimental branch is unrelated and remains unchanged during this migration.

## Native Graphics + FPS controls (initial batch)
The offline DX12 World screen uses per-user persisted Quality Preset, 50/75/100% actual render scale, a 30/60/120/144/unlimited frame pacer, VSync/tearing flags where supported, an FPS counter and tier-specific terrain and directional lighting. Pending: visual and framerate verification on real user hardware, proper GPU multiframe scheduling, real SL assets, PBR, ambient occlusion, shader effects, upscalers, frame generation, RT and adaptive quality.

## DX12 material/shadow graphics pass
- GGX metallic/roughness shading with a true off switch is implemented for procedurally generated test mesh material properties.
- Animated mannequin GPU vertex displacement, distinct from Second Life skeletal animation, is controllable.
- Separate DX12 1024x1024 depth-only directional shadow pass, PCF soft sampling, and a true on/off toggle are implemented for the test scene.
- Both render pipelines are added to the GPU-keyed first-launch shader compilation cache.
- Still missing: physical GPU rendering verification, real SL simulator/authentication, image textures, skinned avatar mesh/animations, realtime reflection and shadow cascades, HRAO, mesh asset codecs, DXR, independent upscaling and frame generation.
