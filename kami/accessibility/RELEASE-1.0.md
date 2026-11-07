# KAMI Viewer 1.0 — Native Accessibility Center

Status: **Release scope approved; implementation not yet started.**
Branch is intentionally isolated from beta and native-DX12 integration.
This is a release acceptance contract, not a claim that these toggles work.

## Location and UX

- KAMI Control Deck → **Accessibility**: dedicated, searchable native tab.
- Quick link in Kami Home and accessible from the login screen.
- Visual design follows Fire Garden without sacrificing contrast or focus.
- No external scripts, remote plugins, mandatory accounts or Windows-only
  accessibility installers. Keep existing user preferences and the SL session.
- Controls have accessible labels, descriptions and keyboard-focus feedback.
  A default Reset to Accessible Defaults option restores known-safe values.
- Settings are local to the device/account as appropriate, persisted, and
  excluded from remote performance-preset overrides. Changes which affect
  only UI themes should be previewable and reversible.

## 1. Vision and readability (release blocker)

- UI size presets including large/extra-large; prevent clipping of tabs,
  inventory, login, dialogs and creator panels on Windows scaling >100%.
- Separate chat/IM text size, with a working preview and high-legibility font
  option using redistributable system fonts. Respect font licenses.
- Native high-contrast UI theme that does not alter scene/world lighting,
  PBR materials or creator screenshots. Focus, hover and disabled controls
  must remain discernible without relying on color alone.
- Color-vision-friendly UI accent palettes with redundant symbols/text.
  Never silently recolor in-world art or avatar/asset textures.
- Optional larger cursor and stronger keyboard-focus outlines.
- Clear warning colors plus symbols and readable patch notices.

## 2. Motion, effects and photosensitivity (release blocker)

- Reduce UI transitions/animations, menu motion and nonessential screen shake.
- Toggle off motion blur, animated decorative effects and optional flare/glare
  where safe and effective. Never override a user-selected cinematic preset
  without explicit consent.
- Limit viewer-generated flashes and flashing buttons.
- Explain honestly that user-generated world lighting, textures, scripted
  effects and third-party ReShade/GShade filters cannot be universally
  prevented by a viewer preference.

## 3. Input and interaction (release blocker)

- Complete keyboard access to login, Control Deck, dialogs, inventory,
  Hotfix Home and the Accessibility Center; visible focus indicator.
- Remappable camera/navigation keys, hold/toggle alternatives for actions
  that require sustained pressing, and an optional reduced double-click
  dependence mode. Preserve all established Second Life keybindings.
- Larger click targets, configurable double-click timing if the existing
  input layer supports it, and reversible defaults.
- Tooltips and errors remain visible long enough to read; no important
  action requires a timed response.

## 4. Hearing, alerts and comprehension

- Visual equivalents for KAMI notification sounds and distinctive,
  non-color-only icons for chat/call/teleport/hotfix messages.
- Optional notification duration, readable history and manual dismiss.
- Clear, simple text descriptions in Update Required and progress UI.
- Live captions for other residents' voice are **not** assumed to exist;
  defer until there's a privacy-reviewed, consent-driven, tested transcript
  service with meaningful accuracy and user controls.

## 5. Compatibility and safety

- Do not reduce the in-world FPS just by enabling high contrast or large
  text. Benchmark UI/graphics changes independently.
- Do not interfere with grid permissions, security settings, RLV/experience
  restrictions, privacy, or SL simulator-enforced asset/upload limits.
- Accessibility presets must not change an existing logged-in avatar,
  movement state, teleport settings or world rendering unexpectedly.
- Never introduce silent remote transcription of live Second Life voice.
- Do not block updates with keyboard-trapping modals. In-viewer hotfix
  notifications respect notification duration, focus and sound preferences.
- Where OS-level accessibility features already work (Windows Magnifier,
  Narrator, contrast themes), test their interaction instead of claiming
  native screen-reader support without accessibility APIs and QA.

## Confirmed foundations in upstream KAMI source (not new features)

The current viewer source contains settings for:
- `UIScaleFactor` (graphics preferences already advertises some UI size
  changes may need a restart or display mode switch)
- `FontScreenDPI`, `ChatFontSize`
- `EnableButtonFlashing`, `RenderMotionBlur`
- Existing Keybindings/Controls and UI Colors/Notifications panels.

The release implementation should reuse these native settings where
appropriate and add new settings only where behavior is actually wired.
Do not add nonfunctional UI controls or claim a feature based on XML alone.

## Required acceptance tests before 1.0

1. Windows 11 scaling 100%, 150%, 200%; 1080p, 1440p, and 4K.
2. Keyboard-only login, in-world navigation, inventory, creator workflows,
   Preferences, Hotfix Center and Update Required/Return to World flows.
3. Accessibility ON/OFF with Fire Garden and other supported UI themes.
4. High-contrast text legible over dialogs, logs, chat, inventory and alerts.
5. Screen-reader and OS magnifier compatibility tested and limitations noted;
   do not label as fully screen-reader accessible without functional tests.
6. Rapid flashing/UI animation test; check motion blur and glow settings
   throughout the UI, including experimental DX12 scene preview.
7. No accessibility setting is wiped out by performance auto-adjust or an
   optimization hotfix; no forced logout or restart for live UI choices
   unless the existing setting fundamentally cannot be reloaded.
8. Manual recovery: accessible controls remain available even when graphics
   presets or renderer initialization fail.
9. Test with volunteer users needing larger text, reduced motion and
   keyboard access; use actionable feedback and repeat regression checks.

## Release sequencing

- **Beta**: keep testing the existing viewer and collect accessibility
  issues; no partially functional Accessibility Center in production.
- **Pre-1.0**: implement the native panel, underlying settings and full QA.
- **1.0 release gate**: ship the tested Accessibility Center along with the
  mature Hotfix System, not as a cosmetic placeholder.
