# KAMI R14 — Seven Workstreams, One Windows Test Build

**One ZIP, one KAMIViewer.exe, one build/test workflow.**
This build combines existing cumulative R9–R13 code and the incremental
R14 improvements. It is NOT seven fully completed live Second Life systems,
and it is NOT yet a production replacement viewer.

## Integrated status of the seven workstreams

1. **Objects and buildings:** LIVE simulator full/compressed/cached/terse/kill
   packet classification works. Actual prim/mesh geometry decoding,
   assets and DX12 object rendering are NOT implemented.
2. **Terrain and textures:** Real SL land DCT patches are decoded and
   triangulated into the DX12 region scene, with neighboring-patch seam
   stitching and bounded vertex uploads. R14 adds slope/height-tinted
   diagnostic shading to make land shape clearer. Actual Second Life
   terrain materials, texture assets and complete streamed LODs are
   NOT implemented.
3. **Avatar rendering:** Local KAMI rig, body physics and prototype
   mannequin renderer are packaged. Real SL avatar skeletons, mesh
   assets, outfits, appearance and animations are NOT connected.
4. **Movement and interaction:** Authenticated neutral AgentUpdate camera
   interest-list heartbeats work. Local WASD/camera preview exists.
   Live simulator-authoritative avatar movement, interactions, sitting,
   teleports and camera synchronization are NOT complete.
5. **Messaging, friends and inventory:** Original offline KAMI messenger,
   inventory Smart Sort previews and friends/inventory foundations are
   in the compiled application. Live Second Life services, messages,
   friend presence, remote inventory changes and receipts are NOT
   connected.
6. **Stability, security and performance:** Bounded wire parsing,
   HTTPS connection diagnostics, shader caching, adaptive quality and
   Windows CI tests are included. MFA, broader real-world testing,
   robust reconnection and security hardening are NOT complete.
7. **Final interface:** Original native Direct2D/DirectWrite UI, light/dark
   theme, DX12 settings, Creator Studio local OBJ preview and Photo
   Studio PNG capture are available. Finished UX, updater,
   accessibility review and full graphics integrations remain pending.

## Verification

The Windows build compiles the independent KAMI app, tests C++ modules
and packages *one* portable ZIP. A successful CI run means the code
compiled and automated tests passed. It does not prove every live SL
feature works.

Extract the complete ZIP; run `KAMI/KAMIViewer.exe`.
Test with care: the login path remains experimental and should not be
assumed production-secure. The HOME page reports region connection
state, while WORLD shows the verified untextured terrain preview.

Build branch: release/kami-r14-seven-in-one-20261103
Repository: Hegodly/Black-Dragon-Viewer

## R15: experimental simulator-authoritative control packets

The WORLD / AVATAR W/S, A/D, Z/C, Shift and Space keys are now mapped to
documented, restricted Second Life AgentUpdate control bits, rather
than only animating the original offline mannequin. Inputs are sent
only after authenticated region movement confirmation and while
the window is active. A 300ms watchdog returns to neutral if the
UI stops refreshing control state. Untrusted click, fly, edit and teleport
bits are rejected. The local DX12 camera and mannequin are not yet
synchronized with real server-authoritative avatar position.

Local loopback tests check control wire bytes and the release back to
zero. No claim is made that all simulator-side movement has been proven
against the live grid. R9-R14 terrain, login, original KAMI UI and
the seven staged workstreams stay in this same ZIP.

## R16: visible avatar placeholder + native camera tracking (experimental)

- R16 appends a generic blue, six-box *local visual placeholder* to the
  already working authenticated SL land mesh in the DX12 vertex buffer.
- The marker is anchored to the verified initial AgentMovementComplete
  simulator position (in SL x/y/z mapped to DX12 x/z/y).
- The same keyboard controls used by R15's real simulator AgentUpdate are
  used to predict movement locally. The DX12 camera follows that estimate,
  and releasing the keys stops its estimated movement. Motion is bounded
  to the current 256m region, never teleporting a user or altering assets.
