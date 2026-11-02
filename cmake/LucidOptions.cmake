include_guard(GLOBAL)

option(LUCID_ENABLE_VALIDATION "Enable Vulkan validation layers" ON)
option(LUCID_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LUCID_BUILD_TESTS "Build Lucid's test suite" ON)

if(LUCID_ENABLE_VALIDATION)
    add_compile_definitions(LUCID_ENABLE_VALIDATION=1)
else()
    add_compile_definitions(LUCID_ENABLE_VALIDATION=0)
endif()

add_compile_definitions($<$<CONFIG:Debug>:LUCID_DEBUG=1>)
