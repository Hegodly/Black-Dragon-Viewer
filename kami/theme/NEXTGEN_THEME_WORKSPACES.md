# KAMI Workspaces Theme 2.0 — Beyond Reality

**Status:** Native interface implementation committed; Windows build and visual interaction QA pending.

## User problem

The previous KAMI build was visibly Black Dragon with a Fire Garden skin. The buttons and palette had changed, but the underlying menu hierarchy, flat launcher pattern, narrow Preferences categories and default dock geometry were nearly identical. This project is a **product/navigation redesign**, not another coat of paint.

## What Theme 2.0 changes in the next Windows build

1. **WORKSPACES (new primary navigation):** a registered native KAMI tool hub. Four purpose-built entry areas — Explore, Avatar, Create and Control — contain real native windows: Inventory, Places, Map, Outfits, Poser, Pose Creator, PBR Materials, Snapshot, Camera, Preferences, Hardware Monitor and toolbar editor. Every button is wired to `LLFloaterReg::showInstance`; none are screenshots, mockups or web links.
2. **KAMI upper menu:** a new `WORKSPACES` menu provides direct access to tools, without removing any Second Life/Black Dragon menus, menu IDs, callbacks or keyboard shortcuts.
3. **Four-edge dock:** change native top/bottom/left/right toolbars into a more substantial Windows game-like control rail (40 px top, 54 px bottom, 42 px side bars). Existing toolbar drag/drop, commands, user layouts and window snap behavior must remain operational.
4. **KAMI CONTROL DECK:** rename and reorganize Preferences categories into numbered sections, including `GRAPHICS + FPS`, `AVATAR CONTROL`, `SOUND + MEDIA`, `INTERFACE` and `UPLOADS`. Underlying preferences panels, settings bindings, and searchable controls remain unchanged. Sidebar width and outer window grow together to prevent clipping the render settings.
5. **Native interaction hierarchy:** 32 px floater title bars, readable press/hover states and bigger button hit targets, all using Fire Garden's existing dark-metal textures and real native controls. No world framebuffer effects, FPS caps, photo filters or shader runtime added.
6. **Accessible credit:** the WORKSPACES footer credits the source viewer creator, NiranV Dean. The original LGPL and other source attributions remain intact in the source tree and About/Credits.

## Full theme vision (later stages, not falsely declared implemented)

- Full-window login/startup motion identity: Fire Garden with subtle ember/silver transitions that never block login or load.
- Unified motion design for native tab transitions, menus, workspace switching, notifications, flyouts and modals. Time-based, cancelable animation, reduced-motion override and no background render cost when idle.
- Original, licensed optional UI navigation sounds, separated from in-world/voice/media volume with explicit mute.
- Unified inventory/creator/editor spacing, iconography and focus behavior; real accessible controls, not visual mockups.
- Per-user workspace layout selection (Explorer / Creator / Cinematic) without changing the underlying SL world renderer.

## Acceptance checklist

**Build:** all Python source preflight, native C++ integration, XUI parse, Windows compilation and asset staging tests pass.

**Interaction:** every Workspaces button opens its corresponding window; every original menu entry remains; all keyboard shortcuts work; toolbar icons align, customize and persist; graphics Display controls scroll and work.

**Visual:** screenshots demonstrate a distinctly different navigation experience and grouping from Black Dragon, while retaining selected Fire Garden assets. No clipped labels at 100%, 125%, or 150% Windows scaling. UI still works at narrow window sizes.

**Performance:** menu opening/click latency and viewer p95/p99 frame times are no worse than the previous build in identical regions/settings. No new full-screen compositor or periodic background texture reupload.

**Accessibility:** readable high contrast, keyboard navigation, tooltips, no distracting motion by default, and support for reducing upcoming animations.

**Attribution:** Based on the open-source Black Dragon Viewer by NiranV Dean and the upstream Second Life viewer. KAMI does not imply endorsement or affiliation.

## How to test

1. Open KAMI from the upcoming Windows build, then open **WORKSPACES** in the upper menu.
2. Test `INVENTORY`, `POSER`, `MATERIALS`, `SNAPSHOT`, `PERFORMANCE` and `GRAPHICS / SETTINGS`.
3. Check top/bottom/side docks with drag and drop; open the Control Deck and scroll the Display settings. Take before/after screenshots.
4. Compare real FPS and frame-time behavior to the previous build with identical scene, camera and graphics settings. Do not claim FPS gains from theme changes without measurements.

The existing build remains a fallback. This theme branch is a draft until its Windows CI and in-world checks succeed.
