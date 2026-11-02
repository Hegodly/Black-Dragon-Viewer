#!/usr/bin/env python3
"""Finalize a portable, user-facing KAMI Viewer distribution.

Runs after KAMIViewer.exe is copied from the old internal build target.
Never delete required runtime dependencies or upstream licensing/credits.
The internal settings_blackdragon.xml name must be kept until its C++
runtime readers and config migration are changed and tested.
"""
from __future__ import annotations
import argparse
import hashlib
from pathlib import Path
import xml.etree.ElementTree as ET

KAMI_EXE = "KAMIViewer.exe"
WORKSPACE_ACTIONS = {
    "hub_inventory", "hub_places", "hub_map", "hub_world",
    "hub_outfits", "hub_poser", "hub_pose_creator",
    "hub_material", "hub_snapshot", "hub_camera",
    "hub_preferences", "hub_hardware", "hub_toolbar",
}
LEGACY_ROOT = {
    "blackdragon-bin.exe", "blackdragon-bin.pdb",
    "blackdragon-bin.exp", "blackdragon-bin.lib", "blackdragon-bin.map",
    "blackdragonviewer.exe", "blackdragon.exe",
    "blackdragonviewer.pdb", "blackdragon.pdb",
}
# Known build/legacy helper scripts copied to the release root by staging.
# These are not viewer runtime dependencies and must NOT ship to residents.
# Keep this allowlist narrow: unknown root scripts still fail validation.
OBSOLETE_ROOT_SCRIPTS = {"start-without-rtx.cmd", "copy_touched.bat"}
REQUIRED = [
    "app_settings/settings.xml",
    "app_settings/settings_blackdragon.xml",
    "app_settings/contributors-source.txt",
    "skins/default/xui/en/panel_login.xml",
    "skins/default/xui/en/floater_preferences.xml",
    "skins/default/xui/en/floater_kami_workspaces.xml",
    "skins/default/xui/en/menu_viewer.xml",
    "skins/default/textures/windows/kami_fire_garden.jpg",
    "skins/default/textures/windows/kami_logo.jpg",
    "licenses.txt",
]
CREDITS = """KAMI Viewer | Beyond Reality
An independent Windows Second Life-compatible viewer.

KAMI is built on the open-source Black Dragon Viewer created by NiranV Dean,
and the upstream Second Life Viewer developed by Linden Lab and its
contributors. Thank you to these developers and the open-source community.

KAMI is an independently maintained fork. It is NOT affiliated with or
endorsed by NiranV Dean or Linden Lab.

Licensing and third-party notices:
- licenses.txt (runtime distribution licenses and third-party notices)
- app_settings/contributors-source.txt (source contributor credits)
- The full viewer source repository, with its original copyright and
  license headers, must remain available in accordance with the applicable
  open-source license obligations.

Rebranding must not erase copyright history, upstream attribution,
proprietary trademarks owned by others, or third-party license requirements.
"""
README = """KAMI VIEWER  |  BEYOND REALITY

KAMIViewer.exe is the ONLY launcher. Open it directly. No CMD, BAT or PowerShell feature installers are required.
Keep app_settings/, skins/, character/, fonts/, DLLs, and helper programs
beside KAMIViewer.exe. They are REQUIRED dependencies, not clutter.

This is an independent Second Life-compatible Windows viewer, not a
separate virtual-world server or grid. Access and asset upload still follow
the simulator's rules.

KAMI's Fire Garden theme, native Workspaces, Control Deck, resolution and
render options remain part of the shipped user interface. KAMI branding
does not remove upstream project licensing or developer credit.

See KAMI-CREDITS.txt and licenses.txt for source/third-party attributions.
Choose personal login music directly in the KAMI login interface.
User MP3s are stored in local Windows user data, never redistributed.
"""


def verify_ui(out: Path) -> None:
    def xml(path: str):
        return ET.parse(out / path).getroot()

    login = xml("skins/default/xui/en/panel_login.xml")
    login_named = {n.get("name"): n for n in login.iter() if n.get("name")}
    assert login_named.get("kami_fire_garden") is not None, "KAMI Fire Garden login missing"
    assert login_named["kami_fire_garden"].get("image_name") == "KAMI_FireGarden"
    assert login_named.get("kami_login_title") is not None
    assert "K A M I" in (login_named["kami_login_title"].text or "")
    assert not list(login.iter("web_browser")), "Legacy streaming login browser shipped"

    menu = xml("skins/default/xui/en/menu_viewer.xml")
    tops = [n for n in menu if n.tag == "menu"]
    assert any(n.get("name") == "KAMI_Workspaces" for n in tops), "Workspaces menu missing"
    brand = [n for n in tops if n.get("name") == "Me"]
    assert len(brand) == 1 and brand[0].get("label") == "KAMI"
    assert all(n.get("label", "").lower() != "dragon" for n in tops), \
        "Old Black Dragon top-level navigation branding survived"

    hub = xml("skins/default/xui/en/floater_kami_workspaces.xml")
    assert hub.get("name") == "kami_workspaces"
    buttons = [n.get("name") for n in hub.iter("button")]
    assert set(buttons) == WORKSPACE_ACTIONS and len(buttons) == len(set(buttons)), \
        "KAMI native Workspaces actions missing, unexpected or duplicated"
    prefs = xml("skins/default/xui/en/floater_preferences.xml")
    assert prefs.get("title", "").startswith("KAMI"), "Preferences not branded"
    ET.parse(out / "app_settings/settings_blackdragon.xml")
    ET.parse(out / "app_settings/settings.xml")


