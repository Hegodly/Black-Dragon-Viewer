# KAMI R38 — CAPS Seed Visibility and SL Mesh Introspection

This is an experimental DX12-only Second Life viewer test build, not a
feature-complete or guaranteed compatible viewer.

## Verified source-level changes
- The F2 Simulator Handshake screen now keeps the original CAPS seed result
  on a separate line while the EventQueueGet long poll continues.
- The seed diagnosis distinguishes an unoffered mesh capability, an invalid
  mesh URL rejected by the strict Linden-owned HTTPS allowlist, and a mesh
  capability present but missing its URL. Never log or display bearer URLs.
- F9 now distinguishes compressed geometry shapes, validated extra parameter
  records, sculpt records, and unsupported optional fields in addition to the
  current MeshID/CAP/Fetch/LOD status.
- Second Life compressed object updates can now safely pass through the
  4-byte ScratchPadSize plus 4-byte embedded binary length and bounded
  ScratchPad payload, matching official viewer data-packer behavior.
- Malformed or oversized scratchpads remain rejected; no blind offset scan
  and no synthetic mesh IDs. Regression tests include successful and
  mismatched-length scratchpad cases.
- R36's validated terrain/primitive geometry, DX12 renderer, and FPS-first
  scheduling remain unchanged.

## Why this build
R37 in-world evidence from Attalia showed 10 real primitive shapes,
256 terrain patches, active EventQueueGet, and 0 downloaded mesh assets.
The original seed result was overwritten by the repeated polling message.
This build distinguishes denied/absent CAPS from missing parsed mesh IDs,
and tests the ScratchPad path that was previously skipped.

## How to test
1. Extract the R38 Windows ZIP as a new folder and run KAMIViewer.exe.
2. Log in using its experimental SL login interface and open F2.
3. Screenshot the separate EventQueue line and **Seed:** line in the
   Simulator Handshake section. Do not share signed URLs or credentials.
4. Enter the native DX12 world viewport (F2), then open F9. Screenshot
   **MeshID, CAP, Fetch, LOD, Shapes, Extra, Sculpt, Skip**.
5. Compare whether the actual platform/mesh surfaces appear.
6. Report any crash or device loss. The successful CI build does not
   establish real-world compatibility or frame-rate performance.

## Still not implemented
General SL mesh/texture pipeline completeness, fitted and Bento avatar
skinning, animated attachments, full inventory, group/IM event handling,
and comprehensive simulator parity. EventQueue event bodies remain
observed but are not applied. Unreal Engine is not part of R38.
