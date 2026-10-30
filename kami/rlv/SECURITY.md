# KAMI RLV: wearer-controlled security boundary

**Status (2026-10-30): Safety foundation only.** Black Dragon's snapshot contains RLVa XML files but no C++ command processor. This viewer cannot truthfully advertise functioning RLVa. The legacy RestrainedLove setting does not install it.

## Required safeguards before activating any future RLVa engine

1. Default OFF on startup. Never activate from object, relay, script, estate, landowner, experience, or imported settings. Wearer must opt in through native UI.
2. Gateway MUST inspect and authorize every incoming RLV command before dispatch, including owner-say, reattachments, shared folders, scripted relays, experiences, IM commands, and replayed/queued messages. Enable-switch alone is NEVER consent.
3. Approval must be scoped to source object UUID, verified owner UUID, command category and current wearer session. A relay needs TWO approvals (origin + relay). No grants based only on display name or creator. Expire consent after no more than 15 minutes by default and on detach, ownership change, logout, or emergency release. No persistent automatic grants.
4. Deny sensitive categories by default: forced teleport/sitting/movement, inventory/wear/remove/detach, chat/IM/privacy, camera/view control, environment, location tracking, and all experimental/unknown commands. Reject malformed batches and unknown commands entirely.
5. Wearer can revoke any grant anytime. Display actual accepted/denied commands and verified origin; keep an optional private local audit log. No hidden remote reporting.
6. The always-accessible EMERGENCY RELEASE in KAMI top menu (Ctrl+Alt+Shift+R) and Preferences may NEVER be blocked by restrictions, fullscreen, object script, RLVa interface, or camera mode. On a future real engine, panic must synchronously clear its active restrictions, consent grants, and queued commands before claiming success. Quitting, safety and abuse reporting must always work.
7. Rate-limit incoming commands, bound queues, and deny auto-accept from landowners or remote relays.
8. No claims of abuse-proof protection: external scripts and people can still behave harmfully; this gateway is for viewer-local enforcement.

## Current source state

- The core settings.xml does not include legacy settings_rlva.xml; that file's permissive defaults are hardened anyway.
- The KAMI header deliberately rejects ALL RLV commands, because no working processor or reviewed consent integration exists.
- Startup forces legacy RLV OFF and keeps the emergency block ON.
- Emergency action revokes future allowances and disables the legacy switch. It does not pretend to clear a nonexistent RLVa command handler.
- CI rejects unreviewed rlv*.cpp/.h processor files and validates the emergency path and native UI.
- The legacy RLVa Preferences panel remains hidden. The newly visible RLV Safety tab cannot activate RLV.

## Required integration tests

- Unapproved or relayed commands: DENY.
- Spoofed/changed/expired ownership, revoked grant, old session: DENY.
- Mixed dangerous commands in a batch, malformed syntax, oversized messages: DENY.
- Camera lock, chat blocking, forced sit/teleport, forced detachment: DENY until specifically approved.
- Press emergency menu or keyboard shortcut during restrictions: remove all real locks, pending commands and grants, block further RLV requests, leave quit and support controls accessible.
- Verify real live grid behavior with representative RLV devices after compiling and integrating a vetted, license-compatible implementation. Do not assert compatibility before testing.

## Compatibility tradeoff

Safety mode deliberately rejects some conventions expected by aggressive collars and relays. A compatible RLVa parser can be ported separately after a security review, but simply exposing outdated XML controls is never an acceptable substitute.
