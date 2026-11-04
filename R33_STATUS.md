# KAMI R33 — Parent Hierarchy + Mesh Intake Test
One experimental portable Windows DX12 build based on R27–R32.

Implemented:
- Track server-originated PCode 47 avatar root local IDs separately from real in-world object roots.
- Differentiate missing world ancestors, known avatar attachments, invalid rotations and cycles.
- Safely resolve longer valid link chains up to 16 transforms without fabricating parent positions.
- Recognize mesh asset UUIDs from the real Sculpt ExtraParams 0x30, type 5 and queue deduplicated mesh requests.
- Preserve mesh references across compressed transform updates.
- F9 reports mesh references/queued jobs and useful missing-anchor reasons.
- Preserve native DX12 terrain, walking/turning, local placeholder, and public nearby chat.

NOT yet implemented: HTTP CAP mesh downloading; binary LLSD mesh LOD/zlib decode; GPU mesh rendering;
J2C texture downloading; appearance wearables, group/IM/inventory sync.
Build success does not prove your platform is visible.

Test: Log in, open F2 and F9, compare SOLID, Missing root, Avatar parent, Cyclic,
Mesh refs, Mesh queued and FPS. Send a screenshot if SOLID remains zero.
Do not post simulator capabilities or authentication credentials online.