- The WORLD footer explicitly says "AVATAR LOCAL ESTIMATE (not SL-synced)".
- Important: NO continuous server-authoritative avatar position decoder
  exists yet; this local placeholder may drift relative to what is seen
  in Firestorm or another viewer. The 216-vertex cuboid is NOT your real
  SL avatar, outfit, or animations. Real SL mesh and texture decoding
  and own-avatar ObjectUpdate movement correction remain future work.
- Regression coverage validates authenticated spawn, invalid spawn,
  predicted movement, key release, region bounding and marker geometry.
- R9-R15 login, CAPS, terrain, stitching and live movement are all
  included in this SAME Windows portable test ZIP.

Build branch: release/kami-r16-live-avatar-placeholder-20261104

## R17: visible A/D turning in the live avatar placeholder

R16's six-box marker appeared almost identical from front and back,
making yaw hard to distinguish. R17 adds an asymmetric gold chest badge,
white face visor and violet back plate. The live WORLD camera stays
steady while A/D is held to reveal avatar rotation, then (if camera
auto-recenter is enabled) eases BEHIND the marker after key release.

WORLD shows the local heading in degrees so turning is observable
independently of the avatar geometry. Camera and geometry tests verify
the front/back appearance, 180-degree turning, and camera recenter.

Existing R15 server-directed movement packet flags and verified R9–R16
terrain/login flows are preserved. The visible marker is a LOCAL
ESTIMATE, not yet the real SL skinned avatar nor authoritative server-
replicated orientation.

## R18: All seven development workstreams in ONE test executable

Press F9 (or click 7 SYSTEMS TEST in the sidebar) in the Windows
application for a single read-only panel containing seven bounded,
live/local evidence checks:

1. **Objects and buildings:** counts authenticated simulator object wire
   packets (full/compressed/cached/terse/kill). These are NOT yet decoded
   into visible meshes or buildings.
2. **Terrain and textures:** checks whether real authenticated SL terrain
   is currently uploaded into DirectX 12. Actual SL textures, PBR material
   assets and objects are still missing.
3. **Real avatar appearance:** checks for the locally predicted blue
   placeholder. Real wearable/mesh/head/body/skin assets are not loaded.
4. **Movement and interaction:** counts nonzero, authenticated movement
   AgentUpdate packets actually transmitted. The user's separate-viewer
   test established real server movement earlier, but the panel alone
   does not prove current live position sync or teleports.
5. **Messaging and inventory:** explicitly says local previews only;
   SL chat/IM/friends/inventory network integration remains undone.
6. **Security and performance:** reports whether DX12 initialized.
   Security audit, MFA, reconnect/recovery and representative FPS
   benchmarks are unfinished. The graphics settings remain testable.
7. **Final UI and graphics:** local KAMI theme, camera settings, creator
   OBJ inspection, photo PNG capture and graphics previews remain
   available. No production updater or full asset parity yet.

An automated diagnostic regression test ensures missing features are
never reported as complete. The existing R9–R17 connection, terrain,
avatar placeholder, real movement controls and camera turning fixes
are inherited in the same app and ZIP.

### Recommended single-session live test
Login to Attalia. Open WORLD to confirm land vertex counts and visible
turning; use W/A/S/D, Shift and Space, observing the real avatar from
another SL viewer. Open F9 and check all seven statuses. Open SETTINGS,
AVATAR, INVENTORY, MESSAGES, CREATOR STUDIO and PHOTO STUDIO to
smoke-test local UI tools; none implies unimplemented live services.
Report the seven displayed status strings and any crashes.

## R19: first in-world object geometry from REAL simulator transforms

R19 retains R9–R18's login, live terrain, visible blue avatar,
server-directed movement, camera and F9 dashboard. It adds a bounded
parser for ObjectUpdateCompressed (High 13), using the official
LLViewerObject packed field layout: UUID, local ID, PCode, state, CRC,
material, click action, XYZ scale, XYZ position, XYZ rotation vector,
special flags, owner UUID and optional angular velocity/parent ID.

- Only root (not linked-child), PCode 9 volume transforms with finite,
  reasonable simulator-local position and scale are accepted. Invalid
  packets never overwrite known-good transforms.
