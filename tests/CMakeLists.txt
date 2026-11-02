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
