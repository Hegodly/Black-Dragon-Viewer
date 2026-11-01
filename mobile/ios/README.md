# KAMI for iPhone — development preview 0.1

A separate native SwiftUI iPhone project, with an interactive local SceneKit garden and an in-memory message composer. Target: iOS 17+. No Apple account or paid membership is needed to compile and exercise the simulator build. CI also compiles an unsigned arm64 device application; that output cannot be installed on an iPhone until properly signed and provisioned.

## Current behavior

- World tab: local 3D scene, camera orbit, walk/turn controls, bounded movement, 30 FPS target.
- Messages tab: local-only composer, whitespace and UTF-8 byte-length validation, no delivery/read claims.
- Settings: clear connection status and local reset.
- No account credentials collected; no network services or analytics.

This is **not yet a functioning Second Life client**. The garden is procedural test geometry, not a downloaded region. Messages never leave the process. No Windows renderer, Firestorm client, or Second Life login code has been ported in this preview.

## Build

On a compatible Mac with Xcode and XcodeGen: run `xcodegen generate` in this directory, open `KAMI.xcodeproj`, and select an iPhone simulator. Scheme: KAMI. Tests cover composer validation and movement behavior. GitHub workflow `KAMI iPhone preview` builds/tests on a hosted Mac, launches the app, captures a screenshot, and compiles an unsigned device build. See CI results for verified build status; generated artifacts do not imply a signed installable app.

## Remaining work toward the requested full viewer

1. Implement and test Second Life login/session lifecycle, secure credential handling, UDP circuit, reliable packet acknowledgements, capabilities and event queue.
2. Connect real IM/local-chat transport, names and friendship data; handle reconnects and iOS suspension without claiming always-on background connectivity.
3. Fetch region terrain, object updates, meshes, textures, avatar skeleton/appearance and animation; replace local scene with streamed world rendering.
4. Synchronize walking/camera/object interactions with the simulator and enforce permissions.
5. Profile device memory, battery, heat, background transitions and network loss on a signed iPhone build.
6. Configure Apple signing and distribution only when device testing is ready.

The iOS workflow is separate from the Windows build. No signing credentials are stored in this project.
