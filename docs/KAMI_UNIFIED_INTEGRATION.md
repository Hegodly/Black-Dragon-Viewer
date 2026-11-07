# KAMI Unified Viewer Integration Contract

The deliverable is the independent Windows `KAMIViewer.exe` with live Second Life functionality. The seven-in-one diagnostic program and DX12 smoke tool are test utilities, never release deliverables. The Windows candidate archive must contain exactly one executable: `KAMIViewer.exe`.

## Integration graph and acceptance gates

`docs/KAMI_FEATURE_ACCEPTANCE.json` is the source of truth for the feature dependency graph. The validator checks all required feature nodes, known dependencies, cycles, explicit statuses and acceptance criteria. Release stays disabled until every feature is verified, blockers are empty, and authenticated live SL acceptance is recorded.

| Stage | Depends on | Acceptance requirement | Current status |
| --- | --- | --- | --- |
| Native shell and workspaces | Original core | Original KAMI window, navigation, input and native UI | Partial / unverified; not live-accepted |
| Login and simulator transport | Native shell | Authenticated region handshake, stable LLUDP and CAPS | Partial / unverified; not live-accepted |
| World geometry and DX12 residency | Transport | Complete linked meshes, opaque/alpha geometry, textures, terrain and PBR without unexplained omissions | Blocked; live acceptance missing |
| Avatar and movement | Transport, world | Server appearance, outfit, attachments and authoritative movement | Blocked; live acceptance missing |
| Inventory | Transport | Remote folders, permissions and server-side actions | Blocked; live acceptance missing |
| Messaging and read receipts | Transport | Live IM/group/local chat and interoperable delivery/read state | Blocked; live acceptance missing |
| Camera and graphics | World, avatar | Controls and graphics settings applied to the live viewport without regressions | Partial / unverified; not live-accepted |
| Creator/Photo Studio | World, inventory | Supported upload operations and captured live pixels | Partial / unverified; not live-accepted |
| Smart Sort, accessibility and hotfixes | Inventory, shell | User-visible integrated operations and safe updates | Partial / unverified; not live-accepted |
| Release packaging | All above | Native viewer acceptance, CTest, forced WARP, artifact inspection and authenticated in-world verification | Blocked; live acceptance missing |

## Branch inventory and promotion policy

- `feature/kami-original-core` is the independent native KAMI baseline and is an ancestor of the live integration branch. It is not wholesale-merged over the newer destination.
- `feature/kami-original-viewer-reintegration` currently contributes workflow configuration only; it does not contain a separate original UI source implementation to merge. The original KAMI UI remains in `src/app/main.cpp`, `src/app/window.cpp` and the native `KAMIViewer` target.
- `feature/kami-live-viewer-integration` is the only promotion destination. Its original KAMI application entry point and UI remain the executable; diagnostics are not a replacement application.
- `feature/kami-r72-black-dragon-geometry-port` is treated as a source of individually reviewed fixes, not a branch to merge wholesale. R72 mesh-interest recovery and exception-safe mesh/texture worker recovery have already been ported into the destination as real source changes with regression coverage. Linkset descendant removal is present in the destination transport and object-spatial tests.
- DX12 renderer, avatar, camera, creator/photo and SL transport branches are candidates only. Each implementation commit must pass its own source Windows CI and the destination validation before promotion.

## Automated integration pipeline

`.github/workflows/kami-unified-integration.yml` runs for ready pull requests targeting `feature/kami-live-viewer-integration`. The source branch must be on the reviewed allowlist and its exact head must have a successful `KAMI Original Core Windows` run. The pipeline then:

1. Checks the feature manifest and dependency graph, source and destination heads, and every queued/running destination workflow.
2. Selects up to three unported single-parent implementation commits in source ancestry order. Each selected commit uses its own latest successful Windows build when available; otherwise it is covered by the exact source-head Windows build. The workflow dispatches that build on the source ref when no current successful run exists, and requires the actual `windows` job to pass (a skipped job is not CI evidence). A newer failed or running attempt cannot be masked by an older green run. Patch-equivalent commits already present in the destination are skipped. Test-only, documentation-only, workflow-only, binary and mixed-scope commits are rejected, and each batch must carry regression-test coverage.
3. Cherry-picks the ordered source commits locally with provenance. Related helper/fix commits are applied in dependency order; conflicts or stale source/destination heads stop the run. If more eligible commits remain, the source PR is safely reopened after a successful destination merge to queue the next batch.
4. Builds the actual `KAMIViewer` target, builds the regression suite, runs CTest, and forces Microsoft WARP to verify DX12 output pixels.
5. Pushes the validated tree to a unique temporary branch and opens a promotion PR. The destination Windows workflow builds the merge candidate and packages the native viewer. The PR is merged only if that destination-targeted CI succeeds, the artifact exists, and the destination base SHA has not moved.

The destination Windows workflow is run for isolated `kami/promotion-*` pull requests targeting the live integration branch; ordinary source-branch PRs do not build their entire divergent tree as a deliverable. The validation archive must contain exactly one executable, `KAMIViewer.exe`; the DX12 smoke tool and all test executables remain outside the package. The promotion PR uses a merge commit so individual `-x` cherry-picks and their source provenance remain visible for patch-equivalence checks on later batches. The candidate includes `NOT_A_COMPLETE_VIEWER.txt` while live acceptance is blocked.

A conflict, stale head, active destination run, failed test, failed WARP check, missing artifact or failed destination PR CI blocks the merge. No destination force-push is used and no diagnostic executable is delivered.

## Automation safety

1. Check branch heads and queued/running Actions jobs before writes. If a source branch has no current Windows build, dispatch the existing Windows workflow on that exact ref and require its real `windows` job to pass. Serialize promotion jobs and recheck destination head immediately before merge.
2. Port compatible implementation commits individually; do not merge divergent branches or replace the original application entry point.
3. Keep regression tests alongside source changes where appropriate; always run the full suite and WARP against the candidate tree and destination PR merge result.
4. Preserve independent DX12 architecture, licensing and bounded worker/cache behavior. Synthetic tests and WARP do not prove live SL correctness.
5. The candidate archive is explicitly incomplete and contains only the native KAMI viewer executable plus required runtime/license files. It is not a release.

## Known blockers

- No authenticated live SL populated-region acceptance has been recorded.
- Complete mesh/linkset/alpha/PBR residency and terrain coverage still need real-region verification.
- Avatar appearance, outfit/attachment state, inventory operations, messaging and read receipts are not proven against live SL services.
- Creator/photo operations and several settings remain local previews or partial integrations.
- A green Windows build, CTest suite or WARP render does not establish a complete in-world viewer.
