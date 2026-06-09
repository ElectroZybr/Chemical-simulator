file(GLOB_RECURSE RENDERING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE RENDERING_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

list(FILTER RENDERING_SOURCES EXCLUDE REGEX "/tests/")
list(FILTER RENDERING_PUBLIC_HEADERS EXCLUDE REGEX "/tests/")

if(APPLE)
    file(GLOB_RECURSE RENDERING_OBJCXX_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.mm"
    )
    list(FILTER RENDERING_OBJCXX_SOURCES EXCLUDE REGEX "/tests/")
    list(APPEND RENDERING_SOURCES ${RENDERING_OBJCXX_SOURCES})
endif()

add_library(latticelab_rendering STATIC
    ${RENDERING_SOURCES}
    ${RENDERING_PUBLIC_HEADERS}
)

add_library(LatticeLab::Rendering ALIAS latticelab_rendering)
add_dependencies(latticelab_rendering wgsl_shaders)

target_compile_options(latticelab_rendering PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
)

target_compile_definitions(latticelab_rendering PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_include_directories(latticelab_rendering PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GENERATED_ROOT}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imguifiledialog_SOURCE_DIR}
    ${webgpu_cpp_SOURCE_DIR}/wgpu-native
)

if(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(METAL_FRAMEWORK Metal)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)

    target_link_libraries(latticelab_rendering PUBLIC
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        ${COCOA_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
endif()

target_link_libraries(latticelab_rendering PUBLIC
    glfw
    glm::glm
    webgpu
)

if(BUILD_TESTING AND BUILD_RENDERING_TESTS)
    add_subdirectory(tests)
endif()
