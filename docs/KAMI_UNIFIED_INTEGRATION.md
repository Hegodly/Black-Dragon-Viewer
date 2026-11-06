# KAMI Unified Viewer Integration Contract

The deliverable is the independent Windows `KAMIViewer.exe` with live Second Life functionality. The seven-in-one diagnostic program and DX12 smoke tool are test utilities, never release deliverables.

## Integration graph and acceptance gates

| Stage | Depends on | Acceptance requirement | Current status |
| --- | --- | --- | --- |
| Native shell and workspaces | Original core | Real KAMI window, navigation and input | Partial |
| Login and simulator transport | Native shell | Authenticated region handshake, stable LLUDP and CAPS | Partial |
| World geometry and DX12 residency | Transport | Complete linked meshes, walls, floors and terrain; no unexplained budget exclusions | Blocked |
| Avatar and movement | Transport, world | Server appearance/outfit and authoritative movement | Blocked |
| Inventory | Transport, avatar | Real remote folders, permissions and actions | Blocked |
| Messaging and read receipts | Transport | Live IM/group/local chat; KAMI-only receipts with compatibility fallback | Blocked |
| Camera and graphics | World, avatar | Controls applied to live viewport without regressions | Partial |
| Creator/Photo Studio | World, inventory | Real supported uploads and captured live pixels | Partial |
| Smart Sort, accessibility, hotfix | Inventory, shell | User-visible integrated operations and safe updates | Partial |
| Release packaging | All above | Full viewer acceptance tests, WARP DX12 smoke, artifact manifest and user in-world verification | Blocked |

## Automation safety

1. Check the branch head and active Actions runs before editing. Do not write concurrently with another integration run.
2. Inventory original feature branches; port compatible implementations into the single live-viewer branch, avoiding wholesale merges of divergent or test-only application entrypoints.
3. Make coherent changes, compile and run automated tests, inspect failed logs and repair failures before the next change.
4. Preserve verified live login/world functionality and retain diagnostic tools solely as CI test executables.
5. Never describe a passing WARP or unit test as proof that a real Second Life region, avatar, inventory or messaging works.
6. Publish an actual KAMIViewer release candidate only after all required gates pass. Until then name packages development candidates and enumerate unsupported services.

## Known blockers

- The combined-scene DX12 draw path remains authoritative while per-instance resident draw coverage is not yet activated.
- Dense-region diagnostics have reported thousands of meshes excluded by global geometry admission even after download queues drained.
- Avatar appearance, remote inventory, messaging and several feature screens remain partial or local-only.
- A green GitHub Actions build does not establish in-world completeness.
