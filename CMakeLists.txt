cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0069 NEW)

# CMake 4.x: keep third-party dependencies on a supported policy floor.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)

project(LatticeLab VERSION 0.1.3 LANGUAGES C CXX)

set(APP_NAME "${PROJECT_NAME}")
set(APP_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(APP_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(APP_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(APP_VERSION_STRING "${PROJECT_VERSION}")
set(APP_VERSION_BASE "${PROJECT_VERSION}")
set(APP_VERSION_SUFFIX "")
set(APP_GIT_COMMIT "unknown")
option(APP_RELEASE_BUILD "Build application with clean release version string" OFF)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE APP_GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE APP_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE APP_GIT_TAG_RESULT
    )

    if(NOT APP_RELEASE_BUILD AND (NOT APP_GIT_TAG_RESULT EQUAL 0 OR NOT APP_GIT_TAG STREQUAL "v${APP_VERSION_BASE}"))
        set(APP_VERSION_SUFFIX "+g${APP_GIT_COMMIT}")
    endif()
elseif(NOT APP_RELEASE_BUILD)
    set(APP_VERSION_SUFFIX "+g${APP_GIT_COMMIT}")
endif()

set(APP_VERSION_STRING "${APP_VERSION_BASE}${APP_VERSION_SUFFIX}")

option(OPTIMIZE_FOR_NATIVE "Enable native CPU optimizations outside Debug builds" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_IPO "Enable link-time optimization for non-Debug builds when supported" ON)
option(BUILD_RENDERING_TESTS "Build Rendering module tests" ON)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base directory" FORCE)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "Suppress developer warnings from third-party CMake projects")

if(WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries on Windows" FORCE)
else()
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries on Unix" FORCE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

function(enable_non_debug_ipo target_name)
    set_target_properties(${target_name} PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
        INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
        INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
    )
endfunction()

include(CTest)
include(dependencies)

if(TARGET lua_static)
    set_target_properties(lua_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

set(GENERATED_ROOT "${CMAKE_BINARY_DIR}/gen_includes")
set(GENERATED_DIR "${GENERATED_ROOT}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")

configure_file(
    "${CMAKE_SOURCE_DIR}/App/AppVersion.h.in"
    "${GENERATED_DIR}/AppVersion.h"
    @ONLY
)

file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "App/*.cpp"
)

list(FILTER CORE_SOURCES EXCLUDE REGEX ".*/App/tests/.*")

set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/App/StbImage.cpp"
    PROPERTIES
        COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>"
)

include(shaders)
include(embedded_resources)
include(embedded_translate)

add_subdirectory(Lattice)
add_subdirectory(Rendering)

file(GLOB PLUGIN_DIRS RELATIVE "${CMAKE_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/Plugins/*"
)

set(PLUGIN_TARGETS "")

foreach(plugin_dir IN LISTS PLUGIN_DIRS)
    if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${plugin_dir}"
    AND EXISTS "${CMAKE_SOURCE_DIR}/${plugin_dir}/CMakeLists.txt")
        message(STATUS "Adding plugin: ${plugin_dir}")
        add_subdirectory("${CMAKE_SOURCE_DIR}/${plugin_dir}")
        get_directory_property(dir_targets DIRECTORY "${CMAKE_SOURCE_DIR}/${plugin_dir}" BUILDSYSTEM_TARGETS)
        list(APPEND PLUGIN_TARGETS ${dir_targets})
    endif()
endforeach()

add_library(latticelab_lib STATIC ${CORE_SOURCES})
add_dependencies(latticelab_lib
    embedded_resources
    embedded_translate
)

target_link_libraries(latticelab_lib PUBLIC TBB::tbb)
target_compile_definitions(latticelab_lib PUBLIC ENABLE_TBB)

target_compile_options(latticelab_lib PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>
)

target_compile_definitions(latticelab_lib PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_include_directories(latticelab_lib PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/App
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

    target_sources(latticelab_lib PRIVATE
        Rendering/backend/MetalBackend.mm
    )

    target_link_libraries(latticelab_lib PUBLIC
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        ${COCOA_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
endif()

target_link_libraries(latticelab_lib PUBLIC
    LatticeLab::Lattice
    LatticeLab::Rendering
    glfw
    imgui
    ImGuiFileDialog_lib
    glm::glm
    webgpu
    zpp_bits
    libzstd_static
    tomlplusplus::tomlplusplus
)

include(CheckIPOSupported)
if(ENABLE_IPO)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported AND NOT MINGW)
        enable_non_debug_ipo(latticelab_lib)
    elseif(MINGW)
        message(STATUS "IPO is disabled for MinGW")
    else()
        message(STATUS "IPO is not supported: ${lto_error}")
    endif()
endif()

if(BUILD_TESTING)
    add_subdirectory(App/tests)
endif()

add_executable(${APP_NAME} main.cpp)

if(PLUGIN_TARGETS)
    add_dependencies(${APP_NAME} ${PLUGIN_TARGETS})
endif()

set_target_properties(${APP_NAME} PROPERTIES
    WIN32_EXECUTABLE $<BOOL:${WIN32}>
)

target_compile_options(${APP_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>
)

target_link_options(${APP_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>
)

if(ENABLE_IPO AND lto_supported AND NOT MINGW)
    enable_non_debug_ipo(${APP_NAME})
endif()

target_link_libraries(${APP_NAME} PRIVATE latticelab_lib)
add_dependencies(${APP_NAME} Lattice-cli)
target_copy_webgpu_binaries(${APP_NAME})

include(windows_resources)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)
if(EXISTS "${CMAKE_SOURCE_DIR}/Mods")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Mods" DESTINATION .)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/Plugins")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/Plugins" DESTINATION .)
endif()

target_link_libraries(Lattice PRIVATE
    tomlplusplus::tomlplusplus
)