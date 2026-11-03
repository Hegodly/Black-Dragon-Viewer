# KAMI Avatar Fundamentals — Independent DX12 Offline Foundation

## Purpose
Original C++20 locomotion controller and GPU transformation for the existing
six-box procedural DX12 avatar. This is the first **local** avatar-system
milestone, not a claim that Second Life resident appearance has been loaded.

## Implemented
- Continuous **W/S** or **Up/Down** forward and backward movement.
- **A/D** or **Left/Right** turns the avatar (rather than orbiting the camera).
- **Z/C** strafes, **Shift** runs, **Space** jumps.
- Movement is normalized diagonally to avoid an unintended speed boost.
- Smooth acceleration/deceleration; local heading; bounded test-region
  coordinates; heightfield grounding; explicit ascending/falling states.
- 8 ms physics substeps and frame-delta clamping for frame pacing resilience.
- Jump edge latch prevents holding Space from auto-jumping repeatedly.
- KAMI-authored procedural walk/run gait in DX12 vertex and shadow shaders,
  using common avatar transform root constants so the shadow moves with the
  avatar.
- Camera subject follows the moving avatar without uploading new geometry
  every frame, and stays compatible with drag orbit, collision checks, zoom.
- **F3 Avatar** opens a real DX12 preview and local telemetry: movement state,
  speed, world position, ground/air status, heading. Reset with **H** or the
  reset button. **F2 World** also accepts walking/running/jumping.
- **Avatar motion** graphics toggle controls gait animation, not locomotion.

## Architecture
- `avatar::Controller` is deterministic local motion that accepts a height
  callback. It doesn't access Windows input or Direct3D, and is regression
  tested separately.
- `world::Renderer` owns the offline avatar state and passes x/y/z/heading,
  gait and phase as root constants to both shadow and lit passes.
- The root signature uses 60 32-bit constants and one SRV descriptor-table
  root parameter, within DX12's 64 DWORD root signature limit.
- `app::Window` samples controls only when the KAMI window is foregrounded.
  No input is sent to any Second Life server.

## Not implemented
- Real SL avatar ID, appearance, mesh/texture asset downloads, rigging,
  skeleton/skinned mesh, bones, attachment transforms or outfit syncing.
- Remote avatar interpolation, server-authoritative movement or animation
  overrides, real-world object collisions/physics, pathfinding or flight.
- Region login, avatar replication, teleport or parcel constraints.

These require the independent Second Life login/network and asset subsystems.
This feature **must not** be described as live Second Life avatar loading.
