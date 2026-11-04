# KAMI R40 — Safe CAPS rejection classification

## Purpose
R38/R39 in-world reports from Attalia confirmed EventQueue works while the
authenticated mesh capability still reported rejection. R39 allowed public
HTTPS CDN hostnames but did not reveal *which* validation rule failed.
This build isolates that failure without showing signed bearer URLs.

## Changes
- Preserve R39's authenticated CDN-capability handling and strict EventQueue
  URL policy. No additional hostnames or insecure schemes are allowed.
- Categorize rejected mesh grants by LLSD grant type, empty value,
  non-HTTPS scheme, unsafe URL syntax, unparseable URL, missing hostname,
  non-public/reserved hostname, invalid path, or unsupported layout.
- The F2 persistent Seed line displays the rejection **category only**.
  It never shows hostname, signed path, tokens, UUIDs, or raw LLSD.
- Add offline regression checks for common host, scheme and LLSD errors.
- The F9 footer displays **R40** to distinguish an older portable EXE.
- No changes to the native DX12 renderer, terrain, lighting, primitive
  shape pipeline, material settings or FPS optimizations.

## Next in-world test
Log in and open F2. Photograph the separate Seed line. Also open F9
and confirm the footer reads R40; share CAP and MeshID counts.
If the Seed line says `non-HTTPS scheme`, this is a server/public URL
compatibility issue requiring a security review, not permission to
disable TLS. If it says `hostname not public`, investigate authenticated
Linden asset URL conventions before making any allowlist change.

## Not complete
Actual in-world mesh drawing remains unverified; textures, materials,
rigged avatars and inventory/chat event handling remain incomplete.
No Unreal Engine integration is included.