- ObjectKill removes cached root transforms; at most 512 roots retained.
- The nearest at most 64 roots within 96m horizontally and 60m vertically
  of the authenticated login position are rendered as narrow AMBER
  spatial bounding frames, not made-up solid buildings.
- The frames share the same DirectX 12 scene buffer/lighting pass as
  the decoded untextured SL terrain, while the local blue avatar retains
  its separate per-frame GPU transform.
- Both land and object revisions coalesce to a single upload no more
  often than every 800ms, protecting GUI responsiveness.
- The WORLD footer now says LIVE AMBER OBJECT BOUNDS [count]. F9's
  object row reports PARTIAL only after at least one root bounding
  frame has actually been uploaded to the renderer.
- New automated regression tests cover packed offsets, parent
  rejection, non-volume rejection, missing-length rejection,
  NaN rejection, KillObject framing and budgeted DX12 proxy geometry.

IMPORTANT LIMITATIONS:
- ObjectUpdate (High 12), cached objects, linked-child transforms,
  sculpt, mesh geometry and actual SL materials/textures are NOT
  decoded/rendered. Some regions send no root compressed messages,
  so a legitimate R19 run can display zero amber bounds.
- The decoded XYZ rotation is NOT yet applied to the amber frames.
  These are axis-aligned transform-size outlines, not final SL shapes.
- Own-avatar continuous live position correction is STILL missing:
  the blue avatar moves from a local estimate anchored to initial
  verified simulator spawn. This build is a meaningful in-world
  packet/render step, NOT full SL visual parity.

LIVE TEST:
Login to Attalia; verify prior walking/turning and terrain. Press F9
and check object row. In WORLD, look for amber bounding outlines
over terrain and read LIVE AMBER OBJECT BOUNDS count. A count of zero
means no supported root-volume compressed transform reached the
renderer; it does not prove absence of in-world objects. Compare
screenshots with Firestorm and report missing/corrupt bounds.

## R20: first major native DX12 in-world graphics pass

In WORLD, R20 keeps prior verified SL land heights, R19 compressed
root-volume bounding outlines, and R17 walking/turning placeholder.
It adds original KAMI-authored visual shading on those live coordinates.

- New DX12 SKY pass: one fullscreen triangle, gradient blue/cyan
  atmosphere, horizon haze, and a projected sun glow calculated from
  the same viewProjection/sun vector as the live terrain. Sky depth is
  disabled, so land/objects occlude it. It is not SL's true environment.
- Grass/sand/stone/soil color blending based on **real SL region-local
  absolute land elevation**, verified simulator water height, actual
  decoded normals/slope, and stable spatial variation. These colors
  are procedural and DO NOT fetch SL's land textures.
- A region-sized six-vertex water plane is positioned at the actual
  authenticated RegionHandshake water level and uses native DX12
  roughness/metallicity. It is not Second Life's water material.
- New shader-cache version prevents stale PSO/bytecode reuse from R19.
  The sky pass and existing scene pass remain on DirectX 12; no OpenGL.
- Terrain, region water, and amber object frames share one bounded
  region geometry upload, with the visible blue local avatar in its
  separate GPU-transformed section. GPU water geometry is only 6 verts.
- WORLD footer now explicitly indicates a WATER SURFACE or WATER PENDING;
  F9 remains the unified seven-system status page.
- Windows native regression tests compile sky vertex/pixel shaders and
  existing scene/shadow shaders with D3DCompiler and validate palette,
  stable colors, water bounds, finite vertex payloads.

IMPORTANT:
  KAMI still has a locally estimated blue avatar and only root
  spatial proxies for compressed objects, NOT real mesh/object/terrain
  textures, animated skinned avatar assets, or a complete viewer.
  The sky is authored by KAMI and not synced to a simulator EEP setting.
  Scene lighting and shadows are supported but not final quality.

Test plan: sign into Attalia, select WORLD, verify blue/cyan sky,
colored grass/slope/shore terrain, water plane and any R19 amber bounds.
Verify WASD and A/D, watch FPS and look for shader startup errors.
If land colors appear but bounds count remains zero, R19 object
packet coverage remains unfinished, not a R20 shading failure.

