# KAMI R64: mesh-first world loading regression fix

R64 starts from the R63 dense-region and PBR material fixes. It preserves the 4,080 Quality primitive request limit, the 4,096 root / 8,192 linked authenticated object-retention limits, and the R63 nearest-object PBR ordering.

## Confirmed source-level defect

In R62 and R63, the live DX12 scene first allocated vertex space to supported procedural primitive geometry, then attempted to render downloaded Second Life mesh LODs using only the *remaining* space. With 4,080 eligible primitives (especially cylinders or spheres), primitive geometry can consume most or all of the 4,000,000-vertex upload budget and thereby hide real buildings and other mesh objects that are successfully downloaded and decoded. Passing the existing unit and forced-WARP pixel tests did not exercise that specific live-scene competition.

## R64 correction

Build and budget authenticated mesh LODs *first*, up to the existing Quality limit of 1,000,000 mesh triangles and the remaining shared DX12 scene capacity. Then draw closest-first supported primitive shapes from the leftover capacity, subject to the same 4,080-shape hard selection limit. Reserve 1,000 vertices for the existing avatar marker and preserve the 4,000,000 total vertex cap, land/water/proxies, original PBR face indices and texture mappings. This deliberately favors actually loaded mesh buildings over procedurally generated prim geometry when either must be deferred.

The bounded scene capacity is centralized in live_world_vertices_remaining(). R64 tests simulate limited remaining world capacity (29,000 vertices) with 1,000 triangles of genuine mesh geometry followed by 1,000 eligible boxes. Assert that the mesh remains fully drawable, the remaining primitive budget is respected, the deferred count is accurate, and the total allocation stays within the existing vertex cap.

## Validation and limitations

Windows CI compiles the independent native KAMI viewer, runs CTest and forced Microsoft WARP DX12 material/texture/shader pixel verification. These synthetic tests cannot confirm actual Second Life scene completeness or on-device frame rate; an in-world comparison of R63 and R64 near a decorated house is still required.

R64 does not add support for unsupported Second Life sculpt/cut/hollow/complex volume shapes, increase the 512-slice GPU texture atlas, or remove the 256 decoded material-asset scheduler bound. Loaded mesh priority may reduce the number of procedurally generated spheres or cylinders in extremely dense regions; this is intentional and reported as primitive deferred shapes.
