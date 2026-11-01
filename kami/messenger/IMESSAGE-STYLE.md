# Kami Messenger

Personal IMs use blue outgoing and dark incoming native chat bubbles. The
existing transcript, local history, URL handling, emoji, typing indicators and
unread counters remain in the SL messaging system. Compact chat, `/me`, group
chat, system messages and interactive offers use the existing renderer.

The read-status footer sits below the transcript, above the composer.
Read receipts default off per account. Both residents must turn on **Read
receipts**. Use **Connect Kami receipts** once in that conversation to request
a compatible peer. There is no automatic probing of arbitrary SL residents.

After a successful handshake, the newest tagged outgoing message can receive
the status **Open and read** and one **Kami Messenger** in-app notification
containing the recipient and **Open and read**. This confirms the peer's client
displayed the message, not human attention. Sent is not server-confirmed
delivery. Other viewers' read status remains unknown.

Receipts are sent only for a pending message in the active app, with the
conversation visible and focused and the transcript scrolled to the newest
message. Muted, DND and offline replay packets cannot initiate replies.
Disabling receipts or changing accounts clears pairing and pending state.
Overlength messages remain plaintext and cannot inherit an older read status.
No local chat-open diagnostic is shipped as a sender receipt.

Transport uses the existing opt-in KAMI-RR1 protocol. A non-Kami viewer may
display a manually requested compatibility invitation as protocol text. A
switch to another viewer after pairing may expose an in-flight metadata
suffix. This is not a server-level or universally invisible SL feature.

## Build integration

The Windows workflow applies `apply-read-receipts.py` followed by
`apply-imessage-style.py` before compiling. `native-bubble.inc` is inserted into
the existing chat-history compilation unit; no webview, timer, rendering pass,
or new library is required. Bubble layout runs on append/resize. Normal frames
perform a single pending-peer lookup before any receipt viewport checks.

`python kami/messenger/test-imessage-style.py` applies both integrations to an
isolated source fixture and compiles/runs the actual generated receipt header
with stub transport, settings and notifications. This covers the handshake,
permission gates, plaintext fallback, old/wrong/duplicate acknowledgements,
one-shot notification, long-text status reset, account/opt-out resets, and
legacy/modern P2P dialog preservation. XML and foreground/viewport hook checks
also run. This does not compile the full viewer or simulate the SL server.

## Live test checklist

- Use two Kami accounts: default off sends ordinary text with no receipt.
- Enable both sides, connect, send text with receiver's chat closed: no receipt.
- Open the conversation in active Kami and show the newest message: sender
  gets one notification and the footer says Open and read.
- Leave receiver minimized, in another app, on another chat, scrolled up, muted
  or in DND: no opening acknowledgement until the permitted visible action.
- Send several messages, then deliver old/duplicate acknowledgements: only the
  newest matching message controls the footer; duplicates cause no new toast.
- Send multiline text, emoji, URLs, long text, `/me` and an inventory offer.
  Resize narrow/wide, copy selections and search/reopen history.
- Check the compact-chat preference, group chat, typing and unread counters.
- Disable/re-enable receipts: a new connection is required; old read state
  must not reappear. Test a non-Kami peer and switching viewers after pairing.

Full Windows compilation, visual layout and the online two-account checklist
must pass before treating this feature as release-ready.
