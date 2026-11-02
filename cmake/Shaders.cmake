include_guard(GLOBAL)

set(LUCID_SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/shaders")
file(MAKE_DIRECTORY "${LUCID_SHADER_OUTPUT_DIR}")

if(Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
    set(LUCID_GLSLANG_VALIDATOR "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}")
else()
    find_program(LUCID_GLSLANG_VALIDATOR NAMES glslangValidator
        HINTS "$ENV{VULKAN_SDK}/bin")
endif()

if(NOT LUCID_GLSLANG_VALIDATOR)
    message(FATAL_ERROR
        "glslangValidator was not found. Install the Vulkan SDK or shader tools.")
endif()

function(lucid_compile_shaders out_var)
    set(_headers)

    foreach(_shader IN LISTS ARGN)
        get_filename_component(_name "${_shader}" NAME)
        string(REPLACE "." "_" _variable "${_name}")
        set(_output "${LUCID_SHADER_OUTPUT_DIR}/${_name}.h")

        add_custom_command(
            OUTPUT "${_output}"
            COMMAND "${LUCID_GLSLANG_VALIDATOR}"
                -V
                --target-env vulkan1.2
                --vn "${_variable}_spv"
                -o "${_output}"
                "${_shader}"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader ${_name}"
            VERBATIM)

        list(APPEND _headers "${_output}")
    endforeach()

    set(${out_var} "${_headers}" PARENT_SCOPE)
endfunction()