## R21: Fix blue-only WORLD despite decoded land

A real user screen from R20 displayed 256 accepted decoded land patches
and ~97,344 terrain vertices at 200 FPS, but the viewport showed
little beyond a blue gradient and the R17 avatar placeholder.

Two grounded fixes:
1. R20 mistakenly drew a huge opaque 256m by 256m water rectangle at
   region water height even over land above the waterline. R21 clips
   narrow DX12 water triangles ONLY to actually decoded, submerged
   SL terrain triangles. It interpolates the actual land-sample edges
   to find shoreline intersections. No height samples are invented.
   Limits water vertices (96k by default) to protect CPU/GPU.
   The WORLD footer now reports whether any decoded wet land exists.
2. Press V on WORLD to toggle a true *Landscape Camera* that looks
   down toward the ground elevation at the nearest authenticated
   land patch. This is a camera-only preview; W/A/S/D still transmit
   the same movement controls and update the blue placeholder.
   Press V again to resume avatar-follow mode. Suppress Win32 key
   auto-repeat so one press flips only once.

R21 preserves the native DX12 sky shader, procedural terrain biome
shading, R19 root-volume object bounds, R17 moving/turning avatar
preview, and R9–R18 SL connection/diagnostics.

New automated tests cover dry terrain -> zero water triangles, mixed
shoreline clipping, completely submerged terrain, finite water vertex
data, vertex budgets, and the camera's raised scenic viewpoint.

LIMITATIONS: This is an experimental SL-connected DX12 prototype.
Avatar position remains a local prediction, amber object bounds only
cover some update packet types, and actual SL appearance/mesh/PBR
texture streaming is not yet integrated. Compiling the test doesn't
confirm what the user sees in Attalia; live verification is required.

LIVE TEST: Log into Attalia and open WORLD. Press V to show LANDSCAPE
CAMERA. The cyan sky should give way to visible green/stone shoreline
and hills where decoded terrain exists. Press V again for the avatar
camera and verify A/D turning, W/S movement and FPS remain intact.

## R22: fix real Second Life terrain DCT coefficient byte order

The R21 field screenshot showed giant near-vertical land spikes despite
256 decoded patches, ~97k land vertices and ~200 indicated FPS.

ROOT CAUSE FOUND IN CODE: a single integer read of `wordbits` DCT
coefficient magnitude incorrectly interpreted 9-17 serial bits as a
big-endian integer. Official `LLBitPack::bitUnpack(U8*, U32)` splits
the bitstream into up-to-8-bit chunks; those chunks are written into
little-endian successive bytes of the U32 destination, with the bits
inside each byte still MSB-first. Multi-byte coefficients were decoded
with the wrong numerical magnitude. This corrupts elevations, especially
when higher-frequency coefficients produce jagged geometry.

- `Bits::unpack_ll_uint` implements the exact per-byte chunk order for
  arbitrary supported coefficient wordbits; patch IDs share it.
- Explicit 11-, 12- and 17-bit DCT **nonzero** DC regression packets,
  each at 16x16 and 32x32, verify computed actual constant elevations.
  R21's all-zero test patches could never catch this bug.
- A last-resort DX12 VISUAL safeguard omits 1-meter cells spanning over
  48 meters vertically rather than creating hundreds of meters of
  triangles. It does not alter or invent authentic SL elevation data and
  may hide some genuine extraordinary cliffs.
- The live water mask skips the SAME discontinuous 1-meter cells, so
  rejected spikes cannot leave suspended water wedges.
- WORLD displays SKIPPED SPIKES [count]. A nonzero value means the
  geometry safeguard omitted some 1m cells; it does not establish they
  were invalid according to SL's server.
- New synthetic spike regression tests verify four affected cells are
  omitted for both land and masked water, smooth surrounding geometry
  remains untouched, and successful full DX12 upload still preserves
  R17 movement and R21 landscape camera.
- All R9-R21 auth, sky, procedural region-coloring, water, object bounds,
  avatar local prediction, DX12-only shading, and user-confirmed movement
  controls are retained in ONE portable Windows ZIP.

