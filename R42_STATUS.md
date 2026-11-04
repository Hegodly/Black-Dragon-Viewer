# KAMI R42 — Object Mesh Intake and Compressed Particle Compatibility

## What changed
- The official Second Life viewer can carry a **legacy 86-byte particle system**
  (68-byte system block + 18-byte particle block) before compressed ExtraParams.
  KAMI previously declined every compressed ObjectUpdate with this SpecialCode,
  potentially losing actual mesh UUIDs on these packets. R42 now skips this
  fixed-width legacy field using strict message bounds, then decodes subsequent
  ExtraParams and volume parameters.
- The 0x30 Sculpt ExtraParam (17 bytes containing UUID + sculpt type) is parsed
  and counted, and only type 5 mesh UUIDs can enter the mesh asset scheduler.
  A bounded shared scanner validates entire ExtraParams payloads and does not
  extract IDs by searching arbitrary packet bytes.
- The full ObjectUpdate path now counts valid ExtraParams, Sculpt entries,
  and malformed ExtraParams. The compressed path counts legacy particle
  records processed along with Sculpt and mesh references.
- F9 separates FExtra / FSculpt from CExtra / CSculpt / Particle / Skip so
  in-world evidence can identify which object transport path carries the data.
- Regression tests cover both successful legacy particle + mesh UUID recovery
  and malformed/truncated particle data, plus bounded ExtraParams validation.

## Preserve existing working functionality
R41 CAPS mesh grant, EventQueue, DX12 terrain and simple primitive
rendering are intentionally left intact, as are the prior scene upload
coalescing and priority scheduler optimizations.

## Real SL test procedure
1. Extract the R42 ZIP into a fresh folder, run KAMI/KAMIViewer.exe.
2. Login, open F2 to initialize the DX12 world, and verify terrain/sky.
3. Open F9 and screenshot MeshID / CAP / Fetch / LOD plus FExtra, FSculpt,
   CExtra, CSculpt, Particle and Skip; the bottom status now reads R42.
4. If MeshID increases, inspect Queue/Fetch/LOD and the actual appearance
   of mesh objects. Do NOT assume a mesh rendered because it was requested.
5. If MeshID stays zero even with CAP YES, further object cache recovery,
   missing object requests, sculpt parameter handling or other packet
   fields require investigation using consented, redacted diagnostic counts.

## Still incomplete
True in-world mesh platform/rendering, texture and material downloads,
Bento/fitted avatar rendering, inventory, group IM, and full server event
handling remain incomplete until explicitly verified. Automated unit tests
cannot establish in-world success. The viewer is an experimental prototype.

## Protocol source reference
The official Second Life viewer LLViewerObject::processUpdateMessage,
LLPartSysData::unpackLegacy, and LLSculptParams::unpack informed these
bounded parser changes. Any reused code must follow existing licenses.
