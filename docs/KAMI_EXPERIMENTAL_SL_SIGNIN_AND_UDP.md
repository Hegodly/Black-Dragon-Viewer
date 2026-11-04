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

## Official Windows viewer identity compatibility bridge (Nov 3, 2026)

This branch replaces the original KAMI `id0` placeholder
(`00000000-0000-0000-0000-000000000000`) with the official Second
Life Windows viewer behavior from
`LLAppViewerWin32::generateSerialNumber()`: MD5 of the raw 32-bit C:
volume serial number, output as 32 lowercase hexadecimal characters.
The raw serial is never sent or logged.

The adapted official algorithm lives in its own replaceable
LGPL-2.1 DLL (`KAMI_SL_Login_Identity.dll`) and is packaged with
the applicable source notices and license. The new deterministic
regression test checks the official Windows input-byte ordering.
An unavailable volume ID produces a local error, not a fabricated
device identifier.

This **does not** import the full official SL viewer login subsystem:
the original viewer depends on LLSD, LLCore HTTP, credential storage,
grid configuration and event-driven login handling. Those modules
would require a much larger dependency-aware port, license compliance
and simulator validation. Full SL login and region loading remain
experimental and must be confirmed on the live grid.

## KAMI DIAG R3: visible, structured HTTP 400 information

The R2 login diagnostic displayed the HTTP status, response classification
and advice in one clipped line; when the WinHTTP response-body read failed,
the classification was omitted entirely.

R3 always stores the HTTP status and a *safe* response category in the login
result. UI shows HTTP status + diagnostic revision, response classification,
and pending simulator state on three distinct lines; it does not reveal the
HTTP body, password/hash, account name, signed simulator capability URL,
cookies, token or machine fingerprint.

If WinHTTP cannot read the response body, the category displays a WinHTTP
GetLastError numeric code. Empty, HTML, XML-RPC, JSON-like and plain-text
error replies have distinct labels. A server HTTP 400 is still **not**
proof of a malformed XML-RPC request; neither this diagnostic patch nor the
official Windows machine-ID bridge proves the live login works.

## Additional official-login compatibility fields

KAMI now sends its **actual** Windows NT platform version and platform
description from RtlGetVersion when available, an empty optional
host_id, and the official viewer's commonly requested options
(including adult_compliant, inventory library, notifications and maps).
The user agent declares `KAMI Beyond Reality Dev 0.1.0 (53)` rather
than claiming to be Linden Lab's viewer. The client still sends
credentials only to the official Linden Lab TLS endpoint.

These fields match the public structure of Linden Lab's viewer request
but do not guarantee authorization. Response Content-Type is classified
safely if the HTTP error body is unreadable. Login remains experimental.

## Synthetic request structural regression

The Windows workflow now generates a strictly synthetic login request
with a known fake account/password digest and validates it independently
with .NET System.Xml using DTD processing disabled. CI verifies
XML well-formedness, required member keys, no duplicate keys, XML-RPC
array options and typed boolean ToS controls. It does not send the
request to Second Life or include any real account credentials.

Passing this test proves only that the constructed XML is structurally
well formed. HTTP 400 server acceptance remains unverified.

## Official Windows hashed machine ID algorithm

Earlier builds calculated login `mac` from the first active physical
network adapter. The modern official viewer normally **does not use
that identifier first**. Its `LLMachineID::init()` selects the WMI
`Win32_ComputerSystemProduct.UUID` if present and valid, otherwise
`Win32_OperatingSystem.SerialNumber`. The serial string is folded by
accumulating its UTF-16 character values modulo 256 across six bytes,
and those six bytes are MD5-hashed into the `mac` login value.

KAMI now calls the LGPL-separated device identity DLL for that WMI-first
algorithm. The prior adapter-based MD5 is only a fallback when WMI
fails, as in the official viewer's final `LLUUID::getNodeID()`
fallback. CI tests use synthetic serial numbers and never query real
hardware identifiers. All identifiers are kept in memory only,
apart from the hashed identifiers sent over TLS to the SL login server.

This is another concrete protocol implementation difference corrected,
but no live login result is yet verified.

## R4 official XML-RPC wire-format parity

A user test of R3 returned HTTP 400 and a WinHTTP 12152 error when
reading the server's error body. Microsoft defines 12152 as an invalid
server response: WinHTTP could not parse it. This is **not** equivalent
to proof that the outgoing login XML was malformed.

Compared with Linden Lab's current `LLXMLRPCTransaction`,
KAMI's XML declaration and Content-Type differed. R4 now sends
`<?xml version="1.0"?>` and `Content-Type: text/xml`, avoiding extra
request headers not used by Linden Lab's XML-RPC login implementation.
The payload still goes only to the official login URL over HTTPS,
with a KAMI-identifying user agent. Offline tests assert the canonical
XML declaration and explain WinHTTP 12152 explicitly.

It remains possible that the service rejects KAMI's application or
other authentication details. This commit does not silently switch
to web SSO, disable TLS verification, downgrade encryption, impersonate
another viewer, retry automatically, or log credential-bearing traffic.

## KAMI DIAG R5 / transport-stage diagnostics

