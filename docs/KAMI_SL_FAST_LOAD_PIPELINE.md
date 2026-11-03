# KAMI — Fast region loading architecture (offline foundation)

The intent is to reduce the time from successful Second Life login to the
**first usable in-world frame** while letting nearby avatars, texture
resolution, and distant scene details converge gradually.

## Implemented and tested in this branch

- Original C++20 `protocol::load::Scheduler` with **visible-first priorities**.
- Distinct own-avatar, visible geometry/texture, animation, attachment,
  nearby geometry, and distant texture priority classes.
- A UUID-based request deduplication layer; repeated requests for an asset
  reference do not generate duplicate queue entries.
- Priority upgrades when a previously distant object becomes visible.
- Fixed concurrency ceilings of 8 downloads and 4 decodes; these numbers
  are conservative **defaults for future adapters**, not advice to create
  parallel unbounded HTTP traffic.
- Explicit `Queued -> Downloading -> DecodeQueued -> Decoding ->
  ReadyForGPU -> Resident` transitions, capped asset sizes, and a
  default **4 MiB per frame GPU upload budget**.
- Session generation/reset behavior: disconnect must invalidate the
  outstanding asset queue, not process stale simulator data.
- No background network threads, texture decoding, GPU upload callbacks,
  or persistent on-disk cache are connected yet.

## Next steps required for actual fast loading

1. Finish authenticated SL login, UDP simulator circuit, capabilities and
   event queue; do not rely on the HTTP HEAD probe as evidence of login.
2. Decode official SL mesh and texture asset formats and use UUID references
   from verified world/appearance messages to feed the scheduler.
3. Cache *verified* assets on SSD under a quota, using content hashing,
   atomic writes, an LRU eviction policy, and account/security boundaries.
   Honor SL permissions, and never add an export bypass.
4. Fetch low-discard/resolution textures first, then progressively load
   better versions. Defer invisible and far-away objects.
5. Decode texture/mesh data on bounded worker threads and stage GPU buffers
   on a frame-time budget. Keep the UI and camera fully responsive.
6. Instrument: TLS/auth time, simulator handshake, region identity,
   first usable frame, own-avatar appearance, 90%-visible scene readiness,
   full-resolution texture completion, cache hit rate, packet loss,
   download queue depth, decoder backlog, and p50/p95 frame time.
7. Benchmark busy and quiet Second Life regions repeatedly under matching
   network conditions against the official viewer and other third-party
   viewers. Prefer real measurements over a promised percentage increase.

## Privacy, networking, and performance

- Don't cache credentials, tokens, or signed capability URLs. Keep
  authentication material memory-only, and restrict download hosts to
  capabilities supplied through the authenticated simulator.
- Bounded worker/connection pools prevent flooding SL services; conform to
  Linden Lab's Third-Party Viewer Policy and avoid unreasonably high
  request rates.
- Cache misses, simulator delays, Wi-Fi packet loss, server-side avatar
  complexity, and rate limits cannot be eliminated by client code.
- The user's measured **200 FPS in the offline test scene** is a useful
  baseline, not a guarantee for a populated Second Life region.

Source references: Second Life wiki Image System / HTTP Texture /
Current login protocols, and Linden Lab Third-Party Viewer Policy.
