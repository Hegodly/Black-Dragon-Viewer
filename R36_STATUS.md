# KAMI R36 — Consolidated DX12 / SL object rendering release

## Changes since R34
- R35/R36 correct compressed ObjectUpdate path/profile primitive decoding using
  the official Second Life 23-byte path/profile layout. Prim shape classification
  is shared between full and compressed update code paths.
- Validated compressed updates no longer silently overwrite newly received mesh
  IDs with older full-update data.
- Viewer reports primitive updates and supported shapes separately from loaded
  meshes, while preserving world-origin and avatar-attachment distinctions.
- Duplicate object updates with identical position, scale, rotation and mesh/shape
  metadata no longer increase spatial revision or trigger an unnecessary world
  rebuild. Regression tests cover those differences.
- Only nearby world-rooted mesh objects are considered for fetch priority. The
  viewer defers avatar attachments until proper skinning support exists.
- Mesh capability GET construction matches the path convention used by the
  official viewer. HTTPS host validation, redirect refusal and bounded downloads
  remain enabled.
- Native DX12 world rendering and 800 ms world-upload coalescing remain intact.

## Known limitations — not finished
- Real grid screenshots of populated mesh scenes are required before claiming
  that live meshes, full linksets or the user's platform render correctly.
- Avatar appearance is still a local placeholder, not Bento rigged skinning.
- Simulator land, prim and mesh ingestion remain a partial implementation.
- Actual texture/normal/PBR material asset download and GPU binding are incomplete.
- Individual/group IM, inventory mutation/smart sort and teleport workflows are
  not server-integrated; the latter are local planning or UI only.
- No UE5 code has been integrated. This branch is native DirectX 12 only.
- Reliable performance comparison requires frametime captures on actual SL regions.
- Automated Windows tests do not replace manual authorized-world tests.

## Test release
Extract the Windows ZIP, launch KAMIViewer.exe and log in using the existing
experimental login. Open F2 and F9; check **MeshID**, **Queue**, **Fetch**, **LOD**,
**Prim updates**, **Eligible**, **Orphan** and **Attached**. Report whether the
platform has real geometry, and separately whether textures appear. Do not post
passwords, session UUIDs, simulator capability URLs or raw private chat logs.

## Caution
The viewer is an experimental prototype, not feature-complete Second Life
client software. Keep it separate from your main installed viewer.