R5 distinguishes WinHTTP errors raised **sending the login POST**,
**receiving the HTTP response**, **reading the HTTP status**, and
**reading a successful response body**. A numeric Windows error code
is recorded for the stage, without disclosing the request body,
credentials, any response body, machine identifiers, or cap tokens.
The persistent HTTP 400 remains separately displayed.

Microsoft defines error 12152 as an invalid server response, not as
proof of a malformed outgoing XML-RPC document. R5 does not claim a
live login fix, force different TLS protocols, spoof the SL viewer
identity, bypass authentication, or retry real credentials.
All tests are local/CI using synthetic values.

## R6 seed CAPS and initial EventQueueGet transport

After a verified Second Life login and a simulator UDP movement confirmation,
KAMI now initiates a **real HTTPS POST to the seed capability URL from
the authenticated login reply**. It requests `EventQueueGet` with an
LLSD XML array and validates the returned `EventQueueGet` bearer URL.
It then performs **one initial EventQueueGet long-poll request** with
`ack=<undef />` and `done=false`. An HTTP 200 response is accepted
only if its LLSD envelope includes `id` and `events` of the expected
types. Standard long-poll timeout/502/503/504 responses after at least
10 seconds are treated as *idle*, not as a verified event reply.

This is an important transport milestone, **not complete event handling**:
R6 deliberately does not ACK or dispatch event payloads, poll endlessly,
decode region object updates, render actual SL world meshes, or use the
simulator capabilities for inventory/assets. The one-shot poll may consume
an event batch that cannot yet be processed; subsequent R7 development
must implement a bounded event dispatcher and acknowledgement loop
before this can be considered a usable in-world session.

Security boundaries: URLs come only from the authenticated handoff and
the verified seed response; hosts must be in Linden Lab's trusted domains,
HTTPS certificate checks stay on, HTTP redirects are disabled, responses
are size limited and parsed using MSXML with external XML entities and
DTDs prohibited. Never print capability URLs, raw event data, account
details, passwords or session identifiers.

The HOME panel will now show `CAPS: requesting region seed` followed
by a seed or EventQueueGet diagnostic instead of a permanent hard-coded
`CAPS/EVENT QUEUE PENDING` string. Offline tests exercise URL validation,
LLSD seed structure and event envelope structure; live server success is
**not** guaranteed.

## R7 — Verified region metadata and safe EventQueue continuation

R7 starts from the R6 build that received a real
`AgentMovementComplete` and validated an `EventQueueGet` response.

- The authenticated, connected simulator's RegionHandshake packet is
  parsed for a bounded, printable region name and finite water height.
  A verified region label is displayed on HOME. This is actual UDP
  metadata, **not** terrain/objects/avatar geometry.
- The EventQueueGet capability URL is kept only for the active
  session rather than discarded after the first poll.
- Empty LLSD event responses are parsed and the bounded, typed server
  `id` is sent as `ack` on the next poll. Only one async request is
  outstanding at a time, and the UI remains responsive.
- A recognized idle long-poll is retried with a small delay. Real
  nonempty event arrays are **not acknowledged or silently discarded**:
  without complete event decoders/handlers they pause automatic polling
  and the HOME screen explicitly states that dispatch is pending.
- Invalid LLSD shapes, unsafe ids, or HTTP errors pause the poller
  rather than sending malformed acknowledgements or retrying
  indefinitely. Signed CAPS URLs, event bodies and credentials are
  never printed.
- Offline tests cover malformed names/water heights, simulator UDP
  loopback, cursor serialization and event-envelope rejection.

**Not yet implemented:** decompression of simulator LayerData terrain
patches, ObjectUpdate/ImprovedTerseObjectUpdate decoding, mesh/texture
asset pipelines, true region DX12 geometry, remote avatar inventory,
simulator crossings and complete event dispatch. KAMI still renders
its standalone offline scene until those paths exist and are verified.
CI tests cannot confirm any live region data beyond the tester's
actual Second Life connection.

## R8: live simulator event types and bounded world-wire intake

R7 established successful Second Life simulator movement and a valid
EventQueueGet response. A separate-viewer observation also showed the
logged-in agent present in-world. **That verifies server presence, not
KAMI's ability to render its appearance or the region.**

R8 extends the authenticated simulator connection with:

- Sanitized `EventQueueGet` message type names (maximum eight names of
  48 printable identifier characters). The raw LLSD event bodies are
  never displayed or logged. Unknown/unhandled events are deliberately
  **not acknowledged**, as R8 has no safe state-mutating dispatcher.
- Bounded message-template framing for `LayerData`,
  `ObjectUpdate` (header only), `ObjectUpdateCompressed`,
  `ObjectUpdateCached`, `ImprovedTerseObjectUpdate` and
  `KillObject`, using the official public Linden Lab message template.
- Actual UDP packet counts for LayerData and object-related messages
  displayed on KAMI's Home screen. Counters reset on disconnect and
  never count the offline scene.
- Tests for valid/truncated or oversized payloads, typed LLSD event
  names, and the simulator handshake regression.

**Important:** a valid packet frame is not a decoded terrain patch,
world object, material, mesh or avatar. ObjectUpdate High 12 is only
header-recognized. The GPU scene remains the original offline DX12
preview; these protocol counters must not be represented as rendered
Second Life content.

Source for the message IDs: Linden Lab's published
`master-message-template/message_template.msg` (High 11-16).
