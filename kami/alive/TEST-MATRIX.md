KAMI ALIVE BETA - TEST MATRIX

Global ALIVE graphics toggle must be enabled. In edit mode select a face of a
PBR object. Use KAMI > Workspaces > ALIVE / Inspect and apply a profile.
Profiles use existing material data, not new uploads. Material hashes are
viewer-local and shared by objects with identical PBR materials.

Modes: Standard (-1), Automatic (0, smooth metal only), Polished Metal (1),
Gemstone/Diamond (2), Living Skin (3), Living Eyes (4), Fabric (5), Leather (6).

Test one PBR surface per mode, rotating the camera and changing sun/EEPs.
Compare OFF, Auto, profile, Standard; restart and check override persistence.
Test a legacy/BOM non-PBR skin: it should not be falsely classified as PBR.
Test p95/p99 frame times moving through occupied regions. Check glints for
unwanted glow, aliasing and eye over-brightening. Check metallic jewelry when
non-metal profiles are applied and check selection of multiple faces.

Not yet implemented: physical subsurface scattering, corneal tearline mesh,
real gemstone caustics or physically accurate sparkle lighting, automatic
semantic skin/eye/cloth recognition, non-PBR material enhancement, DX12 port.
