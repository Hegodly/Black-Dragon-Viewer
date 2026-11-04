# KAMI R43 — Full ObjectUpdate shape-layout correction and asset intake diagnostics

## Ground truth from the R42 Attalia test
- DX12 and shader caching initialized.
- EventQueue and the mesh capability were granted.
- Live simulator data included 8 retained root objects and 601 linked objects.
- R42 reported 598 **full-update extra-parameter records**, but FullMesh 0,
  MeshID 0, queue 0, downloads 0 and drawn meshes 0.
- ExtraParams record count is **not** the same as a valid mesh/sculpt ID.
  The next step must distinguish valid non-mesh params, malformed params
  and absent sculpt/mesh references.

## R43 source changes
1. Fix the **High 12 ObjectUpdate** profile/path classifier. The official
   message template uses ProfileCurve at index 1, PathScaleX at index 6
   and PathScaleY at index 7 in its 23-byte shape field segment.
   Packed High 13 ObjectUpdateCompressed uses a different layout.
   R42 incorrectly reused the compressed classifier for both.
2. Keep existing compressed ObjectUpdate legacy 86-byte particle skipping,
   authenticated CAPS, shader cache, DX12 rendering and asset scheduler.
3. Validate complete full ExtraParams envelopes before counting any records.
   Report FLight, FFlex, FOther, FSc (sculpt), FMesh and FBad (malformed)
   separately. Never log asset UUIDs, capability URLs, private messages
   or credentials.
4. Make F9 mesh pipeline diagnostics short enough to inspect: CAP, MeshID,
   queue, fetch, LOD, compressed sculpt, full sculpt, full mesh, type counts
   and malformed envelopes.
5. Add synthetic regression cases for actual High 12 shape offsets,
   incorrectly packed High 13 lookalikes, light/flexible/projector extras
   and malformed extra lengths.

## Test plan
- Build on a Windows x64 runner with Visual Studio 2022, then run CTest.
- Extract a new ZIP folder; launch KAMIViewer.exe and verify the F9 footer
  identifies R43 (do not mix old binary with new docs).
- Visit the same Attalia test location, enter World (F2), open F9.
- Report CAP, MeshID, Q, Fetch, LOD, CSc, FSc, FMesh, FLight, FFlex,
  FOther, FBad, root/linked and solid counts.
- If FSc=0 and FLight/FFlex dominate, there is **no verified mesh UUID** in
  parsed full updates; investigate object cache requests and packet coverage.
- If FSc>0 but MeshID=0, inspect type bits/zero UUID/extras validity
  and transform acceptance.
- If MeshID>0 but Q/Fetch remain 0, inspect visible-mesh scheduling/CAPS.
- If Fetch>0 but LOD remains 0, inspect mesh decoding; if LOD>0 but
  Mesh DRAW remains 0, inspect world geometry upload.

## Honest limits
This change improves the High 12 primitive classifier and diagnostics.
**It does not prove** buildings, sculpt/mesh assets, textures, materials,
outfits, avatars, inventory or IM will render/operate in-world.
An uploaded source branch is not a tested R43 executable until the Windows
workflow compiles, passes regression tests and produces a release artifact.

## Official protocol reference
- https://wiki.secondlife.com/wiki/ObjectUpdate
