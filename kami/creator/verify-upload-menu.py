#!/usr/bin/env python3
"""Verify native Creator Studio menu routing remains clickable and safe.

Run after theme and upload integration scripts, before expensive Windows compile.
A static test cannot prove in-world mesh entitlement, server fee or actual UI
behavior; the grid still decides whether mesh upload is available.
"""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
VIEW = ROOT / "indra/newview"
menu = ET.parse(VIEW / "skins/default/xui/en/menu_viewer.xml").getroot()
items = [el for el in menu.iter("menu_item_call") if el.get("name") == "Upload Model"]
assert len(items) == 1, "Expected exactly one native Model upload menu entry"
item = items[0]
assert "Creator Studio" in item.get("label", ""), "Model upload label must identify Creator Studio"
assert item.get("enabled", "true").lower() != "false", "Model menu is hard-disabled"
callbacks = [(el.tag, el.get("function")) for el in item]
expected = [
    ("menu_item_call.on_click", "File.UploadModel"),
    ("menu_item_call.on_enable", "File.EnableUploadModel"),
    ("menu_item_call.on_visible", "File.VisibleUploadModel"),
]
assert callbacks == expected, f"Model menu callbacks missing or out of order: {callbacks!r}"
source = (VIEW / "llviewermenufile.cpp").read_text(encoding="utf-8-sig")
for sig in (
    'view_listener_t::addCommit(new LLFileUploadModel(), "File.UploadModel");',
    'view_listener_t::addEnable(new LLFileEnableUploadModel(), "File.EnableUploadModel");',
    'view_listener_t::addMenu(new LLMeshUploadVisible(), "File.VisibleUploadModel");',
    'return gMeshRepo.meshUploadEnabled();',
    'fmp->isModelLoading()',
    'LLFloaterModelPreview::showModelPreview();',
):
    assert sig in source, f"Upload callback or grid entitlement check missing: {sig}"
preview = ET.parse(VIEW / "skins/default/xui/en/floater_model_preview.xml").getroot()
assert "CREATOR STUDIO" in preview.get("title", ""), "Wrong model preview floater"
assert any(x.get("name") == "kami_mesh_audit_btn" for x in preview.iter("button")), (
    "Creator Studio Mesh Audit missing")
controller = (VIEW / "llfloatermodelpreview.cpp").read_text(encoding="utf-8-sig")
assert "onKamiMeshAudit" in controller and "runKamiMeshAudit" in controller, (
    "Mesh Audit has no native handler")
print("PASS Creator Studio File > Upload > Model is enabled by native callback")
print("PASS grid mesh-upload eligibility, loading lockout and Mesh Audit intact")
