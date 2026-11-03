cmake_minimum_required(VERSION 3.25)
project(KAMI VERSION 0.1.0 LANGUAGES CXX)
if(NOT WIN32)
  message(FATAL_ERROR "KAMI Original Core is Windows-only.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(kami_core STATIC src/ui/palette.cpp src/settings/appearance.cpp src/settings/graphics.cpp src/settings/camera.cpp src/camera/system.cpp src/graphics/adaptive.cpp src/features/inventory.cpp src/features/creator.cpp src/features/creator_studio.cpp src/features/photo_studio.cpp src/features/viewer_foundation.cpp src/features/social.cpp src/features/messenger.cpp src/features/navigation.cpp src/features/physics.cpp src/features/avatar_motion.cpp src/features/body_physics.cpp src/features/rig.cpp src/features/rig_physics.cpp src/features/rig_diagnostics.cpp src/features/hotfix.cpp src/features/telemetry.cpp src/protocol/session.cpp src/protocol/transport.cpp src/protocol/bootstrap.cpp src/protocol/services.cpp src/world/device.cpp src/world/scene.cpp src/world/renderer.cpp src/world/shader_cache.cpp src/world/warmup.cpp)
target_include_directories(kami_core PUBLIC include)
target_compile_definitions(kami_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(kami_core PUBLIC d2d1 dwrite dwmapi advapi32 d3d12 dxgi d3dcompiler shell32 ole32 winmm winhttp bcrypt psapi windowscodecs comdlg32)
add_executable(KAMIViewer WIN32 src/app/main.cpp src/app/window.cpp)
target_include_directories(KAMIViewer PRIVATE include)
target_link_libraries(KAMIViewer PRIVATE kami_core ole32)
if(MSVC)
  target_compile_options(kami_core PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_compile_options(KAMIViewer PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
enable_testing()
add_executable(kami_session_tests tests/session_tests.cpp)
target_link_libraries(kami_session_tests PRIVATE kami_core)
add_test(NAME KamiOriginalSessionIntegrity COMMAND kami_session_tests)

add_executable(kami_dx12_tests tests/dx12_backend_tests.cpp)
target_link_libraries(kami_dx12_tests PRIVATE kami_core)
add_test(NAME KamiOriginalDX12DeviceContract COMMAND kami_dx12_tests)

add_executable(kami_scene_tests tests/scene_tests.cpp)
target_link_libraries(kami_scene_tests PRIVATE kami_core)
add_test(NAME KamiProceduralSceneGeometry COMMAND kami_scene_tests)

add_executable(kami_shader_tests tests/shader_tests.cpp)
target_link_libraries(kami_shader_tests PRIVATE kami_core)
add_test(NAME KamiOriginalHLSLShaderCompilation COMMAND kami_shader_tests)

add_executable(kami_shader_warmup_tests tests/shader_warmup_tests.cpp)
target_link_libraries(kami_shader_warmup_tests PRIVATE kami_core)
add_test(NAME KamiStartupShaderWarmupContract COMMAND kami_shader_warmup_tests)

add_executable(kami_graphics_tests tests/graphics_settings_tests.cpp)
target_link_libraries(kami_graphics_tests PRIVATE kami_core)
add_test(NAME KamiNativeGraphicsSettingsAndPresets COMMAND kami_graphics_tests)

add_executable(kami_texture_tests tests/texture_upload_tests.cpp)
target_link_libraries(kami_texture_tests PRIVATE kami_core)
add_test(NAME KamiDX12TextureFootprintContract COMMAND kami_texture_tests)

add_executable(kami_dx12_smoke tests/dx12_render_smoke.cpp)
target_link_libraries(kami_dx12_smoke PRIVATE kami_core user32)
set_target_properties(kami_dx12_smoke PROPERTIES OUTPUT_NAME "KAMI-DX12-Smoke")
add_test(NAME KamiDX12RealRendererSmoke COMMAND kami_dx12_smoke)
set_tests_properties(KamiDX12RealRendererSmoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 45)

add_executable(kami_camera_tests tests/camera_tests.cpp)
target_link_libraries(kami_camera_tests PRIVATE kami_core)
add_test(NAME KamiOriginalCameraSystem COMMAND kami_camera_tests)

add_executable(kami_adaptive_quality_tests tests/adaptive_quality_tests.cpp)
target_link_libraries(kami_adaptive_quality_tests PRIVATE kami_core)
add_test(NAME KamiAdaptiveDX12RenderScale COMMAND kami_adaptive_quality_tests)

add_executable(kami_inventory_tests tests/inventory_tests.cpp)
target_link_libraries(kami_inventory_tests PRIVATE kami_core)
add_test(NAME KamiNativeInventorySafeSort COMMAND kami_inventory_tests)

add_executable(kami_creator_tests tests/creator_tests.cpp)
target_link_libraries(kami_creator_tests PRIVATE kami_core)
add_test(NAME KamiCreatorMeshPreflight COMMAND kami_creator_tests)

add_executable(kami_social_tests tests/social_tests.cpp)
target_link_libraries(kami_social_tests PRIVATE kami_core)
add_test(NAME KamiMessageReceiptTrust COMMAND kami_social_tests)

add_executable(kami_navigation_tests tests/navigation_tests.cpp)
target_link_libraries(kami_navigation_tests PRIVATE kami_core)
add_test(NAME KamiNavigationPermissionContract COMMAND kami_navigation_tests)

add_executable(kami_physics_tests tests/physics_tests.cpp)
target_link_libraries(kami_physics_tests PRIVATE kami_core)
add_test(NAME KamiProceduralAvatarSpring COMMAND kami_physics_tests)

add_executable(kami_hotfix_tests tests/hotfix_tests.cpp)
target_link_libraries(kami_hotfix_tests PRIVATE kami_core)
add_test(NAME KamiHotfixIntegrityAndTrust COMMAND kami_hotfix_tests)

add_executable(kami_telemetry_tests tests/telemetry_tests.cpp)
target_link_libraries(kami_telemetry_tests PRIVATE kami_core)
add_test(NAME KamiSystemTelemetryContract COMMAND kami_telemetry_tests)

add_executable(kami_creator_studio_tests tests/creator_studio_tests.cpp)
target_link_libraries(kami_creator_studio_tests PRIVATE kami_core)
add_test(NAME KamiNativeOBJCreatorStudio COMMAND kami_creator_studio_tests)

add_executable(kami_photo_studio_tests tests/photo_studio_tests.cpp)
target_link_libraries(kami_photo_studio_tests PRIVATE kami_core ole32)
add_test(NAME KamiNativePhotoStudioPresetsAndPNG COMMAND kami_photo_studio_tests)

add_executable(kami_viewer_foundation_tests tests/viewer_foundation_tests.cpp)
target_link_libraries(kami_viewer_foundation_tests PRIVATE kami_core)
add_test(NAME KamiOriginalViewerFoundation COMMAND kami_viewer_foundation_tests)

add_executable(kami_messenger_tests tests/messenger_tests.cpp)
target_link_libraries(kami_messenger_tests PRIVATE kami_core)
add_test(NAME KamiLocalMessengerOfflineContract COMMAND kami_messenger_tests)

add_executable(kami_avatar_motion_tests tests/avatar_motion_tests.cpp)
target_link_libraries(kami_avatar_motion_tests PRIVATE kami_core)
add_test(NAME KamiOriginalAvatarLocomotion COMMAND kami_avatar_motion_tests)

add_executable(kami_body_physics_tests tests/body_physics_tests.cpp)
target_link_libraries(kami_body_physics_tests PRIVATE kami_core)
add_test(NAME KamiLocalBodyPhysicsSpringAndWearableValidation COMMAND kami_body_physics_tests)

add_executable(kami_rig_tests tests/rig_tests.cpp)
target_link_libraries(kami_rig_tests PRIVATE kami_core)
add_test(NAME KamiValidatedFourWeightRigAndBoneHierarchy COMMAND kami_rig_tests)

add_executable(kami_rig_physics_tests tests/rig_physics_tests.cpp)
target_link_libraries(kami_rig_physics_tests PRIVATE kami_core)
add_test(NAME KamiSixAxisPhysicsWearableAndLODValidation COMMAND kami_rig_physics_tests)

add_executable(kami_rig_diagnostics_tests tests/rig_diagnostics_tests.cpp)
target_link_libraries(kami_rig_diagnostics_tests PRIVATE kami_core)
add_test(NAME KamiFittedMeshPhysicsWeightsAndSafetyAudit COMMAND kami_rig_diagnostics_tests)

add_executable(kami_transport_contract_tests tests/transport_contract_tests.cpp)
target_link_libraries(kami_transport_contract_tests PRIVATE kami_core)
add_test(NAME KamiOfficialSLTLSAndCapabilityURLPolicy COMMAND kami_transport_contract_tests)

add_executable(kami_bootstrap_tests tests/bootstrap_tests.cpp)
target_link_libraries(kami_bootstrap_tests PRIVATE kami_core)
add_test(NAME KamiSLLoginRegionBootstrapLifecycle COMMAND kami_bootstrap_tests)

add_executable(kami_services_contract_tests tests/services_contract_tests.cpp)
target_link_libraries(kami_services_contract_tests PRIVATE kami_core)
add_test(NAME KamiSLFeatureRequestAndSnapshotTrustGates COMMAND kami_services_contract_tests)