def finalize(out: Path) -> list[str]:
    out = out.resolve()
    if not (out / KAMI_EXE).is_file() or (out / KAMI_EXE).stat().st_size < 4:
        raise RuntimeError("KAMIViewer.exe missing or empty; refuse to publish release")
    for relative in REQUIRED:
        if not (out / relative).is_file():
            raise RuntimeError(f"Required native runtime or license missing: {relative}")

    # Delete ONLY known obsolete launcher/debug artifacts at the package root.
    # Do not use rglob('*blackdragon*'): internal assets or settings may still
    # be referenced by native C++ code and are essential to launch KAMI.
    removed = []
    for f in out.iterdir():
        if f.is_file() and f.name.lower() in LEGACY_ROOT:
            f.unlink()
            removed.append(f.name)
    orphan = [f.name for f in out.iterdir()
              if f.is_file() and "blackdragon" in f.name.casefold()]
    if orphan:
        raise RuntimeError("Unclassified old-brand root files; review before release: " +
                           ", ".join(sorted(orphan)))

    # These texts are user-facing on the old vendor's settings page. Update
    # *descriptions* without touching setting keys, types, paths or credits.
    settings = out / "app_settings/settings_blackdragon.xml"
    content = settings.read_text(encoding="utf-8-sig")
    content = content.replace("<!-- Black Dragon Debugs -->",
                              "<!-- KAMI Viewer compatibility settings -->")
    content = content.replace("taskbar icon of Black Dragon",
                              "taskbar icon of KAMI Viewer")
    settings.write_text(content, encoding="utf-8")
    ET.parse(settings)

    # Remove only two confirmed legacy/build helper scripts left by staging.
    # Unknown scripts still abort release rather than being silently removed.
    # Do not recurse: this only cleans the portable package root.
    for f in out.iterdir():
        if f.is_file() and f.name.casefold() in OBSOLETE_ROOT_SCRIPTS:
            f.unlink()
            removed.append(f.name)

    # Release readiness means there are no first-run feature installers.
    # Unknown root scripts abort packaging rather than being silently deleted.
    # Build scripts belong in the source repository, not the portable output.
    scripts = sorted(f.name for f in out.iterdir() if f.is_file() and
        (f.suffix.casefold() in (".cmd", ".bat", ".ps1") or
         f.name.casefold().startswith("install-")))
    if scripts:
        raise RuntimeError("User-facing installer/launcher scripts found in standalone release: " +
                           ", ".join(scripts))
    login_root = ET.parse(out / "skins/default/xui/en/panel_login.xml").getroot()
    assert any(node.tag == "button" and node.get("name") == "kami_login_music_browse"
               for node in login_root.iter()), (
        "KAMI personal music must be selectable from the native login UI")
    assert not list(out.rglob("Install-GShade-Assets.ps1"))
    assert not list(out.rglob("Install-KAMI-Login-Music.cmd"))
    (out / "KAMI-CREDITS.txt").write_text(CREDITS, encoding="utf-8")
    (out / "KAMI-README.txt").write_text(README, encoding="utf-8")

    verify_ui(out)

    # A deterministic manifest helps compare artifacts without claiming FPS
    # or launch success from a file-only test.
    asset = out / "skins/default/xui/en/menu_viewer.xml"
    manifest = (
        "KAMI | FINAL PORTABLE ASSET VERIFICATION\n"
        "Viewer executable: KAMIViewer.exe\n"
        "Old vendor launchers: 0\n"
        f"Menu SHA256: {hashlib.sha256(asset.read_bytes()).hexdigest()}\n"
        "Runtime compatibility config: app_settings/settings_blackdragon.xml (required)\n"
        "Windows launch and in-world avatar/graphics QA: NOT inferred from file checks\n"
        f"Deleted obsolete root launchers: {len(removed)}\n"
    )
    (out / "KAMI-PORTABLE-VALIDATION.txt").write_text(manifest, encoding="utf-8")
    print(f"PASS: KAMI standalone portable package validated; "
          f"removed {len(removed)} obsolete root launchers.")
    return removed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", default="build-vc170-64/newview/Release")
    opts = parser.parse_args()
    source_root = Path(__file__).resolve().parents[2]
    final = source_root / opts.output
    finalize(final)


if __name__ == "__main__":
    main()
