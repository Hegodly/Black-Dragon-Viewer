# KAMI Live Second Life Essentials — implementation boundary

**Status:** Windows-only KAMI-original DX12 **offline viewer prototype**.
This is not yet a connected Second Life viewer. The code here adds network
and data contracts; it does NOT authenticate or log a Resident into SL.

## Implemented in this milestone

| Layer | KAMI implementation | Limit |
|---|---|---|
| Secure transport | `protocol/transport.cpp` uses Windows WinHTTP to send user-triggered, no-credential, HTTPS **HEAD** to the main-grid login host; HTTPS security checks are not bypassed | HTTP success/rejection means TLS reachability only; **NOT login** |
| UI | HOME → **Test Secure SL Route [N]** and a distinct, honest OFFLINE indicator | There is no login form and the app does not request account passwords |
| Authentication lifecycle | `Bootstrap` validates login reply identity and initial simulator host/port/seed-cap references, then enforces ordered login → circuit → capabilities → region states | There is no XML-RPC/web-auth client, token verification or active LLUDP circuit yet. “Verified” parameters are **adapter contracts** only |
| Secure capability references | HTTPS-only URLs, strict size/host/path checks, reject local loopback, userinfo, fragments and non-HTTPS | URLs must come from an authenticated simulator—not chat or user input. Signed capability tokens are never logged or displayed |
| LLUDP datagrams | `protocol/lludp.cpp` independently encodes and decodes packet flags, BE sequence numbers, extra header bytes, zero-compressed messages and appended acknowledgments | Not yet connected to a UDP socket, circuit retransmission/acknowledgment scheduler, or message-template field decoder |
| Networking requests | `Services` validates and stages operations for inventory, chat, IM, friends, groups, teleport, map, parcels, building, appearance, assets, animation, sound and voice. Session-generation matching and server-result gating | No actual outbound LLUDP packets, event queue or capability POST; no permissions circumvention |
| World and inventory | `LiveData` validates received object IDs, positions, mesh references, inventory entries, and duplicates. Data bound to authenticated session generation | No server decoding or streaming; the DX12 renderer still draws local test geometry |
| Avatar appearance | `LiveData::apply_avatar` validates avatar, fitted mesh, textures, attachments and optional physics wearable references | No real SL inventory access or fetched asset decoding. KAMI's eight-axis rigging remains local |
| Tests | CTest validates URL/transport policy without making network requests, state ordering, disconnection, fake reply rejection, service intent gating and received data limits | CI passing does not prove live Second Life connection |

## What must still be implemented for a real login

1. Official-compatible, consent-based SL authentication flow with a unique
   KAMI viewer identifier, Terms of Service handling and clear privacy
   disclosure. **Do not request or store account passwords without a
   fully reviewed secure authentication mechanism.**
2. XML-RPC / web-login return parsing as applicable; verify agent and
   session identifiers, simulator endpoint, region handle and seed cap.
3. Reliable LLUDP circuit: the packet envelope codec now exists, but
   UseCircuitCode, CompleteAgentMovement, RegionHandshake, AgentUpdate,
   AgentMovementComplete, retransmission timers and socket I/O still need
   implementation. Message IDs and blocks need strict template decoding.
4. Authenticated simulator capabilities, LLSD encode/decode, discovery,
   EventQueueGet, protocol timeouts and disconnect handling.
5. Region ObjectUpdate / terrain and mesh asset decoders; permission-aware
   object and inventory ingestion; GPU upload to the live DX12 renderer.
6. Real Second Life / Bento skeleton transforms, animations, worn outfits,
   collision-volume bone mapping, attachments and physics wearables.
7. Inventory read/edit with server permissions; nearby chat, IM, groups,
   presence, maps, landmarks, teleport with simulator-confirmed completion,
   voice service integration, sound/audio output and world-building.
8. Tests on real regions, multiple body brands and low-end GPU hardware,
   including crash recovery, credentials, permission boundaries, and FPS.

## Current code boundaries

- `include/kami/session.hpp`: intentionally offline; `can_enter_world()`
  remains false. No developer should substitute `Bootstrap::online()` for
  true live login without authenticating the full flow.
- `include/kami/protocol/transport.hpp`: official host TLS reachability check.
- `include/kami/protocol/bootstrap.hpp`: typed protocol handshake validation.
- `include/kami/protocol/services.hpp`: service commands and safe data staging.
- `include/kami/protocol/lludp.hpp`: real LLUDP datagram envelope and
  zero-compression codec, with no socket/circuit layer yet.
- `include/kami/features/rig_physics.hpp`: eight local spring channels.
- `docs/KAMI_ADVANCED_PHYSICS_CODE_COMPARISON.md`: physics comparison and
  real SL limitations.

## Compliance and source ownership

KAMI source in this branch is independently authored; it does not import
Linden Lab or third-party viewer source. The code was informed by publicly
documented protocol descriptions, not copied LGPL implementation.

Relevant references:
- https://wiki.secondlife.com/wiki/Protocol
- https://wiki.secondlife.com/wiki/Login_sequence
- https://wiki.secondlife.com/wiki/Current_login_protocols
- https://secondlife.com/corporate/third-party-viewers

The policy requires compatible operation, a unique viewer identifier,
privacy and support disclosures, ToS support, and adherence to asset/identity
permissions. Any future proprietary distribution needs a licensing review.

**Important repository warning:** The existing GitHub repository is still
public. To keep KAMI proprietary, move the independently authored branch to
a user-approved private repository, with appropriate history/source review,
before adding secrets or commercial releases. Do not place credentials,
tokens or real avatar/inventory data into public source control.
