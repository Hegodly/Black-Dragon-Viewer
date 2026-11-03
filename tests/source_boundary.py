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
renderer = (root/"src/world/device.cpp").read_text(encoding="utf-8")
cmake = (root/"CMakeLists.txt").read_text(encoding="utf-8")
assert "D3D12CreateDevice" in renderer
assert "D3D12_COMMAND_LIST_TYPE_DIRECT" in renderer
assert "DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE" in renderer
assert "src/world/device.cpp" in cmake
assert "d3d12 dxgi" in cmake
assert not re.search(r"\b(?:OpenGL|glCreateShader|wglCreateContext|D3D11CreateDevice)\s*\(",renderer)

print("PASS: independent KAMI source boundary; no imported viewer code")
