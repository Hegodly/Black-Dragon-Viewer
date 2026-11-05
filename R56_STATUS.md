# R56 texture coverage and recovery

R55's world-mesh decoder fixes are retained.

- GPU residency grows from 48 to 192 preview images, with stable array slots and incremental uploads.
- CPU previews now match the 256-pixel GPU extent instead of retaining 512-pixel images. Up to 384 images fit within the unchanged 96 MiB decoded pixel-data limit.
- The material array uses about 48.25 MiB instead of 12.25 MiB. Initial upload staging is bounded at 64 MiB; later updates copy only changed slots.
- The shader reads the array's actual layer count. Selection, upload validation and diagnostics share the same budgets.
- Texture demand excludes unloaded mesh geometry, faces without UVs and unused overridden defaults.
- Temporary texture connection failures, HTTP 429 and selected 5xx responses retry at most three times with backoff. Missing/corrupt assets do not retry endlessly. Obsolete completed requests are discarded after interest changes.
- Tests cover dense visible demand, the highest slot, stable assignments, incremental copies, full-array footprints and forced-WARP drawing using the last slice.

This is a partial live viewer. Terrain materials, real avatar appearance and SL skinning, teleport/pose synchronization, IM and inventory remain unfinished. More textured surfaces depend on actual asset availability; no FPS or live-world completeness claim is made.