IMPORTANT: This repairs a decoder defect demonstrably capable of false
terrain spikes, but no claim of an accurate Attalia image until tested
live by the user. Real SL terrain material asset downloads, mesh
buildings, and skinned avatar appearance are still not integrated;
200 FPS was observed before full scene content and is not a guarantee.

R22 TEST: Log into Attalia; WORLD -> press V for landscape overview;
compare high vertical gray walls with the earlier photo; read SKIPPED
SPIKES and AMBER BOUNDS; verify W/S + A/D turning and camera switch
survive. Screenshot after applying R22 is the decisive confirmation.

## R23: decode full Second Life ObjectUpdate to enrich live WORLD

The user confirmed R22's height decoder fix: 256 live SL land patches,
97,344 vertices, no giant gray spike walls and SKIPPED SPIKES=0, while
WORLD still reported AMBER BOUNDS=0. That means KAMI has not yet
drawn a supported SL root object.

R23 adds defensive decoding for the *full*, high-frequency 12
ObjectUpdate message. R19 previously handled only High 13 compressed
objects. This parses all fixed and length-prefixed fields in the
official Second Life message template, then extracts finite root
volume (PCode=9, ParentID=0) world position and XYZ scale from 60,
76, 124 and 140 byte full-precision ObjectData.

- Malformed, truncated, out-of-range, parent-relative and non-volume
  blocks cannot enter the same capped live root transform cache.
- Existing DX12 geometry builder draws the nearest 64 supported
  server-derived roots as narrow AMBER bounding frames.
- F9's object diagnostic row shows received Full, Compressed and
  Cached wire counts, decoded full frame counts, and rejected
  spatial frames, distinguishing missing network data from parsing.
- New C++ tests cover multi-block framing, byte offsets, parented,
  non-volume, malformed, out-of-range and wrong-message cases.
- Retains R22's corrected genuine SL terrain height decoder,
  weather-independent KAMI DX12 sky, painted terrain, masked water,
  landscape camera and user-confirmed avatar walking/turning.

This still DOES NOT load SL object meshes, sculpt shapes, textures,
PBR or avatar wearables. Cached object updates still require requesting
missing object data, and compressed objects remain subset-only.
If AMBER BOUNDS remains zero, the F9 counters tell us which
protocol/asset stage must be addressed next. An executable passing
unit tests is NOT proof of full Second Life scene rendering.

LIVE TEST: Log into Attalia; in WORLD note amber count and terrain
appearance, press F9 and capture the Objects row's FULL/COMPRESSED/
CACHED received and decoded/rejected counts; press V and use WASD
to check preserved camera and movement.

## R24: diagnose why 165 full object frames produce 0 object bounds

R23 live F9 screenshot from Attalia:
Full RX 165; Compressed RX 0; Cached RX 1;
Full decoded 165; Rejected 0; WORLD Amber Bounds 0.

A successful full *frame* parse is NOT a decoded root object.
R24 adds per-block counters that explain precisely where accepted full
frames stop producing visible world geometry. F9 object row now shows:
- Full RX / Frame OK / Cached RX / retained Roots / rendered Amber.
- Volume (PCode=9), Other (non-volume), Linked (ParentID!=0),
  Motion skip (unsupported position encoding), Invalid (bad numeric data)
  and Accepted (root transforms extracted).

All counters aggregate from fully validated packets only; no account,
object UUID or raw packet payload is displayed/stored.
Tests prove that a valid frame containing ONLY linked, non-volume,
invalid or unsupported objects is no longer confused with actual
rendered geometry. The original live DX12 rendering, R22 terrain
decoder, R21 V landscape camera and movement remain unchanged.

If Accepted>0 but Roots=0, examine cache churn, bounds or kill handling.
If Roots>0 but Amber=0, inspect radius (96 m X/Y, 60 m Z from initial
spawn), upload revision and DX12 geometry.
If Volume>0 but Accepted=0, inspect Linked/Motion skip/Invalid.
If Volume=0, all decoded blocks are non-volume (often avatars/trees)
and no root-volume position can yet be drawn.

R24 remains a diagnostic and safety patch, NOT full mesh or texture
rendering. Do not treat the F9 stage statuses as viewer completeness.
