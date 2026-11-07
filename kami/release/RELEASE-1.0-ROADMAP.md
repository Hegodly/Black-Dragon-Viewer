# KAMI Viewer — Full Release 1.0 Feature Roadmap

**Status:** Planned scope, not a claim that items are implemented or beta-ready.
**Release strategy:** Complete current beta stability/performance testing first.
Freeze new features in beta; work on 1.0 systems in separate, reviewable branches.
This roadmap captures product decisions; individual features still require native
implementation, testing, performance checks, and a successful portable Windows build.

## 1. KAMI Cinematic Camera 2.0 — NEW (full-release feature)

**Decision:** Keep Black Dragon's existing cinematic head tracking and
realistic Mouselook, and improve them rather than replacing or removing them.

**Compatibility:** Retain the original Black Dragon camera behavior as
**Classic** so current camera users retain familiar motion and scripted
interactions. Preserve ordinary Second Life avatar movement, camera
permissions, scripted vehicle controls, Mouselook clicks, and input bindings.

**Enhanced controls:**
- Adjustable head-tracking strength and third-person head-follow offset.
- Adaptive camera stabilization that reduces animation jitter while
  preserving subtle natural head/body movement; per-state tuning for idle,
  walking, sitting, dancing, posing and camera/photo workflows.
- Independent roll intensity/clamp, separate from existing max roll settings,
  with a true zero-roll / reduced-motion option.
- Frame-rate-independent smoothing with bounded damping and predictable
  responsiveness at 30/60/120+ FPS; avoid mouse/input latency.
- Smooth, interruptible transitions between Mouselook, third person and
  over-the-shoulder views with saved/restorable camera presets.
- Camera-collision improvements that reduce wall/attachment clipping without
  breaking legitimate camera movement or parcel restrictions.
- Test with seated avatars, animesh, camera-follow joints, attachment-heavy
  heads, unusual animation rigs and build/photo mode.

**Native Camera Control Deck presets:**
1. **Classic** — preserves existing Black Dragon cinematic behavior.
2. **Cinematic** — natural head follow, moderate stabilization/roll.
3. **Precision** — direct response, minimal sway/roll; comfortable Mouselook.
4. **Director** — smooth filming, subject-follow options and stable framing.

Presets must be editable, reversible, individually persisted and compatible
with Accessibility Center's Reduce Motion. Never overwrite explicit camera
choices through auto graphics/performance optimization. Leave simulator-side
camera limits and resident permissions unchanged.

**Implementation source anchors (existing code):**
- `indra/newview/llagentcamera.cpp`: `mCinematicCamera`,
  `UseCinematicCamera`, bone-follow position and head-aligned camera roll;
  `mRealisticMouselook` and camera positioning.
- `indra/newview/skins/default/xui/en/panel_preferences_camera.xml`:
  existing cinematic head tracking, roll and Mouselook controls.
- `indra/newview/app_settings/settings_blackdragon.xml`:
  existing `UseCinematicCamera` and `UseRealisticMouselook` settings.
Preserve originals; implement new logic behind opt-in KAMI setting keys,
not by silently changing upstream legacy defaults.

**Acceptance tests:** user can switch Classic ↔ new presets at runtime
without losing a connected Second Life session; no extreme camera shake,
rolling, clipping regressions or unintended avatar-bone mutation; no severe
frame-time spikes; input latency measured; different FPS tested; accessibility
and camera/photo editing preserved.

## 2. KAMI Hotfix Home (full-release feature)

NBA 2K-style in-viewer, non-modal **Optimization Available** notification →
**Return to Kami Home** overlay while **remaining logged into Second Life** →
apply a verified, restart-free hotfix → **Return to World** with the avatar still
connected. Hotfixes ONLY contain explicitly hot-reloadable, signed and
validated optimizations. Feature releases can require a user-approved restart;
native EXE/DLL replacements never masquerade as live patches.
Includes history, safe rollback, channel selection and anti-spam controls.

Development reference: draft PR #35 / `feature/kami-hotfix-home`.
Prototype is not a working update service.

## 3. KAMI Accessibility Center (full-release feature)

Dedicated native tab for UI/chat scaling, high-contrast and color-vision-aware
themes, reduce motion/flashing, keyboard-focused navigation, easier input,
legible notifications, and optional comfort profiles. Respect existing
Windows accessibility support and Second Life privacy/security controls.
Do not invent screen-reader or voice transcription support without testing.

Development reference: draft PR #36 / `feature/kami-accessibility-1-0`.
Specification only; no completed accessibility panel yet.

## Release readiness requirements

- **Beta first:** functional portable installer/ZIP, in-world graphics,
  physics, texture load and camera regressions tested by multiple users.
- **1.0 features must be functional**, not placeholder UI or installer scripts.
- Windows signed binaries and package integrity, dependency/license audit,
  and compliance with Second Life third-party viewer requirements.
- Repeatable baseline profiles: crowded avatars, low/medium/high-end GPUs,
  camera controls at variable FPS, PBR and non-PBR paths.
- Safe, reversible defaults for cinematic/precision/reduced-motion profiles;
  accessibility settings override cosmetic animation preferences.
- No FPS claims without measured frame-time/VRAM results.
- Update service must not silently log out residents or apply native
  binaries to a running process.
