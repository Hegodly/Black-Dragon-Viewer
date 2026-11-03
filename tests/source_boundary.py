from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
files=[p for p in root.rglob("*") if p.is_file() and ".git" not in p.parts]
assert files
for p in files:
    name=p.relative_to(root).as_posix().lower()
    assert not name.startswith(("indra/","llcommon/","blackdragon/","firestorm/")),name
    if p.suffix.lower() in (".cpp",".hpp",".h",".cmake"):
        text=p.read_text(encoding="utf-8")
        assert not re.search(r"black.?dragon|firestorm|NiranV|#include\s+\"llviewer",text,re.I),name
assert not (root/".gitmodules").exists()
assert (root/"src/app/window.cpp").is_file()
assert (root/"src/protocol/session.cpp").is_file()
print("PASS: independent KAMI source boundary; no imported viewer code")
