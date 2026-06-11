set(WGSL_SHADER_SRC "${CMAKE_SOURCE_DIR}/Rendering/shaders")
set(WGSL_SHADER_OUT "${GENERATED_DIR}/shaders")
file(MAKE_DIRECTORY "${WGSL_SHADER_OUT}")

function(compile_wgsl_to_header WGSL_FILE OUT_HEADERS_VAR)
    get_filename_component(NAME ${WGSL_FILE} NAME_WE)
    
    set(H_FILE "${WGSL_SHADER_OUT}/${NAME}.wgsl.h")
    set(VAR_NAME "${NAME}WGSL")

    add_custom_command(
        OUTPUT ${H_FILE}
        COMMAND ${CMAKE_COMMAND} 
            -DINPUT_FILE=${WGSL_FILE} 
            -DOUTPUT_FILE=${H_FILE} 
            -DVAR_NAME=${VAR_NAME}
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/wsgl_to_h.cmake"
        DEPENDS ${WGSL_FILE}
        COMMENT "Generating header for ${NAME} WGSL"
        VERBATIM
    )

    set(${OUT_HEADERS_VAR} ${${OUT_HEADERS_VAR}} ${H_FILE} PARENT_SCOPE)
endfunction()

file(GLOB WGSL_SOURCES CONFIGURE_DEPENDS "${WGSL_SHADER_SRC}/*.wgsl")
set(WGSL_HEADERS "")

foreach(FILE ${WGSL_SOURCES})
    compile_wgsl_to_header(${FILE} WGSL_HEADERS)
endforeach()

add_custom_target(wgsl_shaders ALL DEPENDS ${WGSL_HEADERS})
