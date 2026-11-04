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
