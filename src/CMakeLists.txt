file(GLOB_RECURSE LUCID_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
list(FILTER LUCID_CORE_SOURCES EXCLUDE REGEX "/main\\.cpp$")

if(APPLE)
    list(APPEND LUCID_CORE_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/app/PlatformApp.mm")
endif()

set(LUCID_SHADERS
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_rect.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_rect.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_text.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/ui_text.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/fullscreen.vert"
    "${PROJECT_SOURCE_DIR}/assets/shaders/blur.frag"
    "${PROJECT_SOURCE_DIR}/assets/shaders/welcome.frag")

lucid_compile_shaders(LUCID_SHADER_HEADERS ${LUCID_SHADERS})

add_custom_target(lucid_shaders ALL DEPENDS ${LUCID_SHADER_HEADERS})

add_library(lucid_core STATIC ${LUCID_CORE_SOURCES})

add_dependencies(lucid_core lucid_shaders)

target_include_directories(lucid_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(lucid_core PRIVATE
    "${CMAKE_BINARY_DIR}/generated")

target_include_directories(lucid_core SYSTEM PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/stb")

target_link_libraries(lucid_core PUBLIC
    glfw
    Vulkan::Vulkan
    Threads::Threads)

if(UNIX AND NOT APPLE)
    target_link_libraries(lucid_core PUBLIC util)
endif()

if(WIN32)
    target_link_libraries(lucid_core PUBLIC dwmapi)
endif()

if(APPLE)
    target_link_libraries(lucid_core PUBLIC
        "-framework Cocoa"
        "-framework Metal"
        "-framework QuartzCore"
        "-framework IOKit"
        "-framework CoreVideo"
        "-framework CoreGraphics")
endif()

lucid_set_warnings(lucid_core)

if(APPLE)
    set(LUCID_BUNDLE MACOSX_BUNDLE)
else()
    set(LUCID_BUNDLE)
endif()

add_executable(lucid ${LUCID_BUNDLE} "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(lucid PRIVATE lucid_core)
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
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "${LUCID_ASSET_DESTINATION}"
    COMMENT "Copying Lucid assets")

if(APPLE)
    find_program(LUCID_CODESIGN codesign)
    if(LUCID_CODESIGN)
        add_custom_command(TARGET lucid POST_BUILD
            COMMAND "${LUCID_CODESIGN}" --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:lucid>"
            COMMENT "Ad-hoc signing Lucid.app")
    endif()
endif()

install(TARGETS lucid
    BUNDLE DESTINATION .
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(APPLE)
    install(CODE [[
        if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Lucid.app")
            execute_process(COMMAND /usr/bin/codesign --force --sign - --timestamp=none
                "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/Lucid.app")
        endif()
    ]])
endif()

if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/dev.lucid.editor.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/linux/dev.lucid.editor.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps"
        RENAME "dev.lucid.editor.png")
endif()
