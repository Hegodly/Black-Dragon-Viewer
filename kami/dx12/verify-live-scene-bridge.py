#!/usr/bin/env python3
"""Validate the first real SL geometry-to-native-D3D12 scene bridge.

Source contracts only; successful MSVC viewer compilation, in-world comparison
and avatar/terrain/PBR migration are separate required milestones.
"""
from pathlib import Path
import ast

root = Path(__file__).resolve().parents[2]
source = (root / "indra/newview/kami_dx12_scene_bridge.cpp").read_text("utf-8-sig")
header = (root / "indra/newview/kami_dx12_scene_bridge.h").read_text("utf-8-sig")
viewer_cmake = (root / "indra/newview/CMakeLists.txt").read_text("utf-8-sig")
link_test = (root / "kami/dx12/verify-viewer-link.py").read_text("utf-8-sig")

def check(name, condition):
    if not condition:
        raise AssertionError(f"KAMI native world geometry: {name}")
    print("PASS", name)

check("capture real decoded Second Life LLVolumeFace data",
      all(token in source for token in (
          "gObjectList.getNumObjects()", "gObjectList.getObject(objectNumber)",
          "getPCode() != LL_PCODE_VOLUME", "object->getVolumeConst()",
          "volume->getVolumeFace(faceNumber)", "face.mPositions[vertex].getF32ptr()",
          "mesh.indices.push_back(value)")))
check("capture actual camera projection and object transforms",
      all(token in source for token in (
          "camera.getOrigin()", "camera.getLeftAxis()", "camera.getUpAxis()",
          "camera.getAtAxis()", "camera.getView()", "camera.getAspect()",
          "getRenderPosition()", "getRenderRotation()", "object->getScale()",
          "scaled * rotation + position", "mesh.worldViewProjection = projection")))
check("honor existing frustum and strict memory budget",
      "camera.sphereInFrustum(position, radius)" in source and
      "kMaxMeshes = 256" in source and
      "kMaxGeometryBytes = 32u * 1024u * 1024u" in source and
      "kMaxGeometryBytes - uploadedBytes" in source and
      "results.hitBudget = true" in source)
check("do not incorrectly render rigged or translucent material paths",
      "object->getAvatarAncestor()" in source and
      "if (face.mWeights)" in source and
      "diffuse.mV[3] < 0.999f" in source and
      "skippedRiggedFaces" in header and
      "skippedTranslucentFaces" in header)
check("view camera positions map to native D3D12 depth 0-1",
      "farZ / (farZ - nearZ)" in source and
      "-nearZ * zscale" in source and
      "dot3(relative, forward)" in source)
check("no OpenGL calls used by scene capture",
      not any(token in source for token in (
          "glBind", "glDraw", "glGet", "wgl", "glVertex", "glFlush",
          "gGL.", "glClear")))
check("CMake builds bridge in actual Windows viewer",
      "kami_dx12_scene_bridge.cpp" in viewer_cmake and
      "kami_dx12_scene_bridge.cpp" in link_test)
print("KAMI DX12 live SL volume geometry bridge source checks PASS")
