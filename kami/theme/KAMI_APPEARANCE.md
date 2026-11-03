# KAMI native appearance modes — first implementation

Build branch: `feature/kami-imessage-messenger`.

The Windows build runs `kami/theme/apply-appearance-mode.py` after all other
interface generators and before final packaging. Users do not run a script.

**Dark mode:** retains the existing original KAMI palette. **Light mode:**
switches a defined subset of native color tokens to a cool silver/frost palette,
with dark, high-contrast foregrounds. A saved checkbox and a panel-opacity slider are under the native
KAMI WORKSPACES menu. The slider changes native color alpha, not backdrop blur. Switching mode performs no new 3D render passes or
per-frame UI calculations.

This is a **native appearance foundation**, not a completed iOS clone:
the palette uses no Apple fonts, symbols, assets or brand marks. There is no
true backdrop sampling or GPU blur. Some floater art and XUI labels still use
hard-coded colors and require screenshots and contrast/accessibility QA.
Source licensing and original viewer attribution remain in place.

Test both modes in-world, check People, Inventory, Search, Conversations,
Preferences, Workspaces and the Login UI at 100%, 125% and 150% scaling.
Verify changing the mode persists after restart, text remains legible, and
FPS/p95 frame time stays unchanged. The Windows CI cannot substitute for
manual display and GPU tests.

## Known limitation

User-defined colors in `colors.xml` take precedence over KAMI palette
tokens; this intentionally avoids overwriting the user's custom colors.
A manual Advanced → Reload Color Settings may require toggling the
appearance setting off and on to reapply the selected palette.

Full Liquid Glass-style panel redesign and adjustable backdrop blur
and the production DX12-only Second Life renderer are separate unfinished
milestones.
