# KAMI R43.1 — Pretest source audit and mesh LOD fallback fix

This is an incremental revision of the R43 experimental native DX12 viewer.
It retains R43 High 12 shape handling, object/extra diagnostics and the
existing FPS-first shader/scene pipeline.

## Verified pretest issue
The binary LLSD mesh loader in R43 chose medium_lod whenever its map
existed, even when the map's size was 0 (SL's valid "missing" medium LOD).
The result was a failed entire mesh decode instead of using the high_lod.
The official Second Life Mesh Asset Format specifies high_lod required and
medium_lod optional, and its Download Weight documentation substitutes
high LOD for missing medium LOD.

## Patch
- When medium_lod is absent or has a validated numeric size of exactly 0,
  choose the required high_lod block.
- Nonempty malformed medium_lod blocks remain errors; no unsafe fallback.
- Regression test synthesizes a binary LLSD asset with medium_lod size 0,
  high_lod zlib bytes intact, then also tests malformed nonempty medium
  and damaged compressed data.
- F9 footer says R43.1 to make old/extracted builds distinguishable.

## Before live testing
1. Extract the R43.1 artifact into a NEW directory, not over R43.
2. Run KAMI/KAMIViewer.exe and confirm F9 footer says R43.1.
3. Log into Attalia and enter F2 world view, then F9.
4. Send CAP, MeshID, Q, Fetch, LOD, FSc, FMesh, FLight, FFlex,
   FOther and FBad counters plus visible object/terrain screenshot.

## Current limits (not fixed by this patch)
- Live mesh discovery and asset retrieval are not confirmed in-world.
- If MeshID remains zero, LOD fallback cannot itself fix object visibility.
- Live texture/material download, skinned avatar and inventory/IM remain
  incomplete.
- Up to 32 mesh assets are retained per session, with bounded draw budget.
- Scene rebuilds may be visible as stutters; FPS benefit is not guaranteed.
- A green Windows CI test is not proof of complete in-world correctness.

Sources:
https://wiki.secondlife.com/wiki/Mesh/Mesh_Asset_Format
https://wiki.secondlife.com/wiki/Mesh/Download_Weight
