# KAMI R39 — Authenticated Mesh CDN Capability Support

## Why this version exists
Real-world R38 diagnostics showed:
- EventQueueGet functioning in Attalia, with simulator movement confirmed.
- CAPS seed result: EventQueue OK; mesh URL rejected by host policy.
- Previous F9 counters showed working DX12 terrain/ten solid primitive shapes,
  but MeshID=0 and CAP NO.

The official Second Life capabilities protocol explicitly states that
a capability need NOT share the host of another capability and may point to
a CDN or a different path. KAMI incorrectly imposed Linden-only hostnames
on all capability grants, including ViewerAsset.

Reference:
https://wiki.secondlife.com/wiki/Capabilities
https://github.com/secondlife/viewer

## Changes
- Separate trust policies for seed/EventQueue versus **mesh assets**.
- Preserve strict Linden-owned allowlist for EventQueue, login and seed URLs.
- Permit authenticated simulator-issued ViewerAsset/GetMesh capability
  endpoints at public HTTPS CDN hostnames. Handle valid bare-host CDN URLs,
  which previously failed the requirement for a non-root /cap/ path.
- Reject insecure HTTP, IP literals, localhost and reserved/private
  DNS suffixes, credentials in URL, fragments, invalid DNS labels and
  malformed URLs. TLS certificate verification remains Windows-managed.
- Redirects remain disabled for mesh downloading; existing 8 MiB response
  cap and worker-thread decode stay in place.
- Add regression tests for CDN acceptance and hostile URL rejection.
- Preserve R38 F2/F9 telemetry, the DX12 terrain and 10-prim rendering path,
  and all previous asset queue FPS work. This is not an engine migration.

## In-world validation needed
1. Extract R39 into a **new folder**, then open KAMI/KAMIViewer.exe.
2. Open F2 after login: the persistent **Seed:** line should show whether
   a mesh capability was actually granted under the new HTTPS policy.
3. Open F9 after entering the live DX12 world. Record MeshID, Queue, CAP,
   Fetch, LOD, Shapes, Extra, Sculpt and Skip. Do not share signed URLs.
4. Check whether actual mesh platform/buildings appear. The F9 footer
   should say **R39**, confirming this exact build.
5. If CAP YES but MeshID 0, the remaining blocker is mesh extra-parameter
   discovery/object updates rather than CDN host validation.

## Unfinished systems
No verified real-world mesh rendering yet. Second Life textures/materials,
rigged Bento avatars, full attachments, inventory, IM and group chat are
not feature-complete. CI cannot prove these simulator interactions.
