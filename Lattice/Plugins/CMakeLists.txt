include(CheckIPOSupported)

function(lattice_add_plugin plugin_name)
    set(plugin_dir "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_name}")

    file(GLOB_RECURSE plugin_sources CONFIGURE_DEPENDS
        "${plugin_dir}/*.cpp"
    )

    file(GLOB_RECURSE plugin_headers CONFIGURE_DEPENDS
        "${plugin_dir}/*.h"
    )

    foreach(plugin_file_list IN ITEMS plugin_sources plugin_headers)
        list(FILTER ${plugin_file_list} EXCLUDE REGEX ".*/tests/.*")
    endforeach()

    if(NOT plugin_sources)
        return()
    endif()

    set(target_name "Lattice${plugin_name}")

    add_library(${target_name} SHARED
        ${plugin_sources}
        ${plugin_headers}
    )

    set_target_properties(${target_name} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Plugins"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Plugins"
    )

    target_include_directories(${target_name} PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${plugin_dir}
    )

    target_compile_options(${target_name} PRIVATE
        $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    )

    if(ENABLE_IPO)
        check_ipo_supported(RESULT plugin_lto_supported OUTPUT plugin_lto_error)
        if(plugin_lto_supported AND NOT MINGW)
            enable_non_debug_ipo(${target_name})
        endif()
    endif()

    target_link_libraries(${target_name} PRIVATE
        Lattice
        glm::glm
    )

    if(TARGET TBB::tbb)
        target_link_libraries(${target_name} PRIVATE TBB::tbb)
        target_compile_definitions(${target_name} PRIVATE ENABLE_TBB)
    endif()

    set_property(GLOBAL APPEND PROPERTY LATTICE_PLUGIN_TARGETS ${target_name})
endfunction()

file(GLOB plugin_dirs RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/*")

foreach(plugin_dir IN LISTS plugin_dirs)

    if(plugin_dir STREQUAL "RandomTestPlugin")
        continue()
    endif()

    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_dir}")
        lattice_add_plugin("${plugin_dir}")
    endif()

endforeach()

get_property(lattice_plugin_targets GLOBAL PROPERTY LATTICE_PLUGIN_TARGETS)
if(lattice_plugin_targets)
    add_custom_target(Lattice-plugins DEPENDS ${lattice_plugin_targets})
endif()
