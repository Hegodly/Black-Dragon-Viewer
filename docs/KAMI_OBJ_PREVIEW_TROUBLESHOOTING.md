# Creator Studio — OBJ local preview reliability

The button **Import OBJ [I]** currently loads **Wavefront OBJ triangle geometry
into the original offline DX12 model preview**. It does **not** upload any
object to Second Life, spend L$, or access a simulator.

## What works

- OBJ vertex positions, polygon faces, quads and fan-triangulated polygons
- 1-based and negative OBJ vertex indices; face syntax with /UV/normal suffix
- Unicode Windows paths, OBJ mesh centering and normalized preview scale
- A limited preview of the mesh's actual polygons; no server connection
- Bounds checks on input lines, vertex counts, and GPU preview triangles
- Omission of isolated degenerate (zero-area) triangles, with a count in the UI
- DX12 geometry buffers replaced *transactionally*: failed allocations keep
  the last working scene buffer intact instead of releasing it first
- Specific user-facing errors when the file is unreadable, too dense,
  degenerate, malformed, or cannot be allocated by DirectX 12

## Current limits

- Maximum **333,333 triangles** after triangulation, 999,999 GPU vertices
- Maximum **400,000 OBJ source positions**
- Maximum **128 MiB source file size** checked before parsing
- Maximum **128 corners per face**, 16,384 characters per line and
  1,500,000 lines per OBJ

The **same triangle limit is shared** between the parser and the renderer.
Previously the parser accepted up to 500,000 triangles but the renderer
refused meshes over approximately 333,333 triangles, incorrectly reporting
that DX12 could not allocate preview geometry.

## Blender export guidance

If a large mesh is rejected, duplicate the model for a local preview, apply
transforms, triangulate concave faces, inspect normals, and reduce mesh
polygon count for the preview only. You can retain the high-poly original
in Blender.

OBJ texture coordinates (`vt`), normals (`vn`), MTL materials, texture
images, joint weights, custom normals, PBR materials, sculpt maps, and
avatar skeletons are not yet imported into the original renderer.
Every face currently gets a procedural default material and flat normal.
This is a local inspection tool, not a Second Life mesh upload workflow.

If an imported object is missing or clipped, press R to restore the scene,
then try the new build. Preview camera framing is kept centered on the
normalized mesh regardless of head tracking.

## If the problem persists

Send a screenshot that actually includes the **Creator Studio error/status**
line and, if you can, the OBJ source (or a reduced version). With a sample,
we can reproduce the actual failure rather than guessing from a UI photo.

The code is independently authored in `src/features/creator_studio.cpp`
and `src/world/renderer.cpp`; automated tests cover malformed geometry,
mixed valid/degenerate Blender-style faces and preview limits.
