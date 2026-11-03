# KAMI body physics foundation — local DX12 preview

## What works

- Independent C++20, bounded spring/damper simulation of three physics
  channels: **chest**, **belly**, and **butt**.
- Per-region parameters: max deformation, spring stiffness, gain, damping,
  mass, gravity bias, and drag; invalid inputs are sanitized.
- Original local profiles: **OFF / SUBTLE / NATURAL / LIVELY** (B cycles
  them). These test values **are not** decoded Second Life wearable values.
- Locomotion drives each region independently using gait phase, speed changes,
  jump velocity, and landing impulse. Fixed 8ms substeps reduce FPS-dependent
  spring variations and clamp the effect to a safe range.
- Five low-poly, distinctly colored **physics-region markers** on KAMI's
  original procedural mannequin: left and right chest markers, belly marker,
  and left and right butt markers. They visibly deform in real DX12 shaders
  during movement.
- Physics regions are an extra vertex attribute. The same deformation
  offsets apply in **both shadow and color passes**, preventing the physics
  from appearing only in lighting or only in the shadow.
- Avatar tab (F3): move with W/S, turn A/D, strafe Z/C, run Shift, jump
  Space, change physics profile with **B**, reset locomotion and physics
  with **H**. The UI displays local live offset telemetry in millimeters.
- The OFF profile really zeros the displacement and skips the spring step.
- The additional vertex region defaults to zero for OBJ imports and test
  terrain, so the existing Creator Studio does **not** accidentally animate
  unrelated mesh.
- Tested with C++ spring tests, mesh integrity tests, HLSL shader compilation,
  and a real DX12 smoke test including animation on/off.

## Second Life compatibility and important limitations

Second Life Enhanced Avatar Physics is controlled by a *worn physics item*.
The viewer reads wearable parameters and applies visual deformation. In
fitted-mesh characters, motion depends on compatible rigging/weights to
physics collision-volume bones such as LEFT_PEC, RIGHT_PEC, BELLY and BUTT.
Wearable slider names include Max Effect, Spring, Gain, Damping, Mass,
Gravity and Drag.

Official references:
- https://community.secondlife.com/knowledgebase/english/customizing-a-classic-style-avatar-r1377/
- https://wiki.secondlife.com/wiki/Viewerhelp%3A2.6.3/My_Appearance_-_Edit_Wearable
- https://deliveries.avalab.org/docs/avastar/4/reference/usermanual/advanced/fitted-physics/

The local KAMI preview is **NOT yet real Second Life avatar physics**.
There is no SL login, wearable asset parsing, Bento skeleton, rigged-mesh
weight evaluation, bone transforms, attachment handling, cross-viewer
replication, or live appearance syncing. We do not claim that every SL mesh
body will visibly respond to a physics wearable. The live renderer needs
to decode actual SL avatar assets and apply wearable deformation on
compatible collision-volume bones after the network pipeline is connected.

Physics slider functionality alone is **not sufficient**; visibility
requires both evaluated motion and an actual rendered deforming surface.
KAMI's marker preview explicitly tests both sides before live integration.
