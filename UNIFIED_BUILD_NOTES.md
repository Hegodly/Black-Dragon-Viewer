# KAMI Unified R13 — one portable Windows test build

This ZIP contains the cumulative experimental KAMI Original Core Windows build,
with all fixes made on the linear R9 → R10 → R11 → R12 → R12 camera hotfix → R13
development history. This is **not** a full production Second Life viewer.

Included:
- Original KAMI DirectX 12 renderer, UI, shader cache, and offline preview
- Original experimental Second Life login protocol and simulator movement handshake
- R9: resilient WinHTTP EventQueueGet polling, including malformed 12152 responses
- R10: authenticated neutral AgentUpdate packet sender and world-interest delivery
- R11: typed terrain / wind / cloud and object packet classifiers
- R12: bounded land patch decoder and verified untextured live terrain mesh upload
- R12 camera hotfix: viewport looks toward real received terrain
- R13: stitch neighboring terrain patch edges without inventing absent heights
- Automated native Windows build and regression tests

Not yet implemented for the live SL grid:
- Full prim/mesh/object decoding and drawing, avatars and appearance rendering
- Region textures, materials, visual fidelity and lighting parity with SL viewers
- Production-ready inventory, chat, permissions, teleports or MFA

Testing:
1. Extract the complete ZIP. Run KAMI/KAMIViewer.exe on Windows.
2. Authenticate only if you accept the risks of the experimental login flow.
3. On HOME, confirm simulator movement and live packets.
4. Open WORLD to view the untextured decoded land preview.
5. Check the "LIVE land GPU vertices" count. Nonzero means uploaded triangles,
   not proof that complete textures, objects or avatars are available.

Build source: Hegodly/Black-Dragon-Viewer
Unified branch: release/kami-r13-unified-20261103
