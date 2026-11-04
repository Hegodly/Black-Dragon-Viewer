# KAMI — R27 through R32 unified test

This is ONE portable Windows build, not six independent applications.

## Implemented and automated-testable
- R27: bounded missing-root retries, compressed linked-child transforms, and reasons for zero rendered solids
- R28: uncut/unhollowed box, cylinder, sphere, and straight triangular prism geometry, with verified parent transforms
- R29: default TextureEntry reference presence from real SL ObjectUpdate packets (no texture streaming)
- R30: validated AvatarAppearance metadata from simulator, own-avatar counts (no real avatar visuals yet)
- R31: user-triggered nearby channel-0 chat send and incoming simulator chat display, without fake delivery/read states
- R32: closest-first 128-shape geometry cap, coalesced DX12 scene upload and existing GPU smoke tests

## Not implemented / not falsely claimed
Full arbitrary SL prim/mesh/sculpt volume tessellation; real J2C texture downloads and decoding;
Bento skinned avatar/attachments, wearable inventory, real group/IM messaging, teleport and
interactive edit workflows. These require more source integration and live SL verification.

## Live test
1. Extract KAMI-R27-R32-Unified.zip and start KAMIViewer.exe.
2. Log in with a separate test account. On F2 WORLD, check SOLID ROOTS/SOLID LINKED.
3. F9: note Root, Linked, Missing, Requested, Eligible, No parent, Far, Budget and Texture IDs.
4. F9 avatar line: compare AvatarAppearance packets and Own visual params; the visible avatar is still local.
5. In Messenger, switch to NEARBY CHAT after simulator connection. Send a harmless message and
   verify another independent viewer receives it; remote display is not a delivery receipt.
6. Check FPS/stability with populated regions. Run KAMI-DX12-Smoke.exe for a local renderer test.

Do not paste Second Life passwords, session tokens, signed capability URLs or private chats
into public issues or screenshots.
