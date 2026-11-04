# KAMI R42 — Correct Full ObjectUpdate Shape Fields & Mesh Inspection

## Why this build
Real-world Attalia R41 after DX12 world initialization showed 8 root
objects, 601 linked objects, ten supported primitive shapes, CAP YES,
but MeshID 0 / Fetch 0 / LOD 0. CAPS authorization is working.

Comparing official Second Life message_template.msg against the KAMI decoder
found a definite field-offset bug: Full High 12 ObjectUpdate sends
PathCurve[0], ProfileCurve[1], and PathScaleX/PathScaleY[6,7], while
Compressed High 13 LLDataPacker sends ProfileCurve[16] and PathScale[5,6].
The same classifier was incorrectly used for both. R42 fixes High 12,
retaining the already-working compressed classifier.

## Source changes
- Dedicated strict full-update shape classifier for official High 12
  packet field order; no manufactured topology for unknown profiles.
- New bounded ExtraParams inspection shared by the mesh UUID parser.
  Full-update sculpt counts are tracked independently from compressed
  updates, so 601 linked objects are not misconstrued as sculpt-free
  solely because compressed ExtraParams say zero.
- FullDecodeAudit now reports valid extras, total extra parameter
  records, sculpt records, malformed extras and simple shape candidates.
- F9 labels show **Extra C/F** and **Sculpt C/F** as separate values.
- R42 F9 footer identifies this exact build, rather than stale R40.
- Corrected synthetic High 12 test fixtures (previous tests had used
  compressed offsets), plus regressions for malformed extra data,
  normal light parameters and bad legacy shape layouts.

## How to test
1. Extract the ZIP to a NEW folder. Launch KAMI/KAMIViewer.exe.
2. Log into Attalia, open F2 world viewport until DX12 terrain appears.
3. Open F9. Confirm the footer says R42.
4. Send screenshots showing Root, Linked, SOLID, MeshID, CAP,
   Fetch/LOD, Extra C/F, Sculpt C/F and BadExtra.
5. If CAP YES but MeshID 0 and Sculpt C/F 0/0, this in-world
   sample has not delivered any usable sculpt/mesh UUID to KAMI.
   It does not prove all SL mesh objects are absent from the region.

## Scope and limits
R42 is an experimental native DX12 viewer and does not render full SL
meshes, textures or a proper skinned avatar yet. CI is a synthetic
protocol/regression check, not in-world acceptance testing. FPS and GPU
performance improvements are not being removed or replaced. No UE5.
