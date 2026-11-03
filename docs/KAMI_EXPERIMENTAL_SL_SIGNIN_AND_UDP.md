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

## Login HTTP 400 — third-party XML-RPC request validation

A user screenshot from November 3, 2026 showed **HTTP 400** on the
experimental Login Test. This status confirms an HTTP error response, not
successful account authentication; its exact cause cannot be proven from
the HTTP status alone.

The original build also sent a raw colon-delimited physical MAC string.
The modern official login request uses a hashed unique machine string in
the `mac` field; KAMI now hashes the adapter identifier into a 32-character
hex value before sending it and does not transmit the physical MAC. A stable
hash remains a pseudonymous identifier and is disclosed as such.

Legacy accounts may log in with `first.last` or `First Last`, while newer
single-word accounts use the `Resident` surname. KAMI now normalizes the
legacy dotted form correctly, rather than incorrectly adding `Resident`.

The original request lacked the `id0` field listed among required
parameters in the documented Second Life XML-RPC login contract. The
experimental request now sends the documented all-zero *anonymous*
placeholder rather than a raw disk serial number, and opts in to
`extended_errors` structured error information. This is a candidate
protocol compatibility fix **not verified against the live server**.
The message for 400 no longer claims that the server conclusively rejected
the request *format*. The service might also reject unrecognized viewer
versions, account restrictions, or intermediary HTTP requests.

Do not print XML-RPC password hashes, account identifiers, capability URLs,
HTTP response bodies or MFA tokens in diagnostic UI or logs. No automatic
login retries should be added; users must initiate each attempt. The
prototype does not support MFA continuation, a full ToS challenge flow,
caps/event queue, live world assets or actual region rendering.

Protocol reference: https://wiki.secondlife.com/wiki/Current_login_protocols

## Login request correctness follow-up (November 3, 2026)

Experimental login now emits XML-RPC booleans for `agree_to_tos` and
`read_critical`, initially both false. The local KAMI test warning is not
the actual Second Life Terms/notice confirmation; KAMI must not silently
acknowledge a server challenge. The authenticated login HTTP receive timeout
is now 30 seconds; the password-free TLS probe remains short. The XML parser
and HTTP reader now share a bounded 8 MiB reply limit so optional large
inventory data need not invalidate authentication. Offline regression tests
cover protocol types and a large XML response. This is an unverified client
fix, **not proof of successful live SL authentication**. Passwords, session
tokens and capability URLs are not written to diagnostics.

## Further investigation after repeated HTTP 400

A second user test still returned HTTP 400, disproving the hypothesis
that the earlier booleans/timeouts alone would resolve the problem.

This follow-up corrects the machine identifier to hash the **six raw
adapter bytes** rather than the printable colon-separated address,
matching the official viewer's hashing semantics; the physical address
is never sent. It also adds non-secret typed request metadata used by
the current official login implementation, without impersonating its
channel/version or claiming the user acknowledged SL's Terms.

For non-200 replies, KAMI now reads no more than 512 bytes *only to
classify the response format*: empty, XML-RPC, HTML, JSON-like, or
unrecognized. The response bytes are then cleared, never shown or
logged; this helps distinguish login-service replies from intervening
HTML error pages without exposing any account or capability data.

These are **candidate compatibility changes**. Neither a real login
nor a useful Second Life session is proven by compilation/tests. Re-test
once with a dedicated test account and report only the HTTP code plus
response **category**. Do not supply raw HTTP bodies, SL credentials,
signed URLs or login payloads to issues or chats.

## Diagnostic revision R2

The hard-coded HTTP 400 instruction "check protocol" has been removed because
it was not a real finding. The app now reports "Request declined (KAMI DIAG R2)"
and retains a safe response **format** category, when readable. The new
marker lets testers distinguish the updated executable from prior builds.

The login POST now explicitly accepts XML responses. Offline regression
tests check generated login fields, required XML-RPC types and error status
messages. A proposed full-document MSXML test destabilized CI and was removed;
the existing response parser security tests remain. These tests **do not**
prove that the server will accept the request or that the serialized body
is compatible with its current implementation.

For privacy, avoid publishing any real authentication XML, raw HTTP response
body or token. If HTTP 400 recurs, share only the displayed status and category.
A browser-only GET/HEAD probe cannot validate credential-bearing POST replies.
