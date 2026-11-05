add_executable(lucid_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/core/version_test.cpp")

target_link_libraries(lucid_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_tests)

add_executable(lucid_ui_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/search_bar_test.cpp")

target_link_libraries(lucid_ui_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_ui_tests)

add_executable(lucid_utf8_tests text/utf8_test.cpp)
target_link_libraries(lucid_utf8_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_utf8_tests)
add_test(NAME text.utf8 COMMAND lucid_utf8_tests)

add_executable(lucid_welcome_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/welcome_test.cpp")

target_link_libraries(lucid_welcome_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_welcome_tests)

add_executable(lucid_overlay_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/overlay_test.cpp")

target_link_libraries(lucid_overlay_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_overlay_tests)

add_executable(lucid_visual_review_preview_tests ui/visual_review_preview_test.cpp)
target_link_libraries(lucid_visual_review_preview_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_visual_review_preview_tests)
add_test(NAME ui.visual_review_preview COMMAND lucid_visual_review_preview_tests)

add_executable(lucid_windows_web_preview_router_tests platform/windows_web_preview_router_test.cpp)
target_link_libraries(lucid_windows_web_preview_router_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_windows_web_preview_router_tests)
add_test(NAME platform.windows_web_preview_router COMMAND lucid_windows_web_preview_router_tests)
foreach(web_preview_test IN ITEMS windows_web_preview windows_web_preview_session windows_web_preview_host glfw_web_preview_bridge web_preview_capture_gate webview2_loader_policy web_preview_context_gate guarded_glfw_web_preview web_preview_presentation web_preview_module_fingerprint)
    add_executable(lucid_${web_preview_test}_tests "platform/${web_preview_test}_test.cpp")
    target_link_libraries(lucid_${web_preview_test}_tests PRIVATE lucid_core)
    lucid_set_warnings(lucid_${web_preview_test}_tests)
    add_test(NAME platform.${web_preview_test} COMMAND lucid_${web_preview_test}_tests)
endforeach()

# Real WebView2 capture needs an interactive Windows desktop, installed runtime,
# explicit environment opt-in and a trusted loader; never run it in headless CI.
if(WIN32 AND LUCID_ENABLE_WEBVIEW2)
    foreach(web_preview_smoke IN ITEMS windows_web_preview windows_web_preview_session windows_web_preview_host glfw_web_preview_bridge)
        add_executable(lucid_${web_preview_smoke}_smoke_test EXCLUDE_FROM_ALL "platform/${web_preview_smoke}_smoke_test.cpp")
        target_link_libraries(lucid_${web_preview_smoke}_smoke_test PRIVATE lucid_core)
        lucid_set_warnings(lucid_${web_preview_smoke}_smoke_test)
        if(web_preview_smoke STREQUAL "windows_web_preview_host" OR web_preview_smoke STREQUAL "glfw_web_preview_bridge")
            add_custom_command(TARGET lucid_${web_preview_smoke}_smoke_test POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LUCID_WEBVIEW2_SDK_DIR}/WebView2Loader.dll" "$<TARGET_FILE_DIR:lucid_${web_preview_smoke}_smoke_test>/WebView2Loader.dll"
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LUCID_WEBVIEW2_SDK_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:lucid_${web_preview_smoke}_smoke_test>/WebView2.LICENSE.txt")
        endif()
    endforeach()
    add_executable(lucid_windows_web_preview_security_smoke_test EXCLUDE_FROM_ALL platform/windows_web_preview_security_smoke_test.cpp)
    target_link_libraries(lucid_windows_web_preview_security_smoke_test PRIVATE lucid_core ws2_32 windowscodecs)
    lucid_set_warnings(lucid_windows_web_preview_security_smoke_test)
endif()

# WebKit requires a graphical macOS session. Build and run this opt-in target
# explicitly rather than making a headless CI run depend on WindowServer.
if(APPLE)
    add_executable(lucid_mac_web_preview_test EXCLUDE_FROM_ALL platform/mac_web_preview_test.mm)
    target_compile_options(lucid_mac_web_preview_test PRIVATE -fobjc-arc)
    target_link_libraries(lucid_mac_web_preview_test PRIVATE lucid_core)
    lucid_set_warnings(lucid_mac_web_preview_test)
endif()

add_executable(lucid_editor_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/document_test.cpp")

target_link_libraries(lucid_editor_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_editor_tests)

add_executable(lucid_syntax_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/syntax_test.cpp")

target_link_libraries(lucid_syntax_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_syntax_tests)

add_executable(lucid_diagnostics_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/diagnostics_test.cpp")

target_link_libraries(lucid_diagnostics_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_diagnostics_tests)

add_executable(lucid_project_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/workspace/project_test.cpp")

target_link_libraries(lucid_project_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_project_tests)

add_executable(lucid_path_search_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/workspace/path_search_test.cpp")

target_link_libraries(lucid_path_search_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_path_search_tests)

add_executable(lucid_function_index_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/workspace/function_index_test.cpp")

target_link_libraries(lucid_function_index_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_function_index_tests)

add_test(NAME core.version COMMAND lucid_tests)
add_test(NAME ui.search_bar COMMAND lucid_ui_tests)
add_test(NAME ui.welcome COMMAND lucid_welcome_tests)
add_test(NAME ui.overlay COMMAND lucid_overlay_tests)
add_test(NAME editor.document COMMAND lucid_editor_tests)
add_test(NAME editor.syntax COMMAND lucid_syntax_tests)
add_test(NAME editor.diagnostics COMMAND lucid_diagnostics_tests)
add_test(NAME workspace.project COMMAND lucid_project_tests)
add_test(NAME workspace.path_search COMMAND lucid_path_search_tests)
add_test(NAME workspace.function_index COMMAND lucid_function_index_tests)

add_executable(lucid_json_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/json_test.cpp")

target_link_libraries(lucid_json_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_json_tests)

add_executable(lucid_compiler_diagnostics_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/compiler_diagnostics_test.cpp")

target_link_libraries(lucid_compiler_diagnostics_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_compiler_diagnostics_tests)

add_executable(lucid_settings_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/app/settings_test.cpp")

target_link_libraries(lucid_settings_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_settings_tests)

add_test(NAME editor.json COMMAND lucid_json_tests)
add_test(NAME editor.compiler_diagnostics COMMAND lucid_compiler_diagnostics_tests)
add_test(NAME app.settings COMMAND lucid_settings_tests)

add_executable(lucid_compile_script_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/compile_script_test.cpp")

target_link_libraries(lucid_compile_script_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_compile_script_tests)

add_executable(lucid_pty_process_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/platform/pty_process_test.cpp")

target_link_libraries(lucid_pty_process_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_pty_process_tests)

add_test(NAME editor.compile_script COMMAND lucid_compile_script_tests)
add_test(NAME platform.pty_process COMMAND lucid_pty_process_tests)

add_executable(lucid_ai_tests ai/model_test.cpp)
target_link_libraries(lucid_ai_tests PRIVATE lucid_core)
add_test(NAME ai.model COMMAND lucid_ai_tests)

add_executable(lucid_completion_tests editor/completion_test.cpp)
target_link_libraries(lucid_completion_tests PRIVATE lucid_core)
add_test(NAME editor.completion COMMAND lucid_completion_tests)

# This test opens a native Vulkan window and is run explicitly on a desktop.
add_executable(lucid_interaction_tests EXCLUDE_FROM_ALL app/interaction_test.cpp)
target_link_libraries(lucid_interaction_tests PRIVATE lucid_core)
add_executable(lucid_ai_scroll_tests EXCLUDE_FROM_ALL app/ai_scroll_test.cpp)
target_link_libraries(lucid_ai_scroll_tests PRIVATE lucid_core)
if(APPLE)
    target_sources(lucid_interaction_tests PRIVATE app/macos_menu_test.mm)
endif()

add_executable(lucid_text_search_tests editor/text_search_test.cpp)
target_link_libraries(lucid_text_search_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_text_search_tests)
add_test(NAME editor.text_search COMMAND lucid_text_search_tests)

add_executable(lucid_app_icon_tests platform/app_icon_test.cpp)
target_link_libraries(lucid_app_icon_tests PRIVATE lucid_core)
add_test(NAME platform.app_icon COMMAND lucid_app_icon_tests)

add_executable(lucid_tasks_tests workspace/tasks_test.cpp)
target_link_libraries(lucid_tasks_tests PRIVATE lucid_core)
add_test(NAME workspace.tasks COMMAND lucid_tasks_tests)

add_executable(lucid_git_review_tests workspace/git_review_test.cpp)
target_link_libraries(lucid_git_review_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_git_review_tests)
add_test(NAME workspace.git_review COMMAND lucid_git_review_tests)

add_executable(lucid_shared_document_tests workspace/shared_document_test.cpp)
target_link_libraries(lucid_shared_document_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_shared_document_tests)
add_test(NAME workspace.shared_document COMMAND lucid_shared_document_tests)

add_executable(lucid_modules_tests workspace/modules_test.cpp)
add_executable(lucid_cmake_model_tests workspace/cmake_model_test.cpp)
target_link_libraries(lucid_cmake_model_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_cmake_model_tests)
add_test(NAME workspace.cmake_model COMMAND lucid_cmake_model_tests)
add_executable(lucid_coverage_summary_tests workspace/coverage_summary_test.cpp)
target_link_libraries(lucid_coverage_summary_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_coverage_summary_tests)
add_test(NAME workspace.coverage_summary COMMAND lucid_coverage_summary_tests)
add_executable(lucid_cobertura_security_tests workspace/cobertura_security_test.cpp)
target_link_libraries(lucid_cobertura_security_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_cobertura_security_tests)
add_test(NAME workspace.cobertura_security COMMAND lucid_cobertura_security_tests)
target_link_libraries(lucid_modules_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_modules_tests)
add_test(NAME workspace.modules COMMAND lucid_modules_tests)
add_executable(lucid_shipped_module_manifests_tests workspace/shipped_module_manifests_test.cpp)
target_link_libraries(lucid_shipped_module_manifests_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_shipped_module_manifests_tests)
add_test(NAME workspace.shipped_module_manifests COMMAND lucid_shipped_module_manifests_tests "${PROJECT_SOURCE_DIR}/modules")
add_executable(lucid_shipped_catalog_contract_tests workspace/shipped_catalog_contract_test.cpp)
target_link_libraries(lucid_shipped_catalog_contract_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_shipped_catalog_contract_tests)
add_executable(lucid_registry_security_tests workspace/registry_security_test.cpp)
target_link_libraries(lucid_registry_security_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_registry_security_tests)
add_test(NAME workspace.registry_security COMMAND lucid_registry_security_tests)
foreach(module_test IN ITEMS coverage_run cobertura_branches shader_preview_interface shader_preview_helper spirv_inspection shader_includes github_review_submit api_response_export pe_symbols secret_scan cmake_cache_edit cmake_cache_values cmake_build binary_inspector python_environments benchmark_comparison log_correlation localization_workspace localization_edit design_tokens design_token_aliases release_notes junit_import test_runner_plus dependency_risk dependency_artifact dependency_go_zip shader_diagnostics api_requests api_preflight api_assertions api_collection_edit sql_workbench sql_changes sql_row_diff sql_schema module_process python_preference python_runtime ctest_runner ctest_history serial_devices serial_monitor hex_buffer hex_file visual_comparison visual_crop visual_mask_selection visual_config_edit linker_map container_inspection container_actions board_identification usb_descriptors svd_registers crash_dump unix_crash_report apple_crash_report apple_ips_report apple_ips_symbols crash_symbols breakpad_symbols pull_request_data pull_request_patch github_pull_requests rust_ownership rust_diagnostic_run diagram_preview board_profiles toolchain_profiles toolchain_targets target_configure markdown_outline external_links web_preview_config accessibility_audit)
    add_executable(lucid_${module_test}_tests workspace/${module_test}_test.cpp)
    target_link_libraries(lucid_${module_test}_tests PRIVATE lucid_core)
    lucid_set_warnings(lucid_${module_test}_tests)
    if(module_test STREQUAL "ctest_runner")
        add_test(NAME workspace.${module_test} COMMAND lucid_${module_test}_tests "${CMAKE_CTEST_COMMAND}")
    else()
        add_test(NAME workspace.${module_test} COMMAND lucid_${module_test}_tests)
    endif()
endforeach()
find_program(LUCID_TEST_PYTHON NAMES python3 python)
if(LUCID_TEST_PYTHON)
    add_test(NAME packaging.sha2_notice COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/sha2_notice_test.py")
    add_test(NAME packaging.webview2_sdk COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/webview2_sdk_test.py" "${CMAKE_COMMAND}")
    add_test(NAME packaging.webview2_smoke_contract COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/webview2_smoke_contract_test.py")
    add_test(NAME workspace.shipped_catalog_contract COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/catalog_contract_test.py"
        "$<TARGET_FILE:lucid_shipped_catalog_contract_tests>" "${PROJECT_SOURCE_DIR}")
    add_test(NAME workspace.shipped_module_manifest_negative COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/shipped_module_contract_test.py"
        "$<TARGET_FILE:lucid_shipped_module_manifests_tests>" "${PROJECT_SOURCE_DIR}/modules")
    add_test(NAME workspace.api_transport_loopback COMMAND "${LUCID_TEST_PYTHON}"
        "${CMAKE_CURRENT_SOURCE_DIR}/workspace/api_transport_server_test.py"
        "$<TARGET_FILE:lucid_api_requests_tests>")
endif()
add_executable(lucid_module_live_test EXCLUDE_FROM_ALL workspace/module_live_test.cpp)
target_link_libraries(lucid_module_live_test PRIVATE lucid_core)
lucid_set_warnings(lucid_module_live_test)

add_executable(lucid_recovery_tests workspace/recovery_test.cpp)
target_link_libraries(lucid_recovery_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_recovery_tests)
add_test(NAME workspace.recovery COMMAND lucid_recovery_tests)

add_executable(lucid_debug_protocol_tests editor/debug_protocol_test.cpp)
target_link_libraries(lucid_debug_protocol_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_debug_protocol_tests)
add_test(NAME debug.protocol COMMAND lucid_debug_protocol_tests)

add_executable(lucid_debug_session_tests editor/debug_session_test.cpp)
target_link_libraries(lucid_debug_session_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_debug_session_tests)
add_test(NAME debug.session COMMAND lucid_debug_session_tests)

add_executable(lucid_debug_transport_fixture editor/debug_transport_fixture.cpp)
add_executable(lucid_debug_transport_tests editor/debug_transport_test.cpp)
target_link_libraries(lucid_debug_transport_tests PRIVATE lucid_core)
add_test(NAME debug.transport COMMAND lucid_debug_transport_tests $<TARGET_FILE:lucid_debug_transport_fixture>)
add_executable(lucid_debug_fixture editor/debug_fixture.cpp editor/debug_fixture_helper.cpp)
if(MSVC)
    target_compile_options(lucid_debug_fixture PRIVATE /Od /Zi)
    target_link_options(lucid_debug_fixture PRIVATE /DEBUG /INCREMENTAL:NO)
else()
    target_compile_options(lucid_debug_fixture PRIVATE -g -O0)
endif()
add_executable(lucid_debug_lldb_tests editor/debug_lldb_test.cpp)
target_link_libraries(lucid_debug_lldb_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_debug_lldb_tests)
add_test(NAME debug.lldb_optional COMMAND lucid_debug_lldb_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/editor/debug_fixture.cpp" $<TARGET_FILE:lucid_debug_fixture> "${CMAKE_CURRENT_SOURCE_DIR}/editor/debug_fixture_helper.cpp")

add_executable(lucid_share_link_tests workspace/share_link_test.cpp)
target_link_libraries(lucid_share_link_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_share_link_tests)
add_test(NAME workspace.share_link COMMAND lucid_share_link_tests)

add_executable(lucid_shared_project_tests workspace/shared_project_test.cpp)
target_link_libraries(lucid_shared_project_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_shared_project_tests)
add_test(NAME workspace.shared_project COMMAND lucid_shared_project_tests)

add_executable(lucid_host_session_tests workspace/host_session_test.cpp)
target_link_libraries(lucid_host_session_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_host_session_tests)
add_test(NAME workspace.host_session COMMAND lucid_host_session_tests)

add_executable(lucid_host_server_tests workspace/host_server_test.cpp)
target_link_libraries(lucid_host_server_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_host_server_tests)
add_test(NAME workspace.host_server COMMAND lucid_host_server_tests)

add_executable(lucid_replica_tests workspace/replica_test.cpp)
target_link_libraries(lucid_replica_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_replica_tests)
add_test(NAME workspace.replica COMMAND lucid_replica_tests)

if(UNIX)
    add_test(NAME packaging.installer COMMAND sh "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer_test.sh" "${PROJECT_SOURCE_DIR}/packaging/linux/install.sh")
endif()

add_executable(lucid_lsp_tests editor/lsp_test.cpp)
target_link_libraries(lucid_lsp_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_lsp_tests)
add_test(NAME editor.lsp COMMAND lucid_lsp_tests)

add_executable(lucid_project_search_tests workspace/project_search_test.cpp)
target_link_libraries(lucid_project_search_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_project_search_tests)
add_test(NAME workspace.project_search COMMAND lucid_project_search_tests)

add_executable(lucid_git_remote_tests workspace/git_remote_test.cpp)
target_link_libraries(lucid_git_remote_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_git_remote_tests)
add_test(NAME workspace.git_remote COMMAND lucid_git_remote_tests)

add_executable(lucid_session_tests workspace/session_test.cpp)
target_link_libraries(lucid_session_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_session_tests)
add_test(NAME workspace.session COMMAND lucid_session_tests)

add_executable(lucid_lsp_edits_tests editor/lsp_edits_test.cpp)
target_link_libraries(lucid_lsp_edits_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_lsp_edits_tests)
add_test(NAME editor.lsp_edits COMMAND lucid_lsp_edits_tests)

add_executable(lucid_mesh_tests renderer/mesh_test.cpp)
add_executable(lucid_texture_size_tests renderer/texture_size_test.cpp)
target_link_libraries(lucid_texture_size_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_texture_size_tests)
add_test(NAME renderer.texture_size COMMAND lucid_texture_size_tests)
target_link_libraries(lucid_mesh_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_mesh_tests)
add_test(NAME renderer.mesh COMMAND lucid_mesh_tests "${PROJECT_SOURCE_DIR}/assets/backgrounds/city-transit" "${PROJECT_SOURCE_DIR}/assets/backgrounds/lunar-horizon" "${PROJECT_SOURCE_DIR}/assets/backgrounds/mountain-parallax" "${PROJECT_SOURCE_DIR}/assets/backgrounds/ocean-lighthouse" "${PROJECT_SOURCE_DIR}/assets/backgrounds/orbital-station" "${PROJECT_SOURCE_DIR}/assets/backgrounds/forest-fireflies")

add_executable(lucid_scene_camera_tests renderer/camera_test.cpp)
target_link_libraries(lucid_scene_camera_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_scene_camera_tests)
add_test(NAME renderer.scene_camera COMMAND lucid_scene_camera_tests)
