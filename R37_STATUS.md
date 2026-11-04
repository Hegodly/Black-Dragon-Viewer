# KAMI R37 — CAPS seed recovery and independent mesh grants

## Observed in R36 user test
- Live simulator: Attalia; movement confirmed.
- DX12 primitive geometry: SOLID 10, AMBER 0, eligible 10.
- Linked objects: 601. Actual textures/materials not loaded.
- MeshID 0, Queue 0, CAP NO, Fetch 0, LOD 0.
- CAPS diagnostic: "CAPS seed missing valid EventQueueGet".
These observations prove R36 improved primitive rendering; they do NOT prove
full mesh, attachment or texture loading.

## R37 corrections
- Parse authenticated LLSD seed capability grants independently.
- Do not discard a valid EventQueueGet when an optional mesh URL is
  absent, <undef/>, unsupported or fails HTTPS/host verification.
- Do not discard a valid ViewerAsset/GetMesh2/GetMesh grant solely because
  EventQueueGet is absent or rejected.
- Prefer ViewerAsset, then GetMesh2, then GetMesh when selecting a valid
  mesh asset grant, and do not follow HTTPS redirects.
- Reject duplicate EventQueueGet names and malformed LLSD key/value maps.
- Show distinct safe diagnostics for invalid LLSD XML maps, untrusted
  EventQueue URLs, absent grants and event-only/mesh-only grants.
- Continue preserving R36 DX12 solid primitive decoding, mesh budgets,
  duplicate geometry update suppression, and FPS-conscious uploads.

## Follow-up test
R37 is **not yet proof** that the simulator grants a working mesh URL.
Log in on a test account, open F2/F9, and observe CAPS status plus
MeshID / Queue / CAP / Fetch / LOD / Prim updates / Eligible / SOLID.
If CAP is YES but MeshID stays 0, the object/mesh-reference decoder is
still a separate blocker. If CAP is NO, the refined CAPS diagnostic
should identify whether the server returned malformed XML, an unsafe
URL or no recognized capability. Never share signed URLs, credentials
or message bodies.

## Still not complete
Actual server mesh rendering, real textures/PBR assets, real Bento
avatar and attachments, full IM/inventory/teleport, inworld FPS
benchmarks and parity with an official SL viewer remain unverified
or unimplemented. This is a DX12-only KAMI build, not an UE5 build.
