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
