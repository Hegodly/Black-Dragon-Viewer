# KAMI Discover — full-release feature (deferred)

**Status:** Planned for public release. **Not in current alpha/beta/testing or renderer-validation builds.**  
**Product decision:** KAMI Viewer remains free for residents; KAMI Discover is a dedicated native discovery tab for Second Life creators. Optional paid *creator* promotions may help fund development.

## Full-release requirements

- A dedicated **Discover** tab in the KAMI Fire Garden UI, separate from gameplay and creator tooling.
- Browse featured and upcoming SL creators by category (fashion, hair, jewelry, skins, furniture, etc.), search for creator/store names, and view new releases.
- Link to SL Marketplace listings and permitted in-world store locations using safe, existing viewer handling.
- Manually reviewed creator submissions; distinguish organic discovery from optional sponsored placements with an unmistakable **Sponsored** label.
- Support resident favorites only when safe and privacy-conscious.
- Lazy-load and cache lightweight static listing artwork only while the tab is open. No autoplay video, no overlays during gameplay, and no background polling while closed.
- No mandatory separate account to use the viewer and no paywall for normal features.
- Review applicable Linden Lab Third-Party Viewer policies, advertising disclosures, privacy and content moderation rules before launch.

## Current testing-phase guardrail

**Do not implement or show Discover during the current testing phase.** No advertising SDK, background services, tracking, UI tab, HTTP requests, or shader/resource costs should be added for Discover yet. Prioritize Windows startup stability, true 1440p world rendering verification, frame pacing, creator studio, asset loading, and existing graphics controls. Do not let Discover affect ongoing rendering or performance investigations.

## Release acceptance

- Navigable tab, meaningful moderation, safe SL links, fully working and clearly disclosed sponsored placements.
- No measurable FPS or GPU/CPU overhead while the tab is closed; resources released when closed.
- Privacy review, disclosure review, security tests and UX/accessibility review.
- Tested Windows release candidate with Discover enabled; the viewer itself stays free.

**Implementation is intentionally postponed until full-release integration.** This file records scope only and changes no test-build runtime behavior.
