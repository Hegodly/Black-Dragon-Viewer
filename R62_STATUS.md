# KAMI R62 — 4,080 Quality primitive shapes

Based on the R61 material-envelope and texture/mesh/PBR streaming build.

Quality now requests up to **4,080 nearby verified root/linked simple primitive shapes** (box, cylinder, sphere and prism), selected closest first. The primitive builder's hard ceiling also rises from 2,048 to 4,080. Other presets remain at Balanced 512 / Performance 256 / Potato 128.

The primitive geometry builder now receives the live scene's remaining vertex capacity *after* terrain, water and proxy geometry. It defers eligible shapes rather than overrunning the existing four-million-vertex combined DX12 upload limit. The caller reserves 1,000 vertices for local avatar geometry. Worst-case sphere-heavy regions may therefore draw fewer than 4,080 shapes; such shapes count as budget-deferred, not rendered. Vertex vector preallocation scales for mixed low-poly shapes rather than reserving 1,200 vertices for every allowed prim.

The mesh budget remains 1,000,000 triangles in Quality, subject to shared scene vertex capacity. The R61 material-format compatibility (binary/XML/notation LLSD), live glTF PBR and environment lighting, texture streaming and 512 GPU texture slices are unchanged.

Regression coverage: synthetic 4,200 verified nearby box roots must render 4,080, defer 120 and preserve the 512-shape Balanced limit. A sphere-dense synthetic scene must honor its explicit remaining-vertex budget without failing geometry construction. Windows CI compiles original KAMI source, runs the full unit suite and the existing mandatory forced-WARP DX12 pixel checks; those checks do not substitute for measuring 4,080 complex objects in the user's live Second Life region.

Remaining viewer limitations are unchanged: many real SL prim variants/sculpts, certain avatar systems and full EEP probe behavior are not implemented. This is a rendering-capacity experiment, not a promise that every nearby object is now supported.
