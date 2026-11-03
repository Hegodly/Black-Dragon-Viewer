# KAMI Windows release preparation

The Windows pipeline integrates the native feature set, then runs
`prepare-release.py` before configuring or compiling the viewer. Historical
integration scripts remain source history/build inputs; their prototype
hooks, controls and settings are removed from the compiled release source.
No optional SDK upscaler/RTX runtimes are built or staged.

Retained release candidates:

- Fire Garden identity, portable packaging, login music chooser, Control Deck,
  Workspaces and World navigation.
- Five quality presets, true QHD and explicit 4K/8K supersampling, native AO,
  shadows, SSR/probes, tone mapping and existing renderer controls.
- Performance, Potato, Crowd, Smart tuning, avatar budgets, hardware monitor,
  Render Doctor, scene benchmarking and reversible graphics profiles.
- Avatar physics scheduling/diagnostics, own-avatar priority, Follow Avatar,
  WASD and arrow defaults, camera paths, atmosphere and local EEP restoration.
- Area Search, Smart Sort category/creator/store/brand modes, selected-folder
  sorting and Demo Deleter with previews and guarded inventory moves.
- Personal IM bubbles, optional compatible-client Sent/Delivered/Read statuses,
  foreground read acknowledgements, mute/DND/replay/duplicate protections.
- Native audio controls, recovery snapshots, and existing creator/upload tools
  preserving Second Life permissions, fees and limits.

Excluded from the release:

- DX12 foundation/scene preview execution and diagnostic entry points.
- DLSS/FSR/XeSS/RTX frame transfers and ray-AO modules.
- Optical-flow frame generation, extra present wrapper and unconditional flush.
- ALIVE profiles, near-field GI, experimental cinematic roughness remap and
  unverified true-PBR-off route.
- Incomplete RLV safety/engine UI, fixed-disabled widgets, unavailable FXAA
  choices, quarantined automatic resolution and experimental shader-cache UI.
- FMOD-only resampling controls in this OpenAL Windows configuration.

The main renderer remains OpenGL. This is not a completed DX12 migration.
Source checks do not certify native compilation, network interoperability,
Windows DPI/font rendering, live uploads, or FPS on physical hardware.

Graphics UI uses thirteen native collapsible groups, opaque reading surfaces,
495-pixel content bounds, retained setting IDs/callbacks and a compact header.
The final verifier checks menu preservation, absence of prototype hooks, all
8192 combinations of new group visibility, and retained stable feature hooks.
Packaging rejects known prototype DLLs, including nested/case-varied names.
Old saved FXAA selections migrate to SMAA before graphics listeners start;
retired glow, depth-of-field and motion-blur switches are reset at startup.

Performance changes remove prototype GPU/CPU transfer hooks, frame-generation
presentation work and experimental shader math rather than silently changing
manual frame caps, resolution or preset quality. Actual gains require matched
scene/camera/settings measurements on Windows.
