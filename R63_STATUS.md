# KAMI R63 — dense-region bug fix and PBR priority

R63 is based on the successful R62 4,080-primitive DX12 build, with no rollback of the existing renderer, LLSD material formats, texture streaming or PBR environment pipeline.

Fix 1: The real simulator world-intake layer was limited to 2,048 distinct root objects even though R62 raised the DX12 primitive selection ceiling to 4,080. Raise authenticated root retention from 2,048 to 4,096. Linked-object retention stays 8,192; lower graphics presets still render their pre-existing smaller budgets. This is an intake cap, not a guarantee that every unsupported SL volume can be drawn.

Fix 2: The fixed-capacity GPU PBR material table was assembled by local object ID, potentially exhausting its 1,023 unique material slots on distant roots before nearby drawable objects. In live scenes, reserve terrain PBR first and sort root/linked materials by the viewer's authenticated root-anchor distance, with deterministic tie-breaks. Reject unresolved child hierarchies and reflection probes from this visible material table. Linked-object priority is based on its authenticated parent root position, not its exact rotated local offset, so it is an approximation. The GPU material record limit remains unchanged; overflow falls back to legacy face appearance rather than exceeding the upload buffer.

New tests: compile-time assertion that retained roots cover the 4,080 Quality primitive limit; synthetic material table overfill with 1,040 distinct roots confirms a late-ID near root and terrain still bind; an orphan linked prim does not consume PBR records. Existing 4,200-shape and sphere vertex-budget regressions from R62 remain included.

CI: original Windows x64 build, all ctest native tests, forced Microsoft WARP DX12 smoke (PBR, material formats, pixel output) and test ZIP packaging.

Live limitations: material-asset request scheduler still retains only 256 decoded material assets and a limited texture atlas; geometry for unsupported primitive profiles and full viewer/avatar behavior remain unfinished. A real decorated SL region and RTX 5070 are still needed for FPS/load-time observations.
