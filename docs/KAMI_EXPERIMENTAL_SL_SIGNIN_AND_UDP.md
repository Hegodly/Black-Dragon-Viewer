# KAMI Beyond Reality — Experimental SL login and simulator UDP

**This test build is NOT a complete Second Life viewer.** It now has
an independently authored **experimental authentication and UDP simulator
transport**. It does not render online regions or report an in-world
session, because SL capabilities, event queues, a fully working interest
list, terrain/asset decoding, remote avatars and animation still need
implementation.

## Testing on a Windows PC

1. Download the build artifact for this development branch (not an old zip).
   Run **KAMIViewer.exe** (not the DX12 smoke executable).
2. Open **HOME [F1]**. Press **N** for a credential-free test of HTTPS
   connectivity to the official main-grid login host.
3. Press **L**, or click **SL LOGIN TEST**. Read the warning and the official
   Terms of Service at `https://secondlife.com/corporate/tos`.
   Clicking **NO** opens the Terms in the browser. Clicking **YES**
   represents the user explicitly confirming acceptance and opting in
   to a real HTTPS login attempt. Cancel avoids login.
4. Windows displays its native credentials prompt, addressed to
   `login.agni.lindenlab.com`. Use your *Second Life username*
   (not the display name). Passwords are never saved to disk,
   the GitHub repository, telemetry or a debug log.
5. Observe the status on HOME. Distinguish **AUTHENTICATING**,
   **LOGIN ACCEPTED**, **SIMULATOR REPLIED**, **REGION HANDSHAKE**,
   **MOVEMENT CONFIRMED**, and **SIMULATOR UDP ERROR**. None of these
   mean an online scene is ready yet. If authentication is rejected,
   no UDP connection is opened.
6. KAMI's original DX12 test world remains an **offline environment**
   until the event queue, objects, mesh/terrain decoders and live
   avatar loading are integrated.

## Implemented

- Per-session, non-persistent, Windows native credentials prompt.
- Opt-in user Terms acknowledgment, with a link to the official Terms.
- Independently authored XML-RPC `login_to_simulator` request and
  parser, sent over verified WinHTTP TLS to official login hosts.
- Asynchronous HTTP login so KAMI's D2D UI continues to respond.
- Explicit KAMI channel/version, not an impersonated viewer identifier.
- Runtime password and temporary credential buffer clearing.
- Login reply validation: agent/session UUIDs, simulator endpoint,
  circuit code, seed capability HTTPS URL, status/errors.
- Real Winsock UDP socket bound by `connect` to the host/port
  supplied by the accepted SL login response.
- Original LLUDP `UseCircuitCode`, `CompleteAgentMovement`,
  `RegionHandshakeReply`, `PacketAck` and `CompletePingCheck`
  encoding, with correct UUID wire ordering and U32 endian conversion.
- Incoming reliable ACK and ping handling; startup retry with
  bounded attempts and a deadline; explicit server-response states.
- Loopback UDP test which exercises actual send/receive, startup,
  synthetic region handshake, and agent movement identity.
- **No** legacy 3D backend; the user-facing scene remains hardware DX12.

## Not yet complete

- This is an **experimental login adapter**. Official viewer sign-in
  methods and required account actions can change.
- Server MFA challenge / secure MFA token continuation and any
  server-required critical notice / Terms *challenge* dialog are not
  yet supported. The login response may report these as requiring
  user action; KAMI must not pretend it completed them.
- Caps/LLSD `Seed` POST, EventQueueGet long poll, updated region
  sizes and actual decoded RegionHandshake fields.
- AgentUpdate, region/object update stream, parcel state, texture,
  mesh/terrain/J2K asset decoding, inventory, remote chat and teleports.
- Reliable UDP reordering/deduplication, persistent circuit ACK resend
  manager and full message-template decoder. The current transport
  acknowledges incoming reliable messages and retries only initial
  startup messages.
- Production review of login security and SL Third-Party Viewer
  requirements, and live testing with a **test account**, before
  general account use or public distribution.

## Safety

- Do not enter real SL credentials into GitHub issues, chat messages,
  logs or screenshots. Always type into the local Windows prompt.
- This repository is presently public. No passwords, auth tokens or
  signed capability URLs should ever be committed.
- The TPV policy forbids viewer identifier spoofing and bypassing
  SL asset permissions. All integrations must honor server rules.
- Software DX12 WARP is not a shipped gameplay fallback; hardware DX12
  remains the viewer default.

Official documentation:
- https://wiki.secondlife.com/wiki/Login_sequence
- https://wiki.secondlife.com/wiki/UseCircuitCode
- https://wiki.secondlife.com/wiki/CompleteAgentMovement
- https://wiki.secondlife.com/wiki/RegionHandshake
- https://secondlife.com/corporate/third-party-viewers
