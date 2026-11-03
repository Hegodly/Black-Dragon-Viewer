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
# t0 shadow map, t1 albedo, t2 dynamic skinned-mesh bone palette
assert "NumDescriptors=3" in (root/"src/world/shader_cache.cpp").read_text(encoding="utf-8")
assert "skinRows" in shader and "BLENDINDICES" in shader
assert "make_skin_palette()" in renderer
assert (root/"src/features/rig.cpp").is_file()
assert (root/"src/features/rig_physics.cpp").is_file()
assert (root/"include/kami/features/creator_limits.hpp").is_file()
creator=(root/"src/features/creator_studio.cpp").read_text(encoding="utf-8")
assert "kMaxPreviewTriangles" in creator
assert "skipped_degenerate_triangles" in creator
assert "Microsoft::WRL::ComPtr<ID3D12Resource> candidate" in renderer
assert (root/"src/protocol/transport.cpp").is_file()
assert (root/"src/protocol/bootstrap.cpp").is_file()
assert (root/"src/protocol/services.cpp").is_file()
assert (root/"src/protocol/lludp.cpp").is_file()
assert (root/"src/protocol/login.cpp").is_file()
assert (root/"src/protocol/load_scheduler.cpp").is_file()
login=(root/"src/protocol/login.cpp").read_text(encoding="utf-8")
assert "login_to_simulator" in login
assert "WINHTTP_FLAG_SECURE" in login
assert "ProhibitDTD" in login
assert "kami_sl_host_volume_id(" in login
assert "src/sl_reference/official_login_identity.cpp" in cmake
assert "kami_sl_login_identity" in cmake
assert (root/"third_party/secondlife-lgpl/LICENSE.LGPL-2.1.txt").is_file()
assert "login_http_diagnostic(code)" in login
assert "failure.message=login_http_diagnostic(code)" in login
assert "KAMI Beyond Reality Dev" in login
assert "src/protocol/load_scheduler.cpp" in cmake
assert "src/protocol/caps.cpp" in cmake
assert (root/"include/kami/protocol/caps.hpp").is_file()
assert "src/protocol/lludp.cpp" in cmake
assert "src/protocol/simulator_wire.cpp" in cmake
assert "src/protocol/simulator_transport.cpp" in cmake
assert "ws2_32" in cmake
app=(root/"src/app/window.cpp").read_text(encoding="utf-8")
assert "CredUIPromptForCredentialsW" in app
assert "CREDUI_FLAGS_DO_NOT_PERSIST" in app
assert "CREDUI_FLAGS_ALWAYS_SHOW_UI" in app
assert "std::async(std::launch::async" in app
assert "sl_circuit_.poll()" in app
assert "login.agni.lindenlab.com" in app
assert "src/protocol/simulator_transport.cpp" in cmake
wire=(root/"src/protocol/simulator_wire.cpp").read_text(encoding="utf-8")
assert "use_circuit_code" in wire and "complete_agent_movement" in wire
sock=(root/"src/protocol/simulator_transport.cpp").read_text(encoding="utf-8")
assert "recv(socket_" in sock and "connect(candidate" in sock
assert "packets_received_" in sock
# Reject accidental reintroduction of legacy 3D renderers anywhere in KAMI.
# Direct2D/DirectWrite are permitted for *2D UI only*.
graphics_roots=(root/"src/world",root/"include/kami/world")
for folder in graphics_roots:
    for item in folder.rglob("*"):
        if not item.is_file() or item.suffix not in (".cpp",".hpp",".h"):
            continue
        shader_source=item.read_text(encoding="utf-8")
        assert not re.search(r'(?m)^\s*#\s*include\s*[<"](?:GL/|glad|GLFW|d3d11|vulkan/)',shader_source,re.I),item
        assert not re.search(r'\b(?:wgl[A-Z]\w*|glCreateShader|glBindBuffer|glDrawArrays|D3D11CreateDevice|vkCreateInstance)\s*\(',shader_source),item
assert "src/world/region_geometry.cpp" in cmake
assert "region_mesh_?*region_mesh_:generated" in renderer
assert "device_.initialize(allow_software_dx12)" in renderer
assert "bool allow_software_dx12=false" in (root/"include/kami/world/renderer.hpp").read_text(encoding="utf-8")
assert "d3d11" not in re.sub(r'(?m)^\s*#.*$', "", cmake.lower())
assert "opengl" not in re.sub(r'(?m)^\s*#.*$', "", cmake.lower())

transport=(root/"src/protocol/transport.cpp").read_text(encoding="utf-8")
assert "WINHTTP_FLAG_SECURE" in transport
assert "WinHttpSendRequest" in transport
assert "winhttp" in cmake
assert "probe_login_tls" in (root/"src/app/window.cpp").read_text(encoding="utf-8")
assert "ggx_distribution" in shader
print("PASS: KAMI DX12 core remains separate; official SL login identity adaptation isolated in LGPL DLL")
