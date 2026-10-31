# KAMI Viewer — hardware-aware true 1440p setup

Status: **experimental build integration**. This is not a benchmark or a promise of FPS.

## Purpose

The OpenGL renderer is queried **once before allocating the main 3D GPU framebuffer**. KAMI checks the GPU that is actually rendering, rather than relying on the CPU model or an unrelated graphics adapter. No information is sent to a server.

On a new installation with an untouched resolution preference:

| Active GPU example | Starting world framebuffer |
| --- | --- |
| NVIDIA GeForce RTX 5070 | True 1440p: **2560 x 1440** at 16:9 |
| RTX 5070/5080/5090, RTX 4070+ and RTX 3070+ | True 1440p |
| Selected Radeon RX 6800+, 7700+, 7800+, 7900+, 9070 | True 1440p |
| Intel UHD, Iris Xe, basic Radeon integrated graphics | **Window native**, no forced QHD |
| RTX 3050, unrecognized GPUs, software GL devices | **Window native**, no forced QHD |

These are conservative *model-name rules*, not automatic performance profiling. A high-end GPU can still be CPU-limited by avatar counts, region complexity, and asset loads. A low-end GPU at a high-resolution desktop may still require a lower graphics preset.

## Resident controls and safety

- The existing Display resolution menu continues to offer **Window native / True 1440p / 4K / 8K**, with no forced FPS cap.
- On a fresh profile, hardware detection applies an initial recommendation **only if the world resolution has not been customized**.
- Manual resolution choices override the recommendation. When the active GPU changes, automatic re-evaluation occurs only if the previously auto-chosen value is still selected.
- No repeated GPU polling, graphics benchmarking, new online services, dynamic world scaling, camera crop, or telemetry.
- 1440p means 1440 **real** source-rendered rows; the GPU allocates the framebuffer, not a post-process upscale.
- A failed GPU framebuffer allocation falls back to window native with a visible reason. GPU classification alone does not prove framebuffer success.
- Status is stored locally in the debug setting `KAMIHardwareDetectionStatus` and the world-buffer status display.

## Engineering verification

Pre-patch: `python kami/renderer/verify-gpu-detection.py`

Post-patch: `python kami/renderer/add-gpu-detection.py`, then `python kami/renderer/verify-gpu-detection.py --integrated`

**Windows test matrix:** RTX 5070 first-run with actual 2560x1440 GPU buffer; 1080p and 1440p monitors; Intel and AMD integrated machines; GPU switch (e.g. laptop hybrid graphics); manual 0, 2560, 3840, and 7680 saved settings; startup GL unavailable; FBO allocation failure; FPS and frame-time readings in crowded SL regions; UI, HUD and camera stability. Test against existing profile settings without erasing them.

Do not merge or announce tested GPU/FPS compatibility before a clean Windows executable and real-hardware QA.