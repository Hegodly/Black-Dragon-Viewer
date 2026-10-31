# KAMI Beyond Reality — Whole Viewer Experience & FPS Overhaul

**Scope:** full Windows viewer experience, not graphics-quality presets only. Maintain high-quality native 1440p PBR and optional genuine 4K/8K, while reducing wasted CPU/GPU work, I/O stalls, UI latency, and frame-time spikes. Keep **Fire Garden** as the theme. Work in a feature branch; do not replace or ship existing stable artifacts without testing.

## Architecture and ownership of each subsystem

| System | Existing upstream files / KAMI integration | Planned performance-and-polish upgrade |
|---|---|---|
| Startup, login and teleport | `indra/newview/llstartup.cpp`, `kami/loading/apply-fast-loading.py` | Record milestones for application start, network auth, world handshake, first avatar frame and asset readiness. Remove proven redundant blocking steps. Do not mistake shorter loading overlays for faster asset loading. |
| Menus and windows | `indra/llui/llfloater.cpp`, `indra/llui/llview.cpp`, `kami/theme/apply-fire-garden-ui.py` | Design motion system for focus, hover, open/close and tab transitions; drive from time rather than framerate, retain text clarity/input hit targets; avoid new fullscreen passes or per-frame PNG allocations. |
| Viewer sounds | `indra/newview/lldeferredsounds.cpp`, sound-engine controls and theme files | Preserve correct sound delivery; prevent sound callbacks from recursively extending a flush; ensure volume/mute/accessibility and optional SFX are independent from resident/media/world audio. No undisclosed changes to audio assets. |
| Avatar animation playback | `indra/newview/bdanimator.cpp`, avatar skeleton/motion update paths | Profile pose blending, motion update scheduling and skinning; prevent extra updates for unchanged poses while preserving avatar joint transforms and imported keyframes. Never substitute a lower-fidelity animation or decrease animation rate to claim FPS. |
| Rendering | `indra/newview/pipeline.cpp`, `kami/renderer/*` | Reduce redundant driver state and resource allocations only where equivalence is measurable; retain native 1440p, 4K/8K visible supersampling and separate UI presentation. |
| Texture/mesh readiness | viewer asset cache and image decode/mesh upload pipeline | Time cache misses, decode, upload and main-thread waits. Keep texture sizes, streaming priority, material quality and object visibility identical. |
| Frame pacing | existing viewer frame stats and quality-locked benchmark | Measure real CPU/GPU frame times and stutter percentiles. Preserve settings and test scene for matched A/B runs. |

## User-facing interaction principles

- **Fluid UI motion:** 140–220 ms reference duration, time-based easing, never tied to FPS. Animations must not prevent button clicks, ESC/cancel actions, keyboard navigation or loading progress.
- **Reduced motion:** global switch disables nonessential UI transitions, camera embellishment, flashing effects and motion-based notifications, without affecting avatar/scene animations. Respect the Windows accessibility preference where technically available.
- **Audio controls:** interface sounds distinct from the world/voice/media buses; mute and volume remember choices. Avoid a click sound for every hover. No overlapping notification storms.
- **Loading honesty:** first usable frame, character complete, textures complete are separate milestones. Show a genuine loading state and failures; prioritize visible assets, but never deliberately omit them or change visual quality.
- **Animation quality:** protect animation timeline, motion priority, blend weights and uploaded data. No deceptive FPS doubling through interpolation without clear labeling; aim for smooth real rendered frames.
- **Performance:** a low-end fallback may be offered explicitly, but never treat quality reduction as an improvement under the quality-locked benchmark.

## Implementation gates (incremental, not a big-bang rewrite)

**Gate 1 — Baseline and safety (started).** Collect per-scene timing and quality-locked frame metrics; audit existing startup and UI loops; preserve all existing user settings. Fix safe queue or lifecycle bugs with unit/static checks.

**Gate 2 — Loading and audio.** Add reliable timeline and cache/scene loading telemetry; apply validated prefetch/cache fixes with measurable real load-time savings; make sounds robust during startup/teleport/notification bursts, maintaining audio settings.

**Gate 3 — Interface motion.** Reusable, opt-in GPU-cheap tween/spring controller driven by real time. Integrate Fire Garden window/tab/hover transitions with accessibility support and animation cancellation. Validate 30/60/120 Hz and focus behavior.

**Gate 4 — Avatar and scene animation.** Motion update instrumentation, no repeated blending or skeleton updates when state truly unchanged, stable time-based interpolation for visible motion without editing source keyframes, correct animations across FPS variance, pelvis/facial/joint/attachment consistency.

**Gate 5 — Core render & streaming.** Profile draw submission, shader binds, image decode, geometry updates and FBO lifecycles; optimize CPU/GPU hotspots preserving pixels. Architecture changes like indirect draw are capability-gated with rollback.

**Gate 6 — Quality-locked final verification.** Full Windows build, startup/login/teleport tests, identical-scene visual screenshots, animation footage, UI interaction latency, audio burst/drop tests, median/p95/p99 frame timings, 1% lows, VRAM stability, lower-end fallback and RTX 5070 baseline.

## No false promises

KAMI is an OpenGL-based Second Life viewer. Maintaining a particular FPS everywhere regardless of scene/avatar complexity is not physically guaranteed. A movie-quality interface transition does not justify an extra 8K framebuffer or heavier per-frame compositor. More costly GPU work may increase FPS variance, so fidelity and speed must be tested independently.

The user-supplied login MP3 installer is **local user-only**. This source branch must not bundle or redistribute the third-party soundtrack without explicit redistribution rights. No proprietary media is copied into this repository.

## Development status at creation

- Renderer improvements staged separately in `feature/kami-visible-qhd-4k-8k`.
- Quality-locked benchmark, regression tests and CI staged in `feature/kami-quality-locked-performance`.
- This roadmap records the *expanded complete-viewer request*; it does not claim all subsystems have been implemented or benchmarked.
