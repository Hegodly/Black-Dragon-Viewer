#!/usr/bin/env python3
"""Produce the exact full-release ZIP expected by the KAMI automatic updater.

Only upload this ZIP to an official KAMI GitHub Release after verification.
A CI artifact is NOT automatically a published GitHub Release.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import zipfile

REQUIRED = {
    "KAMIViewer.exe", "app_settings/settings.xml",
    "app_settings/kami-updater.ps1",
    "app_settings/kami-test-updater.ps1",
    "app_settings/KAMI-Test-Update.zip",
    "app_settings/KAMI-Test-Update.json",
    "skins/default/xui/en/panel_login.xml",
    "KAMI-README.txt", "KAMI-CREDITS.txt", "licenses.txt",
}
def package(source: Path, archive: Path) -> dict:
    source = source.resolve()
    archive = archive.resolve()
    assert source.is_dir(), f"Missing compiled output: {source}"
    files = sorted(p for p in source.rglob("*") if p.is_file())
    relative = {p.relative_to(source).as_posix():p for p in files}
    missing = sorted(REQUIRED - relative.keys())
    assert not missing, f"Release missing required entries: {missing}"
    assert archive not in files, "Cannot zip the ZIP into itself"
    for rel,p in relative.items():
        path = PurePosixPath(rel)
        assert not p.is_symlink() and not path.is_absolute() and ".." not in path.parts
        assert p.stat().st_size < (1 << 31), f"Unexpectedly huge artifact: {rel}"
    archive.parent.mkdir(parents=True, exist_ok=True)
    partial = archive.with_suffix(".zip.partial")
    if partial.exists(): partial.unlink()
    with zipfile.ZipFile(partial, "w", zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for rel,p in relative.items():
            z.write(p,rel)
    with zipfile.ZipFile(partial) as z:
        names = z.namelist()
        assert len(names) == len(files) and len(names) == len(set(names))
        assert REQUIRED.issubset(set(names))
        assert z.testzip() is None, "Damaged release ZIP"
    partial.replace(archive)
    result = {
        "filename": archive.name, "schema": 1, "files":len(files),
        "size":archive.stat().st_size,
        "sha256":hashlib.sha256(archive.read_bytes()).hexdigest(),
        "published":False
    }
    assert result["size"] > 1048576, "Release ZIP is unexpectedly small"
    assert result["size"] < 2147483648, "Release ZIP exceeds updater's accepted maximum"
    archive.with_suffix(".json").write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("PASS: full KAMI Windows release ZIP includes native updater and offline test update")
    print(json.dumps(result,indent=2))
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source",type=Path,default=Path("build-vc170-64/newview/Release"))
    parser.add_argument("--output",type=Path,default=Path("kami-release/KAMI-Windows-Portable.zip"))
    args=parser.parse_args()
    package(args.source,args.output)

if __name__ == "__main__": main()
