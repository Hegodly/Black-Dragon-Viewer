#!/usr/bin/env python3
"""Install KAMI's approved artwork into a checked-out Windows viewer source tree.

Unzip the KAMI official branding kit into kami/assets/KAMI_Branding before running.
This changes branding assets only; it preserves upstream credits and license files.
"""
from pathlib import Path
import shutil
import sys

root = Path(__file__).resolve().parents[1]
assets = root / "kami/assets/KAMI_Branding"
required = ["KAMI_Viewer.ico", "KAMI_Installer_Icon.bmp",
            "KAMI_Official_Logo_Original.png", "KAMI_Login_Banner_1400x480.png",
            "KAMI_Splash_1600x900.png"]

def main():
    missing = [x for x in required if not (assets / x).is_file()]
    if missing:
        raise SystemExit("Missing KAMI artwork: " + ", ".join(missing) +
                         ". Extract the official kit to kami/assets/KAMI_Branding.")

    def put(name, relative):
        src, dst = assets / name, root / relative
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        print("KAMI art:", relative)

    for variant in ("release", "beta", "project", "test"):
        base = root / "indra/newview/icons" / variant
        if not base.is_dir():
            continue
        put("KAMI_Viewer.ico", f"indra/newview/icons/{variant}/blackdragon.ico")
        put("KAMI_Installer_Icon.bmp", f"indra/newview/icons/{variant}/blackdragon_256.BMP")
        for size in (16, 32, 48, 128, 256, 512):
            filename = f"KAMI_App_Icon_{size}.png"
            if (assets / filename).exists():
                put(filename, f"indra/newview/icons/{variant}/blackdragon_{size}.png")

    for name in ("indra/newview/res/bd_icon.ico",
                 "indra/newview/installers/windows/install_icon.ico",
                 "indra/newview/installers/windows/uninstall_icon.ico"):
        put("KAMI_Viewer.ico", name)
    for name in ("indra/newview/res/bd_icon.BMP", "indra/newview/res-sdl/bd_icon.BMP"):
        if (root / Path(name).parent).is_dir():
            put("KAMI_Installer_Icon.bmp", name)

    for asset, dest in (
        ("KAMI_Official_Logo_Original.png", "kami_logo.png"),
        ("KAMI_Login_Banner_1400x480.png", "kami_login_banner.png"),
        ("KAMI_Splash_1600x900.png", "kami_splash.png"),
    ):
        put(asset, "indra/newview/skins/default/textures/windows/" + dest)

    manifest = root / "indra/newview/skins/default/textures/textures.xml"
    data = manifest.read_text(encoding="utf-8-sig")
    if 'name="KAMI_Official_Logo"' not in data:
        entries = (
            '  <texture name="KAMI_Official_Logo" file_name="windows/kami_logo.png" preload="false" />\n'
            '  <texture name="KAMI_Login_Banner" file_name="windows/kami_login_banner.png" preload="false" />\n'
            '  <texture name="KAMI_Splash" file_name="windows/kami_splash.png" preload="false" />\n'
        )
        if "</textures>" not in data:
            raise RuntimeError("Invalid textures.xml: closing tag missing")
        manifest.write_text(data.replace("</textures>", entries + "</textures>"), encoding="utf-8")
    print("KAMI artwork staged. Rebuild to apply resource icons and splash text.")

if __name__ == "__main__":
    main()
