# KAMI Beyond Reality — Original Core

KAMI is a new Windows application with an independently authored DX12 renderer and UI. It does not include Black Dragon or Firestorm renderer source or themes. A **separate LGPL-2.1 Windows DLL** now adapts the official Second Life viewer\'s volume serial login-identity algorithm, with source, attribution and license in `src/sl_reference/` and `third_party/secondlife-lgpl/`. The official viewer\'s entire authentication and simulator networking implementations have not been ported.

## Current status
KAMI is a Windows-only independent prototype with a working **offline DX12
world preview**, avatar locomotion, eight-axis procedural avatar physics,
local inventory/outfit planners, native messaging layout, graphics controls
and creator/photo tools. **It is not a working Second Life viewer yet.**
Actual login, simulator networking, remote inventory, avatar assets, messaging
and teleport execution are not implemented.

The HOME screen includes a user-triggered **Test Secure SL Route [N]**
diagnostic that performs a real credential-free HTTPS HEAD to the official
SL login service. The result only verifies TLS/HTTP reachability; it never
claims the user is authenticated or connected. The new simulator handshake,
capability references, service requests, inventory/world snapshots, and
avatar appearance manifests are original validated integration contracts
that need actual verified LLUDP/capability transport and data decoders.
See [live integration status](docs/KAMI_LIVE_SECOND_LIFE_CORE_STATUS.md).

## Second Life login HTTP error diagnosis

**HOME → L → login** displays the actual numeric status returned by the
official Agni HTTPS login endpoint. Earlier builds collapsed all
non-200 responses into `SL LOGIN HTTP ERROR`. Examples:

- **HTTP 301/302/307/308**: the login endpoint requested a redirect.
  KAMI deliberately refuses automatic credential-bearing redirects.
- **HTTP 400**: the login server rejected the request format.
- **HTTP 403**: the server refused the login request. This is not proof
  the account password is wrong, and can involve edge/WAF policy.
- **HTTP 404/405**: endpoint missing or POST method disallowed.
- **HTTP 429**: rate limited; do not retry repeatedly.
- **HTTP 500/502/503/504**: service may be temporarily unavailable.
- **HTTP 200 / unsupported XML-RPC**: HTTPS succeeded but KAMI's XML-RPC
  response parser still needs compatibility changes.

Only status codes are displayed. KAMI does **not** show or log server
response bodies, redirect URLs, passwords, session keys, or signed
capability URLs. Do not paste credentials into bug reports or chat.

