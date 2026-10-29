# KAMI Viewer — Beyond Reality

KAMI Viewer is a custom Second Life viewer based on [Black Dragon Viewer](https://github.com/NiranV/Black-Dragon-Viewer), focusing on advanced graphics controls, user-selectable visual effects and smooth performance. The application branding uses the approved minimalist silver **神 / KAMI** emblem with a subtle bronze halo on black.

## Brand assets

The approved original artwork and derivative Windows icons, login banner, splash screen and installer icon are distributed in **KAMI Viewer Official Branding Kit**. Keep that artwork intact and use the supplied logo rather than recreating a different typeface. To apply artwork to a checkout, extract the kit's `KAMI_Branding` directory into `kami/assets`, then run `python kami/apply_branding.py`. Source-based channel, executable, product metadata and splash-dialog text are already renamed in this branch. Until the asset files are placed in this repository, GitHub Actions cannot package the approved graphical logo automatically.

## Development

Graphics integration and Windows builds remain experimental. The source tree contains optional NVIDIA/AMD/Intel upscaler work, subject to hardware support, SDK availability and validation; settings are not a guarantee of a particular FPS. See the Windows CI workflow and existing draft pull requests for build status.

## Attribution and licensing

KAMI is a downstream fork, **not** an official Linden Lab or Black Dragon release. Retain the original Black Dragon and Second Life contributor acknowledgments, license notices, copyright notices, trademarks and applicable third-party notices. See `LICENSE`, `doc/`, and upstream license documentation.
