cmake_minimum_required(VERSION 3.21)
project(KamiDX12Foundation VERSION 0.1 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Kami DX12 is a native Windows-only renderer. No OpenGL fallback.")
endif()

add_library(kami_dx12_native STATIC KamiDx12Renderer.cpp KamiDx12World.cpp KamiDx12Renderer.h)
target_compile_features(kami_dx12_native PUBLIC cxx_std_17)
target_compile_definitions(kami_dx12_native PUBLIC
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(kami_dx12_native PUBLIC .)
target_link_libraries(kami_dx12_native PUBLIC d3d12 dxgi d3dcompiler)

add_executable(kami_dx12_smoke KamiDx12Smoke.cpp)
target_link_libraries(kami_dx12_smoke PRIVATE kami_dx12_native user32)

if(MSVC)
    target_compile_options(kami_dx12_native PRIVATE /W4 /EHsc)
    target_compile_options(kami_dx12_smoke PRIVATE /W4 /EHsc)
endif()

enable_testing()
# Uses Microsoft's native DirectX WARP software adapter on Windows CI, not GL.
# An actual D3D12 shader renders a triangle into a texture; a GPU readback
# verifies both rasterization and the cleared background.
add_test(NAME kami_dx12_native_warp_rasterizer
    COMMAND kami_dx12_smoke --warp --offscreen)
set_tests_properties(kami_dx12_native_warp_rasterizer PROPERTIES TIMEOUT 90)