The endpoint remains `https://login.agni.lindenlab.com/cgi-bin/login.cgi`,
a documented third-party login target. If login fails, check
[Second Life grid status](https://status.secondlifegrid.net/) and
compare against a supported viewer. Use a dedicated test account for
KAMI until its login and MFA/security implementation is reviewed.

## Experimental Second Life login + simulator UDP milestone

In **HOME (F1)**, **N** still runs the password-free official HTTPS
reachability diagnostic. **L** (or **SL LOGIN TEST**) now opens an explicit
Terms acceptance warning followed by the **native Windows credential prompt**
for the official main-grid login host. Password input is transient and is
not saved. A real official HTTPS login request runs asynchronously.
KAMI reports **LOGIN ACCEPTED** only after a validated login reply.

After acceptance, an original WinSock UDP circuit sends UseCircuitCode,
CompleteAgentMovement, and responds to simulator ping, acknowledgments
and region handshakes. **Simulator responses, login acceptance and a
fully usable in-world session are separate states.** This does not yet
establish a full live SL session: capability/event queue, MFA handling,
live terrain/mesh/texture assets, avatar skeletons, inventory, chat,
teleporting and the remote DX12 world renderer remain to be built.
The DX12 WORLD tab currently remains an offline test scene.

See [experimental sign-in and UDP transport notes]
(docs/KAMI_EXPERIMENTAL_SL_SIGNIN_AND_UDP.md). Use a test account when
reviewing this experimental build; avoid submitting real passwords in
issues or bug reports. Compliance and privacy reviews remain necessary
before distributing a completed viewer.

## World rendering policy
KAMI's 3D rendering backend is **DirectX 12 exclusively**. The new original `src/world/device.cpp` uses DXGI to select a hardware D3D12 device and creates a direct command queue. An optional DX12 WARP path is provided for diagnostics where hardware DX12 is unavailable. This is only a foundation: it does not yet create a swap chain, draw geometry, or render a Second Life scene. No OpenGL or D3D11 world renderer is included.

The existing **Direct2D/DirectWrite** rendering is strictly for the two-dimensional Windows app interface and does not replace the required DirectX 12 world renderer.

## Build
Requires Windows, Visual Studio 2022 Desktop development with C++, Windows SDK, and CMake.
Run these commands in the project root:
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release
    ctest --test-dir build -C Release --output-on-failure
Then start build/Release/KAMIViewer.exe.

F1-F6 changes screens; T toggles light/dark and saves the preference in the Windows user registry.

This clean source tree lives on a separate branch in the owner's existing GitHub repository. The old repository name and earlier branches remain until a new repository is created or the existing one renamed. No inherited viewer files are included in this branch's source tree. The user has not yet selected a license for their newly authored code.


## Original DX12 scene milestone
The World tab now hosts a **DirectX 12 rendering viewport**. It renders a locally generated terrain mesh and a primitive anonymous mannequin using vertex-colored materials, directional HLSL lighting, depth testing, and a movable camera. Use F2 to open World; A/D or arrow keys orbit; W/S or wheel zoom; Up/Down adjusts height. This is offline demonstration data, not an actual Second Life region or avatar. Native Windows 2D UI remains Direct2D; no OpenGL world rendering.

## Hardware-based shader optimization at startup
KAMI now starts shader preparation automatically on the HOME/initial screen (no F2 required). It detects the DirectX 12 hardware adapter, compiles its currently implemented HLSL shaders once using optimization level 3, creates an actual D3D12 graphics pipeline state, and writes cached shader bytecode and GPU-specific pipeline state under **%LOCALAPPDATA%\\KAMI\\ShaderCache**. Future launches reuse cached blobs and validate the pipeline against the current driver; changing the GPU, driver, shader code or build-cache version causes a safe rebuild. A progress bar on HOME shows real compilation stages and cache status. This is NOT a login blocker and no fake percent animation runs. On unsupported hardware it reports unavailable.

This mimics Call of Duty's high-level initial shader optimization *workflow*, not proprietary Call of Duty code or its full shader library. Only the independent KAMI scene's implemented shaders can currently be optimized. The cache can reduce shader compilation stutter and loading overhead but is not a guarantee of higher average FPS. The app still lacks real Second Life account login and world networking.

## Graphics + FPS — first native control batch

Open **SETTINGS (F6)**. The new Graphics + FPS pane is authored from KAMI C++ and includes four **functional** DX12 scene controls: Quality Preset (Performance / Balanced / Quality), Render Scale (50 / 75 / 100 percent of the World viewport pixels), Frame Rate Limit (30 / 60 / 120 / 144 / unlimited), and VSync (on/off). Click each row or press **1–4** to cycle. Values persist per Windows user under `HKCU\\Software\\KAMI\\OriginalCore\\Graphics` and are applied when the World tab is opened, or live while it is running.

- **Performance:** 16x16 terrain triangles and lighter directional lighting.
- **Balanced:** 32x32 terrain divisions and mid-strength lighting.
- **Quality:** 64x64 terrain divisions and stronger directional lighting.
- **Render Scale:** changes the actual DXGI swap-chain and depth-buffer pixel dimensions; DXGI stretches the finished scene to the viewport. It is not AI upscaling.
- **FPS cap:** a frame pacing loop targets the selected maximum. VSync uses the DXGI presentation interval, with tearing only where hardware/Windows reports support. The real-time World FPS display updates once per second.

These are **only** settings for KAMI's original *offline test scene*. DLSS, XeSS, FSR, real-time ray tracing, SSAO, full PBR asset ingestion, frame generation and a live Second Life world remain unimplemented and have no pretend toggles. This first batch is not yet benchmarked against any existing viewer; average FPS can be bounded by CPU/GPU and OS scheduling.

## DX12 PBR, GPU animation, and depth-based directional shadows

The independent KAMI scene now uses a GGX metallic/roughness shader path, per-vertex material parameters, and a separately toggleable simplified diffuse path. The preview mannequin has its own optional GPU-driven breathing motion (it is not a real Second Life avatar). A second D3D12 depth-only render pass writes a 1024x1024 shadow texture, with a 3x3 percentage-closer filter sampled by the physically based material shader. **Shadows OFF** genuinely skips the shadow pass. These effects are available as settings 5, 6 and 7. The startup GPU shader optimization compiles and caches both the main and shadow DX12 pipelines.

This adds real DirectX 12 material and shadow machinery, **not** SL asset ingestion, network login, server terrain, skeletal avatar playback, physically based glTF texture import, hardware ray tracing, frame generation, or DLSS/FSR/XeSS. Those remain unimplemented. Compilation tests cannot confirm correct imagery on physical GPUs; use the offline F2 scene for visual validation. Real-world viewing requires independent authentication, transport, and asset streaming.

## First DX12 texture-upload path
KAMI creates an original procedural 64x64 RGBA material image, allocates a true D3D12 GPU texture, stages data through an upload buffer using proper row-pitch alignment, transitions the texture into pixel-shader-readable state, and binds it as a shader-resource view alongside the depth shadow map. The original HLSL material shader performs sampled, wrapped linear-filtered texturing in the World scene.

This path is **not a Second Life asset decoder**. Real SL textures (including their encodings, LOD, permissions and HTTP capabilities), skeletal avatar animation, hardware DXR ray tracing, DLSS/FSR/XeSS, frame generation, and actual SL connection must be implemented independently before Kami is a usable alternative viewer. The current build is an offline graphics prototype, even though the original 3D renderer itself is DX12-only.

## Renderer runtime diagnostic executable
The Windows build now includes KAMI-DX12-Smoke.exe, a separate console program written specifically for KAMI. It opens a temporary Windows test viewport, initializes the real DX12 swap chain and material/shadow/texture pipelines, renders a frame, changes PBR and shadow settings, changes geometry and render scale, resizes, and renders again. Run it on your own Windows PC to see the actual GPU adapter and a PASS/FAILED result. On headless CI hosts, the test explicitly reports SKIPPED if a display-capable DXGI swap chain is unavailable; compilation alone is not treated as proof that rendering works. This does not establish Second Life compatibility.


## Original KAMI Camera Engine (feature branch)
A KAMI-authored camera controller is now wired to the original DX12 offline scene; it does not depend on Black Dragon or Firestorm camera source. Defaults: **75-degree vertical FOV** (internal fixed value, no UI slider), **50% mouse sensitivity**, **15% smoothing**, **4-meter follow**, **terrain collision ON**, **auto-recenter ON**, **head-target tracking ON**, and **shake OFF**. Camera options are saved under the KAMI Windows user settings, independent of graphics settings.

In **Settings (F6)**, choose **Camera [C]** and click one of the seven settings rows or press keys 1–7. Switch back to Graphics with C. In **World (F2)**, left-drag to orbit; use the mouse wheel for dolly zoom, **Q** or right click for center/right/left shoulder compositions, **O** for cinematic idle orbit, **P** for precision pointer movement, **F** to lock/unlock the tracked target, and **R** to reset the view. Arrow keys/A/D orbit and W/S zoom the offline preview, preserving the existing demo keys.

The custom camera has frame-rate-independent smoothing, proximity sampling against KAMI's procedural terrain, an automatic return toward the subject's facing direction after inactivity, a head-target anchor that follows the procedural mannequin's existing idle motion, and an opt-in shake toggle that is off by default. Its math is portable and unit-tested separately from Win32/DirectX. It deliberately does not pretend to implement collision against streamed Second Life objects, camera avatar bone tracking, avatar movement, or remote region camera control. Those require the future independently implemented SL world and animation pipelines.


## Native KAMI Creator Studio + Photo Studio
**Access:** F7 opens Creator Studio. F8 opens Photo Studio. Both share the real
KAMI original DirectX 12 viewport and independent camera controls.

**Creator Studio (F7):** Select **IMPORT OBJ** or press **I**; Windows opens a
native file picker. KAMI parses an OBJ triangle/quad/polygon mesh from local
disk, checks indices, finite vertex coordinates, degenerate triangles and
resource limits, normalizes it for the local DX12 preview, then uploads actual
preview triangles to the GPU. The studio displays the audited triangle,
source-position and polygon counts alongside a clearly labelled example
uncompressed mesh/texture memory estimate. Choose **RESTORE SCENE** or press
**R** to return to original terrain/avatar preview. OBJ materials, rigs, texture
maps, LODs and live mesh uploading are not implemented. KAMI does not bypass
the Second Life simulator's upload charges, constraints or permission rules.

**Photo Studio (F8):** KAMI-authored DX12 HLSL grades the actual preview with
four styles (Cinematic, Natural, Monochrome and Night Studio), independent
controls for exposure, contrast, saturation and vignette, and an option to
disable the live grade. Click rows or press keys **1-6**. Photo settings persist
per Windows user. **CAPTURE PNG** opens Windows Save As, reads back the
current DX12 scene through a correctly aligned GPU readback resource, and
encodes its pixels via the Windows Imaging Component. It captures the current
DX12 render resolution; it does not secretly render additional 4K/8K pixels
or include the Direct2D application chrome. No external injectors, MXAO or
GShade are bundled.

**Original viewer feature audit:** See
`docs/SECOND_LIFE_FEATURE_AUDIT.md`. The code includes independent modules
for locally staged outfit references, landmark drafts, the geometry/texture
budget advisor and feature readiness reporting. Connected login, inventory,
world, avatar, messaging and simulator mutations remain unavailable until
the independently built SL protocol and asset streaming layers exist.

These studios are real native code paths for an **offline DX12 development
prototype**, not evidence that the viewer is ready for Second Life.


## Official Second Life login identity component

KAMI now includes a **separately built and replaceable DLL**
(`KAMI_SL_Login_Identity.dll`) implementing the same Windows `id0`
device-identity hashing algorithm as the official Second Life viewer:
MD5 of the four raw bytes of the Windows C: volume serial number.
This replaces KAMI's prior, incompatible **36-character all-zero UUID**
placeholder. The resulting pseudonymous 32-hex `id0` is sent only to
the official login endpoint over TLS; no raw serial is transmitted.

The DLL is a legally separate LGPL 2.1 component and is included
with its source and full license. See
`third_party/secondlife-lgpl/NOTICE.md`. KAMI's DX12 renderer stays
independent; the rest of Linden Lab's login / LLSD / LLCore networking
modules are not included or claimed to be working.

A successful compilation/test does not prove Second Life login succeeds.
Use a test account and report the HTTP status/response category only.

### Official Windows machine-ID login compatibility

The LGPL Windows login bridge now also follows Linden Lab's current
WMI machine fingerprint preference: motherboard product UUID,
operating-system serial fallback, then (if unavailable) adapter bytes.
It folds the WMI serial to six bytes as `LLMachineID` does and hashes
that result before sending the pseudonymous identifier to Second Life.
See the bundled LGPL license and `third_party/secondlife-lgpl/NOTICE.md`.
This is not a claim of complete live viewer networking or login.

## R6 region capability handshake (experimental)

After successful authentication and simulator movement confirmation, KAMI
requests `EventQueueGet` from the live region's signed seed capability,
then probes the returned event queue exactly once. It validates LLSD XML
and reports whether the capability was granted, idle, or returned a
structural/HTTP error. This test does **not** implement sustained
event polling, dispatch simulator events, enable inventory or load
actual region assets. UI status names distinguish that limitation.
The DX12 renderer remains independently implemented.

### R7: early real-world networking, not live terrain rendering

The R7 experimental branch collects sanitized, verified **region name
and water-level metadata** from the connected simulator's
`RegionHandshake` packet. It also maintains a session-bound
`EventQueueGet` request across empty long-polls, with typed
acknowledgement IDs and a hard stop before acknowledging any
unimplemented event. KAMI's DX12 preview remains an *offline demo*
until terrain/object/asset decoders produce authenticated geometry.
Do not interpret a valid EventQueueGet response as a live-world render.

## R8: live simulator event types and bounded world-wire intake

R7 established successful Second Life simulator movement and a valid
EventQueueGet response. A separate-viewer observation also showed the
logged-in agent present in-world. **That verifies server presence, not
KAMI's ability to render its appearance or the region.**

R8 extends the authenticated simulator connection with:

- Sanitized `EventQueueGet` message type names (maximum eight names of
  48 printable identifier characters). The raw LLSD event bodies are
  never displayed or logged. Unknown/unhandled events are deliberately
  **not acknowledged**, as R8 has no safe state-mutating dispatcher.
- Bounded message-template framing for `LayerData`,
  `ObjectUpdate` (header only), `ObjectUpdateCompressed`,
  `ObjectUpdateCached`, `ImprovedTerseObjectUpdate` and
  `KillObject`, using the official public Linden Lab message template.
- Actual UDP packet counts for LayerData and object-related messages
  displayed on KAMI's Home screen. Counters reset on disconnect and
  never count the offline scene.
- Tests for valid/truncated or oversized payloads, typed LLSD event
  names, and the simulator handshake regression.

**Important:** a valid packet frame is not a decoded terrain patch,
world object, material, mesh or avatar. ObjectUpdate High 12 is only
header-recognized. The GPU scene remains the original offline DX12
preview; these protocol counters must not be represented as rendered
Second Life content.

Source for the message IDs: Linden Lab's published
`master-message-template/message_template.msg` (High 11-16).
