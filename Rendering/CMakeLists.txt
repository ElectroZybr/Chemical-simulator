file(GLOB RENDERING_ROOT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB RENDERING_2D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/2d/*.cpp"
)
file(GLOB RENDERING_3D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/3d/*.cpp"
)
file(GLOB RENDERING_CAMERA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/camera/*.cpp"
)
file(GLOB RENDERING_API_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/api/*.cpp"
)

set(RENDERING_SOURCES
    ${RENDERING_ROOT_SOURCES}
    ${RENDERING_2D_SOURCES}
    ${RENDERING_3D_SOURCES}
    ${RENDERING_CAMERA_SOURCES}
    ${RENDERING_API_SOURCES}
)

set(RENDERING_PUBLIC_HEADERS
    BaseRenderer.h
    RenderData.h
    RenderMath.h
    RendererWGPU.h
    WGPUContext.h
    2d/Renderer2DWGPU.h
    3d/Renderer3DWGPU.h
    camera/Camera.h
)

if(APPLE)
    list(APPEND RENDERING_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/MetalBackend.mm")
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
    Lattice
    glfw
    glm::glm
    webgpu
)

if(BUILD_TESTING AND BUILD_RENDERING_TESTS)
    add_subdirectory(tests)
endif()
