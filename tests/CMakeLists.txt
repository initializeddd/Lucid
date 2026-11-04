add_executable(lucid_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/core/version_test.cpp")

target_link_libraries(lucid_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_tests)

add_executable(lucid_ui_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/search_bar_test.cpp")

target_link_libraries(lucid_ui_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_ui_tests)

add_executable(lucid_welcome_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/welcome_test.cpp")

target_link_libraries(lucid_welcome_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_welcome_tests)

add_executable(lucid_overlay_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/overlay_test.cpp")

target_link_libraries(lucid_overlay_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_overlay_tests)

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
target_link_libraries(lucid_modules_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_modules_tests)
add_test(NAME workspace.modules COMMAND lucid_modules_tests)

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
target_link_libraries(lucid_mesh_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_mesh_tests)
add_test(NAME renderer.mesh COMMAND lucid_mesh_tests "${PROJECT_SOURCE_DIR}/assets/backgrounds/city-transit" "${PROJECT_SOURCE_DIR}/assets/backgrounds/lunar-horizon" "${PROJECT_SOURCE_DIR}/assets/backgrounds/mountain-parallax" "${PROJECT_SOURCE_DIR}/assets/backgrounds/ocean-lighthouse" "${PROJECT_SOURCE_DIR}/assets/backgrounds/rainy-street" "${PROJECT_SOURCE_DIR}/assets/backgrounds/orbital-station" "${PROJECT_SOURCE_DIR}/assets/backgrounds/forest-fireflies" "${PROJECT_SOURCE_DIR}/assets/backgrounds/drafting-grid" "${PROJECT_SOURCE_DIR}/assets/backgrounds/retro-landscape")

add_executable(lucid_scene_camera_tests renderer/camera_test.cpp)
target_link_libraries(lucid_scene_camera_tests PRIVATE lucid_core)
lucid_set_warnings(lucid_scene_camera_tests)
add_test(NAME renderer.scene_camera COMMAND lucid_scene_camera_tests)
