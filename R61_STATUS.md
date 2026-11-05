# KAMI R61: material envelope compatibility

The live R60 screenshot shows HTTP 200 with a 543-byte response rejected before glTF parsing. That proves fetching succeeded but does not identify the container. R61 extends material decoding to binary, XML and notation LLSD, matching the format dispatch used by the official viewer. Supports UTF-8 BOMs, XML entities and CDATA, and notation length-delimited strings. TinyXML2 10.0.0 handles XML without fetching external resources; DTDs and duplicate/wrongly typed map fields are rejected.

WinHTTP asset requests now enable gzip/deflate Content-Encoding decompression before the bounded body read. Embedded compressed asset blobs are detected and reported, not guessed at. All decoded response and JSON size limits, UUID-only image references, type/version checks and transactional material decoding remain enforced.

F9 reports the detected response format and specific failure stage (header, binary map/key/value/trailer, XML map, notation map, version/type/glTF). These expose no asset contents or authenticated URLs. The Home footer reports actual mesh/primitive draw counts instead of the stale NOT RENDERED wording. R61 appears in the sidebar.

Tests cover binary/XML/notation equivalence, malformed and truncated envelopes, XML entity/CDATA handling, DTD and duplicate rejection, raw-string length checks, unchanged output on failure, and production GPU pixel equivalence for material formats. Live material loading still requires confirmation from F9 assets/faces; the user's 543-byte response has not been captured or replayed.

Maximum-world option: Quality raises mesh draw allowance from 500k to 1M triangles and primitive allowance from 1024 to 2048. The overall scene vertex ceiling is 4M (288 MB of vertex data); CPU mesh cache ceilings rise to 4096 unique assets / 4M decoded triangles (256 MB of triangle data). Quality reduces minimum mesh/texture/material dispatch intervals from 350/250/250 ms to 50/35/50 ms. Each asset kind still has one asynchronous worker and existing bounded HTTP retries/backoff; these are dispatch ceilings, not guaranteed download throughput. Other presets keep their existing draw budgets and dispatch intervals. GPU texture residency remains 512 slices and all texture memory limits remain intact.

R59 PBR lighting remains included. Known limitations: one active probe, native KAMI sky rather than EEP, no multi-probe blending or planar mirrors, incomplete real-avatar and other viewer systems.
