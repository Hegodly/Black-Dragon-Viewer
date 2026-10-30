# Kami Creator Studio / 創造

## Purpose

Make Kami a reliable *creator-first* Windows Second Life viewer, without attempting
to evade simulator-side mesh requirements or L$ upload fees. UI styling should
follow Kami's existing Liquid Glass theme, not bundle an unrelated skin.

## Opening Creator Studio in a Kami build

1. Sign in to Second Life on a mesh-upload-enabled grid.
2. From the top menu choose **File > Upload > KAMI Creator Studio (Mesh)...**.
3. Load a supported COLLADA `.dae` model and use **Mesh Audit** before
   asking the simulator to calculate fees.

The menu item is **not** permanently disabled. Its enable callback keeps it
inactive while the model preview is loading; the original mesh availability
check can hide it on an unsupported grid. The server still decides upload
permission, land impact and L$ price. Older Kami builds containing the
hard-disabled **Model...** entry need a newly compiled viewer.

## First increment (this branch)

- Rename the existing mesh upload floater as "KAMI / CREATOR STUDIO - MESH".
- Add a **Mesh Audit** action next to the Calculate / Cancel / Reset controls.
- Report loaded per-LOD vertex, triangle, material-face and piece counts.
- Give actionable labels (LOD, model, material face) for up to 20 suspicious
  faces so a creator can fix a Blender export.
- Treat malformed index buffers, excessive per-face vertex counts, invalid
  source geometry, too many material faces and built-in model validation
  failures as blocking preflight errors.
- Flag the *legacy* 21,844-triangle-per-face threshold as a warning rather than
  a universal server rule.
- Refuse client-side fee calculation / paid upload if preflight fails.
- Continue to use **the existing viewer uploader, SL upload authorization,
  server-calculated cost, land-impact calculation and upload URL**. Kami does
  not decide server acceptance or fees.

The audit only describes meshes already loaded successfully by the existing
importer. It is not yet a multimillion-poly file loader.

## Creator Studio roadmap (not part of this commit)

1. **Ultra Scene Intake** — background parsing with bounded memory, cancellation,
   file size estimates, progress, overflow protection and safe handling of huge
   DAE / glTF / GLB sources. Import scene assets, not single mesh primitives.
2. **Smart Split Plan** — nondestructively plan separate compliant mesh assets,
   keeping relative positions, UVs and material assignments. Preview the
   number of generated pieces and cost consequences *before* preparing upload.
   Preserve rigging associations. Never silently delete triangles or modify
   creator originals.
3. **Jewelry profile** — prioritize silhouette and close-up reflections on
   pendants, chains, diamonds and prongs; clearly flag costly geometry. Offer
   opt-in normal maps for microscopic detail rather than claiming to render
   millions of real facets cheaply.
4. **LOD Workbench** — show each model's High/Medium/Low/Lowest plus physics,
   wireframes, material mapping, per-LOD triangle budgets and high-cost hotspots.
   A user must approve destructive optimization.
5. **Material and PBR checks** — warn about unsupported material channels,
   broken texture paths and missing UVs; distinguish preview quality from in-world
   results and SL capabilities.
6. **Validated Batch Upload** — editable queue, per-asset quote and fee
   confirmation, stop/retry semantics, resumable preparation and a report of
   successful or rejected uploads. Never automatically retry a paid upload
   without checking server status.
7. **Blender workflow** — optional exporter/plugin versioned with format
   documentation and a manifest for reassembly into linksets in-world.

## Acceptance and regression checklist

- Small unrigged mesh loads, passes preflight, calculates the real SL server
  quote and uploads through the unchanged payment path.
- Rigged meshes and joint uploads continue to use existing rules.
- LOD mismatch / missing High LOD is caught by both built-in validation and
  preflight. Invalid meshes never launch a fee quote.
- Boundary testing: 65,534 vs 65,535 effective vertices *in one loaded face*,
  8 vs 9 material faces per part, non-triangle index count, zero-face model.
- Excessive face triangle count logs a warning; does not pretend that the
  historical 21,844 threshold is universal.
- Large jewelry sample reports named hotspots without hanging the UI.
- Physics model handling, scene transforms, preview controls and L$ quote
  display remain intact.
- Test on a real Windows build. Exercise Agni/Beta grid upload separately.
- No new external library or background worker is added in this increment.

## Current verification status

Code and XUI changes committed to an isolated feature branch. No successful
Windows compile, live simulator upload or in-viewer UI screenshot is yet confirmed.
