# KAMI Original Core — Native feature restoration audit

## What actually executes today (Windows original-core offline scene)

- DirectX 12 scene renderer (no OpenGL world renderer).
- A 4th native graphics preset: Potato (8x8 terrain, 50% internal rendering, no PBR/shadow/contact haze/glints). Existing 16/32/64 terrain presets remain.
- Auto render scale with frame-based hysteresis and cooldown (50%/75%/100%) and a user-selectable FPS target. It uses application frame rate rather than GPU hardware timing and does not claim frame generation.
- Additional **original HLSL DX12 material shading**: optional distance haze, object/material-space contact darkening, and procedural metallic microglints. These are not SSR, hardware ray tracing, true screen-space AO or volumetric fog.
- Preview-avatar spring motion applied by the same DX12 scene/shadow shader, plus the prior KAMI camera.
- Read-only CPU/RAM/DXGI local-memory budget diagnostics in World. CPU percentage is for **the KAMI process**, not system total; GPU VRAM figure is the DXGI adapter usage, not dedicated KAMI allocation.
- Persistent controls under Settings -> DX12 FX (G), camera remains C; existing graphics controls remain.
- Source-only safe planning modules: inventory creator/store sorting, demo detection, mesh preflight, consent-gated navigation requests, correctly staged message receipt states, cryptographic package digest check with externally approved signature flag.

## What is NOT implemented or connected

- **Networking:** authentication, simulator UDP circuits, HTTP capability discovery, event queue, inventory/asset endpoints, message/teleport operations. No live Second Life account connection exists in this branch. Local planner modules cannot mutate the SL server.
- **GPU libraries:** real NVIDIA DLSS / AMD FSR / Intel XeSS, optical flow and frame generation need independently integrated, tested SDKs and compatible hardware. No placeholder toggles are exposed for them.
- **World graphics:** true SSR, screen-space ambient occlusion, volumetric ray-marched lighting, shadow quality controls, and live-world texture/material ingestion are not implemented.
- **Creation:** the creator preflight audits KAMI's own procedural triangle mesh only; it is not a Second Life uploader. SL limits and permission checks still apply.
- **Social:** read/delivered receipts are only legal to display when supported by an actual authenticated transport; a viewer must not imply read status without evidence. Viewer identification is opt-in, verified peer-provided metadata only, not fingerprinting other users.
- **Hotfixes:** current SHA-256 verification checks local payload bytes and refuses to stage without an external trusted signature result; it does not download, sign, install, or update executables.
- **UI:** original KAMI native dark/light style exists. iOS-style animations / glass and a full messaging UI are not yet implemented.

## Next KAMI networking boundary: independent, DX12-compatible

Build an original Windows networking library separate from the renderer:

1. HTTPS login client + response parser for currently supported Second Life login flow. Do not store or transmit credentials to any third-party server.
2. Parse the server's simulator endpoint, session credentials and seed capability; validate TLS and restrict redirects.
3. Winsock UDP simulator circuit including packet message encoding, circuit-code handshake, acknowledgements, retries, throttles and region handshake.
4. LLSD-based capability discovery, HTTPS requests and EventQueue long-poll updates.
5. Decode world/object/avatar appearance/animation/mesh/texture assets and build immutable scene updates for the DX12 rendering thread.
6. Only after authenticated protocol responses, plug inventory, messaging, navigation, friend presence and Creator Studio into the service adapters.
7. Test on local protocol fixtures, then the authorized Aditi beta grid, before the production Agni grid.

**No Black Dragon/Firestorm/source imports, no GShade, no MXAO.**
The source project is currently on a publicly visible GitHub repository; moving it private is a separate repository administration task and is not accomplished by these changes.
