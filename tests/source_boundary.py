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
assert "src/settings/graphics.cpp" in cmake
assert "src/world/scene.cpp" in cmake
assert "winmm" in cmake
assert "d3d12 dxgi" in cmake
assert (root/"src/world/shader_cache.cpp").is_file()
assert (root/"src/world/warmup.cpp").is_file()
assert "shader_warmup_.begin()" in (root/"src/app/window.cpp").read_text(encoding="utf-8")
assert not re.search(r"\b(?:OpenGL|glCreateShader|wglCreateContext|D3D11CreateDevice)\s*\(",renderer)

ui=(root/"src/app/window.cpp").read_text(encoding="utf-8")
renderer=(root/"src/world/renderer.cpp").read_text(encoding="utf-8")
assert "GRAPHICS + FPS" in ui and "advance_graphics(" in ui
assert "DXGI_PRESENT_ALLOW_TEARING" in renderer
assert "graphics_.render_scale_percent" in renderer
assert "graphics_.terrain_divisions()" in renderer
assert "graphics_.vsync" in renderer
assert "make_shadow()" in renderer
assert "D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE" in renderer
assert "shadow_srv_heap_" in renderer
shader=(root/"include/kami/world/shaders.hpp").read_text(encoding="utf-8")
assert "shadow_vertex_main" in shader
assert "shadow_factor" in shader
assert "materialTexture.Sample" in shader
assert "make_material_texture()" in renderer
assert "CopyTextureRegion" in renderer
assert "GetCopyableFootprints" in renderer
assert "material_texture_" in renderer
assert "NumDescriptors=2" in (root/"src/world/shader_cache.cpp").read_text(encoding="utf-8")
assert "ggx_distribution" in shader
print("PASS: original KAMI graphics controls are wired to DX12 backend; no imported viewer code")
