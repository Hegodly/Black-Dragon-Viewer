# KAMI official Second Life backend migration

Status: **wire component integrated, full network/session backend pending**.
KAMI uses the official LGPL LLZeroCode encoder/decoder and official
0x60->0x30 legacy mesh type mapping through an independently replaceable
DLL. The full LLMessageSystem, login/session, objects and assets are not
migrated or proven live. Do not report this branch as a
completed login, asset, inventory or simulator connectivity migration.

## Objective
Replace the experimental hand-coded Second Life login/session, LLUDP,
message-template parsing, EventQueue/CAPS, asset fetching, avatar and inventory
implementations with compatible components based on Linden Lab's official
viewer, while **retaining KAMI's independent Windows DX12 world renderer**,
camera/UI and performance settings. No Unreal Engine, no OpenGL fallback.

## Audited official upstream
- Repository: https://github.com/secondlife/viewer
- Reference revision inspected: `5a43248759bd1b2e8d7ebf4ce00fe55c181824ac`
  (verify pin/branch and downstream licensing before vendoring source).
- `indra/llmessage/message.cpp`: LLMessageSystem, reliable circuits, ACK,
  retransmission, template dispatch, simulator message ingestion.
- `indra/llmessage/CMakeLists.txt`: `llmessage` includes dozens of
  translation units and links to `llcommon`, `llfilesystem`, `llmath`,
  `llcorehttp`. It cannot be copied into standalone KAMI as a single file.
- `indra/llprimitive`: message-independent prim, texture entry and ExtraParams
  interpretation, including PARAMS_EXTENDED_MESH=0x70,
  PARAMS_RENDER_MATERIAL=0x80 and PARAMS_REFLECTION_PROBE=0x90.
- `indra/newview/llviewerobject.cpp`, `llvovolume.cpp`: decode real object
  updates into viewer-state objects; this code depends on the official
  scene/object managers and may need an adapter rather than direct linking.
- `indra/newview` asset/inventory/avatar code is coupled to viewer state,
  cache and graphics. It must be separated behind a renderer-neutral API.

## Required implementation (in dependency order)
1. Stage 1 completed: integrate official LLZeroCode and PARAMS_MESH remapping
   in a separately replaceable DLL, with in-stream runtime integration and
   tests. See `OFFICIAL_SL_WIRE_STATUS.md`.
2. Pin and build the official LGPL `llcommon`, `llmath`, `llfilesystem`,
   `llcorehttp` and `llmessage` modules as a separately replaceable Windows
   DLL with an audited source/third-party dependency inventory. Keep their
   copyright headers and original licenses; provide compliant source to users.
3. Implement the versioned native C ABI in
   `include/kami/protocol/official_sl_bridge_api.h`. This header is only a
   contract today. A bridge MUST use official message/HTTP code internally;
   do not forward undecoded LLUDP frames and call the backend "official".
4. Route official login and simulator circuit lifecycle through the DLL,
   including authenticated login, reliable retransmits/ACKs, region
   handshake, EventQueue/CAPS and typed event dispatch. Avoid double
   connections from the legacy KAMI implementation. No secret strings in
   logs or diagnostics.
5. Publish immutable, validated **renderer-neutral** snapshots (region,
   object hierarchy, per-face material/texture references, mesh assets,
   terrain, avatar appearance) to KAMI; feed DX12 geometry and texture
   upload via a bounded, visible-first streaming queue.
6. Adopt official inventory, IM and group/agent update flows behind the
   same adapter after world objects render correctly.
7. Use unit fixtures from official wire formats and full in-world tests for
   login, teleport, object rez, assets, outfits and messaging. Measure FPS,
   network stalls, RAM and VRAM before disabling the legacy transport.

## ABI and license boundary
- KAMI executable and DX12 core do not include `LLMessageSystem` or
  `LLViewerObject` headers: their data crosses the C ABI as typed,
  fixed-width structs and explicit byte lengths.
- `KAMI_SL_OfficialBackend.dll` can have a different internal build and
  release schedule. KAMI's own renderer can remain independently authored
  and non-open-source, subject to review of licenses and LGPL compliance.
- The official viewer source states **LGPL 2.1 only** on individual files;
  inspect every copied file and third-party dependency separately.
  Distribution may require library source/patches, notices and user ability
  to replace or relink the LGPL-covered components.
- Never copy an official rendering/OpenGL pipeline into KAMI in the
  name of transport compatibility.

## Acceptance gates
- A C-ABI header and tests are NOT an official backend implementation.
- Official DLL loads with a compatible ABI and has a real authenticated
  network-session implementation: only then mark "official backend active".
- Live objects: >0 real mesh refs, successful downloads, geometry decoded,
  and in-world solid meshes displayed accurately, verified against the
  same region in the official SL viewer.
- Textures: real texture IDs and per-face image data displayed (not
  procedural placeholders).
- Avatar: own and nearby skin, rig and outfit render.
- Simulator messages, inventory and chat survive reconnect and teleport.
- Preserve DX12-only and performance settings throughout.
