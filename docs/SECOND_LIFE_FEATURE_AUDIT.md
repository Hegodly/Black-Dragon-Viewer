# KAMI independent viewer checklist (official Second Life source audit)

Reference used solely to understand expected features:
- Upstream: https://github.com/secondlife/viewer/tree/develop/indra/newview
- Checked at the November 3, 2026 KAMI development milestone.
- The upstream viewer is LGPL-2.1; this new KAMI original-core branch imports **no upstream C++ implementation, resource, shader, skin or build script**. It is not a source-to-source translation.

## Basic viewer feature mapping

| Official viewer area / representative upstream files | KAMI-original code | Runtime state |
|---|---|---|
| Login / agent state (`llagent.cpp`, `llagentdata.cpp`) | `protocol::Session`, viewer feature catalog | Not connected; no SL login |
| Avatar camera (`llagentcamera.cpp`, `llfloatercamera.cpp`) | `camera::System` | Original offline 75-degree DX12 camera ready |
| Inventory (`llinventorybridge.cpp`, `llinventoryfilter.cpp`) | `inventory::preview_smart_sort`, `viewer::OutfitDraft` | Local planning only; no SL write |
| Chat and IM (`llchatbar.cpp`, `llchathistory.cpp`) | `social::Outbox` | Staged drafts, no network messaging |
| Friends (`llpanelpeople.cpp`, `llfriendcard.cpp`) | Viewer capability catalog | Requires presence / consent / network |
| Groups (`llgroupactions.cpp`, `llgrouplist.cpp`) | Viewer capability catalog | Requires network and roles |
| World map (`llfloatermap.cpp`) | `viewer::LandmarkLibrary` | Local location references only |
| Build/edit (`llfloatertools.cpp`, `llpanelobject.cpp`) | `creator::inspect`, `creator::import_obj` | Local geometry preview only; no SL object editing |
| Photo (`llfloatersnapshot.cpp`, `llfloater360capture.cpp`) | `photo::Settings`, DX12 capture, native WIC PNG | Native current-frame PNG in offline DX12 scene |
| Movement / following / teleport | `navigation::Planner` | Permission-aware request planning, not execution |
| Asset and avatar appearance streaming | Original device/material/scene modules | Original offline test geometry only |
| Parcels, gestures, animations | Viewer capability catalog and offline spring | Requires server and asset animation interfaces |

These file names are audit references, **not dependencies or copied implementations**.

## KAMI-only useful features built into the original branch

1. Creator Studio: bounded OBJ importer, source triangle/vertex validation, geometry audit, 3D preview inside the native DX12 scene. Does not circumvent Second Life mesh restrictions.
2. Photo Studio: native preset-driven DX12 grading; 8-bit PNG readback with Windows Imaging Component. No GShade, MXAO or filter injection.
3. KAMI quality guard: Potato / Performance / Balanced / Quality presets and auto scale controller with hysteresis.
4. KAMI camera: precision mode, shoulder positions, focus tracking and cinematic orbit.
5. KAMI mesh budget advisor: clearly labelled estimate of uncompressed mesh/texture footprint, not upload fees.
6. Offline outfit staging and bookmark notebook: data structures with bounds checking; no pretend avatar wear or teleport action.
7. Independent receipt/permission gating: no fake read receipts, viewer detection, teleport or remote inventory mutations.
8. Local DXGI VRAM / process CPU and RAM diagnostics.

## Required before KAMI can become a usable Second Life viewer

Independent implementation and live integration of login transport, UDP region circuits, HTTP capabilities and events, asset decoders, object/appearance streams, inventory service, chat, groups, friends, teleports, parcel permissions, mesh upload/cost APIs and live object editing. The third-party viewer requirements and server-side rules cannot be bypassed with DirectX 12.

**Project boundary:** The current repo is still public even though the owner wants proprietary KAMI original code. Repository visibility and license should be addressed separately before distributing these files.
