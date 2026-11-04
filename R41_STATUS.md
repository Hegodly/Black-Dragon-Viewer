# KAMI R41 — Legacy Second Life Mesh CDN Compatibility

## In-world evidence
KAMI R40 in Attalia received EventQueueGet and confirmed simulator movement.
The preserved Seed diagnostic was "mesh rejected (non-HTTPS scheme)".
The earlier F9 screen had live DX12 geometry, ten solid primitive shapes,
but CAP NO, MeshID 0, and no fetched mesh geometry.

## Protocol evidence
Official SL Capabilities documentation says capability URLs may be http: or
https:, may use different hosts, and needn't start with /cap.
Historical official SL CDN notes list GetMesh/GetMesh2 and GetTexture
as http://asset-cdn.aditi.lindenlab.com/ URLs.
- https://wiki.secondlife.com/wiki/Capabilities
- https://wiki.secondlife.com/wiki/Beta_Server_Office_Hours/Minutes/2014-08-21

## Changes
- Keep login, CAPS seed and EventQueue on authenticated HTTPS.
- Accept legacy HTTP mesh only for the unsigned public root at
  asset-cdn.*.lindenlab.com on port 80, only when supplied by an
  authenticated simulator seed response.
- No username/password, cookies, session token, capability-bearer query,
  arbitrary remote hosts or private addresses can use HTTP.
- All other CDN mesh grants must still use HTTPS.
- Select HTTP/HTTPS WinHTTP flags; no redirects; limit response to 8 MiB;
  keep bounded asset decoding.
- Preserve DX12 rendering and viewer performance optimization.
- Add tests for canonical legacy grants and rejected hostile hosts/paths.

## Security caveat
Plain HTTP asset transfers are NOT encrypted or authenticated in transit.
This limited exception transfers a mesh UUID request and its returned
asset bytes, not a secret capability URL. Content can be modified
in transit; never broaden this exception to arbitrary sites.

## In-world validation
1. Extract R41 into a new folder; close other KAMI instances.
2. Log in and check the F2 Seed diagnostic.
3. Enter the native DX12 world via F2; open F9. Footer should read R41.
4. Screenshot CAP, MeshID, Fetch, LOD, Extra, Sculpt, SOLID and Mesh DRAW.
5. CAP YES with MeshID 0 means object mesh discovery is the next blocker.
Do not share signed capability URLs or passwords.

Still experimental: mesh/textures, full rigged avatars, inventory,
IM and event applications are not fully verified.
