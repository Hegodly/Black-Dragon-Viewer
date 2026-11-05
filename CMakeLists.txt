cmake_minimum_required(VERSION 3.25)
project(KAMI VERSION 0.1.0 LANGUAGES C CXX)
# Native static OpenJPEG 2.5.4 provides genuine JPEG 2000 J2C decoding.
# Its BSD 2-Clause license is shipped with the Windows build. KAMI keeps
# the DirectX12 renderer independent and does not use OpenGL.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Static OpenJPEG for portable Windows viewer" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_CODEC OFF CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openjpeg
    GIT_REPOSITORY https://github.com/uclouvain/openjpeg.git
    GIT_TAG v2.5.4
    GIT_SHALLOW TRUE
    GIT_PROGRESS FALSE)
FetchContent_MakeAvailable(openjpeg)
# OpenJPEG sets parent-scope output directories. Reset those before
# declaring KAMI targets so native .exe/.dll remain at build/Release,
# as expected by our Windows smoke tests and portable packaging.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
if(NOT WIN32)
  message(FATAL_ERROR "KAMI Original Core is Windows-only.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# LGPL-covered Second Life Windows login identity algorithm lives in a
# replaceable DLL, separated from the independently authored KAMI core.
add_library(kami_sl_login_identity SHARED src/sl_reference/official_login_identity.cpp)
target_include_directories(kami_sl_login_identity PUBLIC include)
target_compile_definitions(kami_sl_login_identity PRIVATE
    KAMI_SL_LOGIN_IDENTITY_EXPORT UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(kami_sl_login_identity PRIVATE bcrypt ole32 oleaut32 wbemuuid)
set_target_properties(kami_sl_login_identity PROPERTIES
    OUTPUT_NAME "KAMI_SL_Login_Identity")
# The official viewer's LGPL LLZeroCode implementation is compiled in
# a separately replaceable DLL. KAMI's renderer remains independent.
add_library(kami_sl_lludp_codec SHARED
    src/sl_reference/official_lludp_zerocode.cpp)
target_include_directories(kami_sl_lludp_codec PUBLIC include
    PRIVATE third_party/secondlife-lgpl/lludp)
target_compile_definitions(kami_sl_lludp_codec PRIVATE
    KAMI_SL_LLUDP_CODEC_BUILD UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(kami_sl_lludp_codec PROPERTIES
    OUTPUT_NAME "KAMI_SL_LLUdpCodec")

# Official LGPL primitive metadata length/type contract, kept replaceable.
add_library(kami_sl_extra_params SHARED
    src/sl_reference/official_extra_params.cpp)
target_include_directories(kami_sl_extra_params PUBLIC include)
target_compile_definitions(kami_sl_extra_params PRIVATE
    KAMI_SL_EXTRAPARAMS_BUILD UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_target_properties(kami_sl_extra_params PROPERTIES
    OUTPUT_NAME "KAMI_SL_ExtraParams")
add_library(kami_core STATIC src/ui/palette.cpp src/settings/appearance.cpp src/settings/graphics.cpp src/settings/camera.cpp src/camera/system.cpp src/graphics/adaptive.cpp src/features/inventory.cpp src/features/creator.cpp src/features/creator_studio.cpp src/features/photo_studio.cpp src/features/viewer_foundation.cpp src/features/social.cpp src/features/messenger.cpp src/features/navigation.cpp src/features/physics.cpp src/features/avatar_motion.cpp src/features/body_physics.cpp src/features/rig.cpp src/features/rig_physics.cpp src/features/rig_diagnostics.cpp src/features/hotfix.cpp src/features/telemetry.cpp src/protocol/session.cpp src/protocol/transport.cpp src/protocol/bootstrap.cpp src/protocol/services.cpp src/protocol/lludp.cpp src/protocol/reliable_circuit.cpp src/protocol/simulator_wire.cpp src/protocol/simulator_transport.cpp src/protocol/simulator_texture.cpp src/protocol/world_wire.cpp src/protocol/object_spatial.cpp src/protocol/object_spatial_full.cpp src/protocol/object_spatial_terse.cpp src/protocol/mesh_reference.cpp src/protocol/texture_entry.cpp src/protocol/texture_decode.cpp src/protocol/mesh_lod.cpp src/protocol/avatar_appearance.cpp src/protocol/live_chat.cpp src/protocol/terrain.cpp src/protocol/login.cpp src/protocol/caps.cpp src/protocol/load_scheduler.cpp src/world/device.cpp src/world/scene.cpp src/world/live_terrain.cpp src/world/live_water.cpp src/world/object_proxies.cpp src/world/live_primitive_geometry.cpp src/world/region_geometry.cpp src/world/renderer.cpp src/world/shader_cache.cpp src/world/warmup.cpp)
target_include_directories(kami_core PRIVATE
    ${openjpeg_SOURCE_DIR}/src/lib/openjp2
    ${openjpeg_BINARY_DIR}/src/lib/openjp2)
target_include_directories(kami_core PUBLIC include third_party/stb)
target_compile_definitions(kami_core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(kami_core PUBLIC kami_sl_login_identity kami_sl_lludp_codec kami_sl_extra_params openjp2 d2d1 dwrite dwmapi advapi32 d3d12 dxgi d3dcompiler shell32 ole32 winmm winhttp iphlpapi oleaut32 bcrypt psapi windowscodecs comdlg32 ws2_32 credui)
add_executable(KAMIViewer WIN32 src/app/main.cpp src/app/window.cpp)
target_include_directories(KAMIViewer PRIVATE include)
target_link_libraries(KAMIViewer PRIVATE kami_core ole32)
if(MSVC)
  target_compile_options(kami_core PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_compile_options(KAMIViewer PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
enable_testing()
# ABI-only test for a *future* separately compiled official LGPL backend.
# This intentionally does not claim or enable SL connectivity.
add_executable(kami_official_sl_bridge_abi_tests
    tests/official_sl_bridge_api_tests.cpp)
target_include_directories(kami_official_sl_bridge_abi_tests PRIVATE include)
add_test(NAME KamiOfficialSLBackendABIContractOnly
    COMMAND kami_official_sl_bridge_abi_tests)
# Run against the *actual* LGPL Linden LLZeroCode implementation loaded
# from KAMI_SL_LLUdpCodec.dll; no simulator/credentials are required.
add_executable(kami_sl_official_zerocode_tests
    tests/official_zerocode_tests.cpp)
target_link_libraries(kami_sl_official_zerocode_tests PRIVATE kami_sl_lludp_codec)
add_test(NAME KamiActualLindenLLUDPZeroCodeCompatibility
    COMMAND kami_sl_official_zerocode_tests)

add_executable(kami_sl_extra_params_tests
    tests/official_extra_params_tests.cpp)
target_link_libraries(kami_sl_extra_params_tests PRIVATE kami_sl_extra_params kami_core)
add_test(NAME KamiOfficialExtraParamsMeshAndModernMaterialTypes
    COMMAND kami_sl_extra_params_tests)
# Validate real High15 simulator motion decoding against known-object state.
add_executable(kami_spatial_terse_tests
    tests/object_spatial_terse_tests.cpp)
target_link_libraries(kami_spatial_terse_tests PRIVATE kami_core)
add_test(NAME KamiLiveTerseObjectPoseUpdatesPreserveAssetIdentity
    COMMAND kami_spatial_terse_tests)
add_executable(kami_sl_texture_entry_tests
    tests/texture_entry_tests.cpp)
target_link_libraries(kami_sl_texture_entry_tests PRIVATE kami_core)
add_test(NAME KamiSLTextureEntryDefaultTintAndBounds
    COMMAND kami_sl_texture_entry_tests)
add_executable(kami_sl_jpeg2000_tests tests/texture_decode_tests.cpp)
target_link_libraries(kami_sl_jpeg2000_tests PRIVATE kami_core)
add_test(NAME KamiGenuineSecondLifeJPEG2000LosslessDecode
    COMMAND kami_sl_jpeg2000_tests)
add_executable(kami_official_login_identity_tests tests/official_login_identity_tests.cpp)
target_link_libraries(kami_official_login_identity_tests PRIVATE kami_sl_login_identity)
add_test(NAME KamiOfficialWindowsLoginIdentityCompatibility
    COMMAND kami_official_login_identity_tests)
add_executable(kami_session_tests tests/session_tests.cpp)
target_link_libraries(kami_session_tests PRIVATE kami_core)
add_executable(kami_reliable_circuit_tests
    tests/reliable_circuit_tests.cpp)
target_link_libraries(kami_reliable_circuit_tests PRIVATE kami_core)
add_test(NAME KamiLiveLLUDPReliableResendsAndDuplicateProtection
    COMMAND kami_reliable_circuit_tests)
add_test(NAME KamiOriginalSessionIntegrity COMMAND kami_session_tests)

add_executable(kami_dx12_tests tests/dx12_backend_tests.cpp)
target_link_libraries(kami_dx12_tests PRIVATE kami_core)
add_test(NAME KamiOriginalDX12DeviceContract COMMAND kami_dx12_tests)

add_executable(kami_seven_systems_tests tests/seven_systems_tests.cpp)
target_link_libraries(kami_seven_systems_tests PRIVATE kami_core)
add_test(NAME KamiSevenInOneEvidenceAndReadinessNotExaggerated
         COMMAND kami_seven_systems_tests)

add_executable(kami_live_avatar_tests tests/live_avatar_tests.cpp)
target_link_libraries(kami_live_avatar_tests PRIVATE kami_core)
add_test(NAME KamiSpawnAnchoredDX12AvatarPlaceholderTracking
         COMMAND kami_live_avatar_tests)

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

add_executable(kami_lludp_codec_tests tests/lludp_tests.cpp)
target_link_libraries(kami_lludp_codec_tests PRIVATE kami_core)
add_test(NAME KamiSLUDPWireEnvelopeAndAckCodec COMMAND kami_lludp_codec_tests)

add_executable(kami_sl_login_tests tests/login_tests.cpp)
target_link_libraries(kami_sl_login_tests PRIVATE kami_core)
add_test(NAME KamiSLXMLRPCLoginAndSecurityValidation COMMAND kami_sl_login_tests)

add_executable(kami_stream_scheduler_tests tests/load_scheduler_tests.cpp)
target_link_libraries(kami_stream_scheduler_tests PRIVATE kami_core)
add_test(NAME KamiVisibleFirstAssetsAndGPUTimingBudget COMMAND kami_stream_scheduler_tests)

add_executable(kami_region_dx12_tests tests/region_geometry_tests.cpp)
target_link_libraries(kami_region_dx12_tests PRIVATE kami_core)
add_test(NAME KamiVerifiedRegionGeometryDX12OnlyBoundary COMMAND kami_region_dx12_tests)

add_executable(kami_sim_wire_tests tests/simulator_wire_tests.cpp)
target_link_libraries(kami_sim_wire_tests PRIVATE kami_core)
add_test(NAME KamiSimulatorUDPStartupWireCodec COMMAND kami_sim_wire_tests)

add_executable(kami_sim_loopback_tests tests/simulator_transport_tests.cpp)
target_link_libraries(kami_sim_loopback_tests PRIVATE kami_core)
add_test(NAME KamiSimulatorWinSockLoopbackHandshake COMMAND kami_sim_loopback_tests)
set_tests_properties(KamiSimulatorWinSockLoopbackHandshake PROPERTIES TIMEOUT 12)

add_executable(kami_unified_avatar_intake_tests tests/avatar_appearance_tests.cpp)
target_link_libraries(kami_unified_avatar_intake_tests PRIVATE kami_core)
add_test(NAME KamiR30AvatarAppearancePacketValidation
         COMMAND kami_unified_avatar_intake_tests)

add_executable(kami_unified_nearby_chat_tests tests/live_chat_tests.cpp)
target_link_libraries(kami_unified_nearby_chat_tests PRIVATE kami_core)
add_test(NAME KamiR31SLNearbyChatCodecAndSafety
         COMMAND kami_unified_nearby_chat_tests)

add_executable(kami_r34_mesh_lod_tests tests/mesh_lod_tests.cpp)
target_link_libraries(kami_r34_mesh_lod_tests PRIVATE kami_core)
add_test(NAME KamiR34VerifiedBinaryLLSDMeshLoDTriangles
         COMMAND kami_r34_mesh_lod_tests)

add_executable(kami_mesh_ref_tests tests/mesh_reference_tests.cpp)
target_link_libraries(kami_mesh_ref_tests PRIVATE kami_core)
add_test(NAME KamiR33RealMeshExtraParamsIdentification COMMAND kami_mesh_ref_tests)

add_executable(kami_caps_tests tests/caps_tests.cpp)
target_link_libraries(kami_caps_tests PRIVATE kami_core)
add_test(NAME KamiSLSeedCapsAndEventQueueLLSDValidation COMMAND kami_caps_tests)

add_executable(kami_r21_live_water_tests tests/live_masked_water_tests.cpp)
target_link_libraries(kami_r21_live_water_tests PRIVATE kami_core)
add_test(NAME KamiR21MaskWaterToVerifiedSubmergedSLTerrain
         COMMAND kami_r21_live_water_tests)

add_executable(kami_r20_graphics_tests tests/r20_graphics_tests.cpp)
target_link_libraries(kami_r20_graphics_tests PRIVATE kami_core)
add_test(NAME KamiR20LiveTerrainPaletteRegionWaterAndDX12SkyShaders
         COMMAND kami_r20_graphics_tests)

add_executable(kami_object_spatial_full_tests tests/object_spatial_full_tests.cpp)
target_link_libraries(kami_object_spatial_full_tests PRIVATE kami_core)
add_test(NAME KamiR23RealSLFullObjectUpdateTransforms
         COMMAND kami_object_spatial_full_tests)

add_executable(kami_linked_primitives_tests tests/live_primitive_geometry_tests.cpp)
target_link_libraries(kami_linked_primitives_tests PRIVATE kami_core)
add_test(NAME KamiVerifiedLinkedPrimShapesFiniteGeometry
         COMMAND kami_linked_primitives_tests)

add_executable(kami_object_spatial_tests tests/object_spatial_tests.cpp)
target_link_libraries(kami_object_spatial_tests PRIVATE kami_core)
add_test(NAME KamiRealSLRootObjectSpatialFramesSafeDX12
         COMMAND kami_object_spatial_tests)

add_executable(kami_world_wire_tests tests/world_wire_tests.cpp)
target_link_libraries(kami_world_wire_tests PRIVATE kami_core)
add_test(NAME KamiR8RealWorldPacketFraming COMMAND kami_world_wire_tests)

add_executable(kami_land_dct_tests tests/terrain_tests.cpp)
target_link_libraries(kami_land_dct_tests PRIVATE kami_core)
add_test(NAME KamiVerifiedLandPatchDCTCodec COMMAND kami_land_dct_tests)

add_executable(kami_live_terrain_seam_tests tests/live_terrain_tests.cpp)
target_link_libraries(kami_live_terrain_seam_tests PRIVATE kami_core)
add_test(NAME KamiSLTerrainPatchStitchAndNoInventedHeights
    COMMAND kami_live_terrain_seam_tests)
