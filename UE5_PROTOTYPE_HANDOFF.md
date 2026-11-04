# Separate future UE5 prototype — KAMI Beyond Reality

The user intends to start **a separate new conversation** to experiment with
an Unreal Engine 5 Second Life viewer prototype. This is NOT permission to
replace KAMI's existing renderer or merge an Unreal project into the current
DX12 branch. First finish/verify the current KAMI native DX12 build.

## Prototype goal
Create an Unreal Engine 5 proof of concept for original and legal Second Life
asset ingestion and rendering. Use a separate branch or repository. The minimum
demo should be offline and verified: decode a sample unrigged SL mesh LOD and
display it inside a UE5 scene, then evaluate materials and performance. Only
after that should authenticated simulator connections be considered.

## Non-negotiable design goals
- Preserve KAMI Beyond Reality branding and clean Windows-first UI.
- Native DX12 viewer remains independent; UE5 is an optional R&D branch.
- Maintain FPS-first optimization and preserve quality/preset behavior.
- Target RTX 5070 12 GB and scalable lower-end hardware; track 1% lows and
  frame times in addition to average FPS.
- No false claims of actual Bento, inventory, IM, wearable physics, DLSS etc.
- Follow Linden Lab's third-party viewer rules, asset permissions and server
  limits; keep credentials/capability bearer URLs private.
- UE5 is source-available under Epic's EULA, not conventional OSS. Review
  license compatibility of any reused LGPL components before linking/packaging.
- A successful Unreal offline render is not a production Second Life viewer.

## Repository reference
Current native viewer branch:
release/kami-r36-consolidated-audit-20261104
GitHub: https://github.com/Hegodly/Black-Dragon-Viewer
Read R36_STATUS.md for current verified scope.
