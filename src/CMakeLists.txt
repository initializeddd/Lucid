file(GLOB_RECURSE LUCID_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER LUCID_CORE_SOURCES EXCLUDE REGEX "/main\\.cpp$")

if(APPLE)
    list(APPEND LUCID_CORE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/app/PlatformApp.mm"
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/browser/MacWebPreview.mm")
    set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/platform/browser/MacWebPreview.mm"
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

set(LUCID_SHADERS
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_rect.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_rect.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_text.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_text.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_image.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/fullscreen.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/blur.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/background3d.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/background3d.frag")

lucid_compile_shaders(LUCID_SHADER_HEADERS ${LUCID_SHADERS})

add_custom_target(lucid_shaders ALL DEPENDS ${LUCID_SHADER_HEADERS})

add_library(lucid_md4c STATIC "${PROJECT_SOURCE_DIR}/third_party/md4c/md4c.c" "${PROJECT_SOURCE_DIR}/third_party/md4c/entity.c")
target_include_directories(lucid_md4c PUBLIC "${PROJECT_SOURCE_DIR}/third_party/md4c")
add_library(lucid_sqlite STATIC "${PROJECT_SOURCE_DIR}/third_party/sqlite/sqlite3.c")
target_include_directories(lucid_sqlite PUBLIC "${PROJECT_SOURCE_DIR}/third_party/sqlite")
target_compile_definitions(lucid_sqlite PRIVATE SQLITE_OMIT_LOAD_EXTENSION SQLITE_DQS=0)
add_library(lucid_core STATIC ${LUCID_CORE_SOURCES})

if(MSVC)
    target_compile_options(lucid_core PRIVATE /bigobj)
endif()

add_dependencies(lucid_core lucid_shaders)

target_include_directories(lucid_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(lucid_core PRIVATE
    "${CMAKE_BINARY_DIR}/generated")

target_include_directories(lucid_core SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb"
    "${PROJECT_SOURCE_DIR}/third_party/picosha2")

target_link_libraries(lucid_core PUBLIC
    glfw
    Vulkan::Vulkan
    Threads::Threads
    lucid_md4c
    lucid_sqlite
    llama
    libcurl_static)

if(UNIX AND NOT APPLE)
    target_link_libraries(lucid_core PUBLIC util)
endif()

if(WIN32)
    target_link_libraries(lucid_core PUBLIC dwmapi shell32 bcrypt advapi32 setupapi)
endif()

if(LUCID_ENABLE_WEBVIEW2)
    if(NOT WIN32)
        message(FATAL_ERROR "LUCID_ENABLE_WEBVIEW2 is available only for Windows builds")
    endif()
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM};${CMAKE_SYSTEM_PROCESSOR};${MSVC_CXX_ARCHITECTURE_ID}" lucid_webview2_platform)
    if(lucid_webview2_platform MATCHES "arm64|aarch64")
        set(LUCID_WEBVIEW2_ARCH "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(LUCID_WEBVIEW2_ARCH "x86")
    elseif(lucid_webview2_platform MATCHES "x64|amd64|x86_64")
        set(LUCID_WEBVIEW2_ARCH "x64")
    else()
        message(FATAL_ERROR "Cannot determine the Windows WebView2 target architecture")
    endif()
    include(WebView2Sdk)
    lucid_validate_webview2_sdk("${LUCID_WEBVIEW2_SDK_DIR}" "${LUCID_WEBVIEW2_ARCH}")
    target_compile_definitions(lucid_core PRIVATE LUCID_ENABLE_WEBVIEW2=1)
    target_include_directories(lucid_core SYSTEM PRIVATE "${LUCID_WEBVIEW2_SDK_DIR}")
    target_link_libraries(lucid_core PUBLIC ole32 uuid)
endif()

if(LUCID_WEBVIEW2_UI_TEST)
    if(NOT WIN32 OR NOT LUCID_ENABLE_WEBVIEW2)
        message(FATAL_ERROR "LUCID_WEBVIEW2_UI_TEST requires an opt-in Windows WebView2 build")
    endif()
    target_compile_definitions(lucid_core PUBLIC LUCID_WEBVIEW2_UI_TEST=1)
endif()

if(APPLE)
    target_link_libraries(lucid_core PUBLIC
        "-framework Cocoa"
        "-framework WebKit"
        "-framework Security"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreGraphics")
endif()

lucid_set_warnings(lucid_core)

# A separate, supervised process for explicitly approved trusted shader work.
add_executable(lucid_shader_preview_helper
    "${PROJECT_SOURCE_DIR}/tools/shader-preview/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/vulkan/ShaderPreviewPass.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/SpirvInspection.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/modules/ShaderPreviewInterface.cpp")
set_target_properties(lucid_shader_preview_helper PROPERTIES OUTPUT_NAME "lucid-shader-preview-helper")
target_include_directories(lucid_shader_preview_helper PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(lucid_shader_preview_helper PRIVATE Vulkan::Vulkan)
add_dependencies(lucid_shader_preview_helper lucid_shaders)
if(WIN32)
    target_link_libraries(lucid_shader_preview_helper PRIVATE shell32)
endif()
if(UNIX AND NOT APPLE)
    target_link_options(lucid_shader_preview_helper PRIVATE "LINKER:--disable-new-dtags")
    set_target_properties(lucid_shader_preview_helper PROPERTIES BUILD_RPATH "$ORIGIN/lib" INSTALL_RPATH "$ORIGIN/lib")
endif()
lucid_set_warnings(lucid_shader_preview_helper)

if(APPLE)
    set(LUCID_BUNDLE MACOSX_BUNDLE)
else()
    set(LUCID_BUNDLE)
endif()

add_executable(lucid ${LUCID_BUNDLE} "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(lucid PRIVATE lucid_core)
add_dependencies(lucid lucid_shader_preview_helper)
lucid_set_warnings(lucid)

if(WIN32)
    target_sources(lucid PRIVATE "${PROJECT_SOURCE_DIR}/packaging/windows/lucid.rc")
    set_target_properties(lucid PROPERTIES OUTPUT_NAME "Lucid")
endif()

if(UNIX AND NOT APPLE)
    target_link_options(lucid PRIVATE "LINKER:--disable-new-dtags")
    set_target_properties(lucid PROPERTIES
        BUILD_RPATH "$ORIGIN/lib"
        INSTALL_RPATH "$ORIGIN/lib")
endif()

if(APPLE)
    set(LUCID_MACOS_ICON "${PROJECT_SOURCE_DIR}/packaging/macos/Lucid.icns")
    set(LUCID_MACOS_ASSET_CAR "${PROJECT_SOURCE_DIR}/packaging/macos/Assets.car")
    set_source_files_properties("${LUCID_MACOS_ICON}" "${LUCID_MACOS_ASSET_CAR}"
        PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(lucid PRIVATE "${LUCID_MACOS_ICON}" "${LUCID_MACOS_ASSET_CAR}")

    set_target_properties(lucid PROPERTIES
        OUTPUT_NAME "Lucid"
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist"
        MACOSX_BUNDLE_BUNDLE_NAME "Lucid"
        MACOSX_BUNDLE_GUI_IDENTIFIER "dev.lucid.editor"
        MACOSX_BUNDLE_ICON_FILE "Lucid"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2026 Lucid contributors."
        INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(LUCID_ASSET_DESTINATION "$<TARGET_FILE_DIR:lucid>/../Resources/assets")
else()
    set(LUCID_ASSET_DESTINATION "$<TARGET_FILE_DIR:lucid>/assets")
endif()

add_custom_command(TARGET lucid POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LUCID_ASSET_DESTINATION}/backgrounds"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "${LUCID_ASSET_DESTINATION}"
    COMMENT "Copying Lucid assets")

if(WIN32 AND LUCID_ENABLE_WEBVIEW2)
    add_custom_command(TARGET lucid POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LUCID_WEBVIEW2_SDK_DIR}/WebView2Loader.dll" "$<TARGET_FILE_DIR:lucid>/WebView2Loader.dll"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${LUCID_WEBVIEW2_SDK_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:lucid>/WebView2.LICENSE.txt"
        COMMENT "Adding pinned WebView2 loader and license")
    install(FILES "${LUCID_WEBVIEW2_SDK_DIR}/WebView2Loader.dll" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES "${LUCID_WEBVIEW2_SDK_DIR}/LICENSE.txt" DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME "WebView2.LICENSE.txt")
elseif(WIN32)
    add_custom_command(TARGET lucid POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E rm -f "$<TARGET_FILE_DIR:lucid>/WebView2Loader.dll" "$<TARGET_FILE_DIR:lucid>/WebView2.LICENSE.txt"
        COMMENT "Removing disabled WebView2 test loader")
endif()

if(APPLE)
    add_custom_command(TARGET lucid POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:lucid_shader_preview_helper>" "$<TARGET_FILE_DIR:lucid>/lucid-shader-preview-helper"
        COMMENT "Adding shader preview helper")
endif()

if(APPLE)
    find_program(LUCID_CODESIGN codesign)
    if(LUCID_CODESIGN)
        add_custom_command(TARGET lucid POST_BUILD
            COMMAND "${LUCID_CODESIGN}" --force --sign - --timestamp=none "$<TARGET_FILE_DIR:lucid>/lucid-shader-preview-helper"
            COMMAND "${LUCID_CODESIGN}" --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:lucid>"
            COMMENT "Ad-hoc signing Lucid.app")
    endif()
endif()

# The executable's POST_BUILD copy handles an initial or explicitly requested
# `lucid` build. Track assets separately as well so editing a model updates an
# already-built application even when the executable itself does not relink.
file(GLOB_RECURSE LUCID_ASSET_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/assets/*")
set(LUCID_ASSET_STAMP "${CMAKE_CURRENT_BINARY_DIR}/lucid-assets-$<CONFIG>.stamp")
if(APPLE AND LUCID_CODESIGN)
    add_custom_command(OUTPUT "${LUCID_ASSET_STAMP}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LUCID_ASSET_DESTINATION}/backgrounds"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${LUCID_ASSET_DESTINATION}"
        COMMAND "${LUCID_CODESIGN}" --force --sign - --timestamp=none "$<TARGET_BUNDLE_DIR:lucid>"
        COMMAND "${CMAKE_COMMAND}" -E touch "${LUCID_ASSET_STAMP}"
        DEPENDS lucid ${LUCID_ASSET_FILES}
        COMMENT "Updating and signing Lucid assets")
else()
    add_custom_command(OUTPUT "${LUCID_ASSET_STAMP}"
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${LUCID_ASSET_DESTINATION}/backgrounds"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/assets" "${LUCID_ASSET_DESTINATION}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${LUCID_ASSET_STAMP}"
        DEPENDS lucid ${LUCID_ASSET_FILES}
        COMMENT "Updating Lucid assets")
endif()
add_custom_target(lucid_assets ALL DEPENDS "${LUCID_ASSET_STAMP}")

install(TARGETS lucid
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "${PROJECT_SOURCE_DIR}/third_party/sha2/LICENSE" DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/lucid" RENAME "sha2-BSD-3-Clause.txt")
if(NOT APPLE)
    install(TARGETS lucid_shader_preview_helper RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(APPLE)
    install(CODE [[
        if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Lucid.app")
            execute_process(COMMAND /usr/bin/codesign --force --sign - --timestamp=none
                "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Lucid.app")
        endif()
    ]])
endif()

if(NOT APPLE)
    install(DIRECTORY "${PROJECT_SOURCE_DIR}/assets" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/dev.lucid.editor.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/dev.lucid.editor.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
        RENAME "dev.lucid.editor.png")
endif()
