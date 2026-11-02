# Live outgoing-bubble status

The most recent live outgoing personal IM bubble shows a right-aligned label immediately below its surface. Sent means handed to KAMI's existing IM transport, not server delivery. Delivered requires a matching receipt from the receiving opted-in KAMI client. Read uses the existing foreground, focus, viewport, muted-user and DND guards; it does not prove human attention.

Statuses update from the in-memory protocol revision during normal UI drawing. There are no network polls, services, or per-message timers. A new outgoing message supersedes the previous label. History bubbles do not acquire delivery claims. Unknown/unsupported viewers and untagged overlength messages stay at Sent.

Delivery capability is advertised by a canonical HELLO nonce with the reserved d311b002 prefix and 96 remaining random bits. A compatible receiver sends its own capability HELLO once per session. Existing RR1 clients still understand HELLO/HELLO_ACK and can use SEEN, but are never sent DELIVERED unless they advertise the new capability. Duplicate delivery acknowledgments do not advance state or cause loops. The acknowledged peer, latest outgoing message ID and conversation must match. Read takes precedence if delivery packets arrive later.

Both residents must opt in; use Connect Kami receipts as before. First-contact HELLO remains an explicit in-band message and may be visible in unsupported viewers. This change does not solve that existing protocol limitation.

Tests compile the actual generated protocol against transport/settings stubs and exercise two-client negotiation, Sent/Delivered/Read, old-client fallback, wrong peer/session/ID rejection, late acknowledgments, duplicate packets, overlength text and opt-out reset. Native bubble rendering still needs Windows compilation and in-world testing.

The Windows build produces the complete viewer plus a KAMI-Windows-Hotfix artifact. Its PowerShell installer accepts the known 55feac4 executable, verifies all payload checksums, backs up overwritten files, and restores originals if installation fails. For other base versions, use the complete viewer build. Do not run the installer while KAMI is open. The installer runs locally without administrator privileges and does not download files or change system settings.
