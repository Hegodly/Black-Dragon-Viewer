# KAMI R34 — Real Mesh Alpha

Windows DX12 experimental build based on R27–R33.

- Compressed object updates: decode mesh Sculpt ExtraParams.
- CAPS: negotiate ViewerAsset or GetMesh2/GetMesh and request asset bytes over HTTPS.
- Fetch: bounded to 8 MiB, no redirects, only trusted capability hosts, asynchronous.
- Decode: validate binary LLSD headers, mesh LOD offsets, zlib/gzip data, vertex domains and triangles.
- Render: untextured, static mesh triangle geometry in the existing DX12 world render pass,
  anchored only to verified world object roots/children; cap 18k mesh triangles per scene refresh.
- F9: Mesh refs, Q, Fetched, Decoded, Mesh DRAW to distinguish stages without fabricated success.
- Retains previously verified FPS-oriented terrain, movement, and nearby chat paths.

Known limitations:
This supports only selected static mesh assets. Complex SL mesh variants, UV/textures,
skin weights, attachment poses, avatar appearance, PBR and inventory/IM/group parity
are NOT complete. No automated test can prove that your particular Second Life platform rezzes.

To test: extract, run KAMIViewer.exe, log in, examine F2 and F9. Compare Mesh refs,
Q, Fetched, Decoded and Mesh DRAW, check actual platform pixels and FPS.
Please don't share credentials or signed capability URLs.
Decoder dependency: public-domain/MIT stb_image.h, source and license within third_party/stb.
