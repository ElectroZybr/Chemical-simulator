if(DEFINED ENV{CPM_SOURCE_CACHE})
    set(_cpm_source_cache "$ENV{CPM_SOURCE_CACHE}")
else()
    set(_cpm_source_cache "${CMAKE_SOURCE_DIR}/.cache/cpm")
    set(CPM_SOURCE_CACHE "${_cpm_source_cache}" CACHE PATH "CPM source cache")
endif()

set(CPM_DOWNLOAD_VERSION 0.43.1)
set(CPM_DOWNLOAD_LOCATION "${_cpm_source_cache}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        STATUS download_status
    )
    list(GET download_status 0 status_code)
    if(NOT status_code EQUAL 0)
        list(GET download_status 1 status_message)
        file(REMOVE ${CPM_DOWNLOAD_LOCATION})
        message(FATAL_ERROR "Failed to download CPM.cmake: ${status_message}")
    endif()
endif()
include(${CPM_DOWNLOAD_LOCATION})

# --- Настройка TBB ---
find_package(TBB QUIET)
if(NOT TARGET TBB::tbb)
    set(TBB_TEST OFF CACHE BOOL "Build TBB tests" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "Build TBB examples" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Treat compiler warnings as errors" FORCE)
    set(TBB_INSTALL OFF CACHE BOOL "Enable TBB install targets" FORCE)

    CPMAddPackage(
        NAME oneTBB
        GITHUB_REPOSITORY oneapi-src/oneTBB
        GIT_TAG v2021.13.0
        GIT_SHALLOW YES
    )
endif()

# --- Настройка GLFW ---
set(_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
    NAME glfw
    URL  https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
    OPTIONS
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_DOCS OFF"
        "GLFW_INSTALL OFF"
        "GLFW_BUILD_WAYLAND OFF"
        "GLFW_EXPOSE_NATIVE_WAYLAND OFF"
)
set(BUILD_SHARED_LIBS "${_saved_build_shared_libs}")
unset(_saved_build_shared_libs)

# --- Настройка Lua ---
CPMAddPackage(
    NAME lua
    URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
)

# --- Настройка sol2 ---
CPMAddPackage(
    NAME sol2
    URL https://github.com/ThePhD/sol2/archive/refs/tags/v3.3.1.zip
)

set(_sol2_optional_impl "${sol2_SOURCE_DIR}/include/sol/optional_implementation.hpp")
if(EXISTS "${_sol2_optional_impl}")
    file(READ "${_sol2_optional_impl}" _sol2_optional_impl_content)
    string(REPLACE
        "template <class... Args>\n\t\tT& emplace(Args&&... args) noexcept {\n\t\t\tstatic_assert(std::is_constructible<T, Args&&...>::value, \"T must be constructible with Args\");\n\n\t\t\t*this = nullopt;\n\t\t\tthis->construct(std::forward<Args>(args)...);\n\t\t}\n"
        "template <class... Args>\n\t\tT& emplace(Args&&... args) noexcept {\n\t\t\tstatic_assert(sizeof...(Args) == 1, \"optional<T&>::emplace expects exactly one argument\");\n\t\t\tauto&& value = std::get<0>(std::forward_as_tuple(std::forward<Args>(args)...));\n\t\t\tm_value = std::addressof(value);\n\t\t\treturn *m_value;\n\t\t}\n"
        _sol2_optional_impl_content
        "${_sol2_optional_impl_content}"
    )
    file(WRITE "${_sol2_optional_impl}" "${_sol2_optional_impl_content}")
endif()

if(NOT TARGET sol2)
    add_library(sol2 INTERFACE)
    target_include_directories(sol2 INTERFACE "${sol2_SOURCE_DIR}/include")
endif()

add_library(lua_static STATIC
    ${lua_SOURCE_DIR}/src/lapi.c
    ${lua_SOURCE_DIR}/src/lauxlib.c
    ${lua_SOURCE_DIR}/src/lbaselib.c
    ${lua_SOURCE_DIR}/src/lcode.c
    ${lua_SOURCE_DIR}/src/lcorolib.c
    ${lua_SOURCE_DIR}/src/lctype.c
    ${lua_SOURCE_DIR}/src/ldblib.c
    ${lua_SOURCE_DIR}/src/ldebug.c
    ${lua_SOURCE_DIR}/src/ldo.c
    ${lua_SOURCE_DIR}/src/ldump.c
    ${lua_SOURCE_DIR}/src/lfunc.c
    ${lua_SOURCE_DIR}/src/lgc.c
    ${lua_SOURCE_DIR}/src/linit.c
    ${lua_SOURCE_DIR}/src/liolib.c
    ${lua_SOURCE_DIR}/src/llex.c
    ${lua_SOURCE_DIR}/src/lmathlib.c
    ${lua_SOURCE_DIR}/src/lmem.c
    ${lua_SOURCE_DIR}/src/loadlib.c
    ${lua_SOURCE_DIR}/src/lobject.c
    ${lua_SOURCE_DIR}/src/lopcodes.c
    ${lua_SOURCE_DIR}/src/loslib.c
    ${lua_SOURCE_DIR}/src/lparser.c
    ${lua_SOURCE_DIR}/src/lstate.c
    ${lua_SOURCE_DIR}/src/lstring.c
    ${lua_SOURCE_DIR}/src/lstrlib.c
    ${lua_SOURCE_DIR}/src/ltable.c
    ${lua_SOURCE_DIR}/src/ltablib.c
    ${lua_SOURCE_DIR}/src/ltm.c
    ${lua_SOURCE_DIR}/src/lundump.c
    ${lua_SOURCE_DIR}/src/lutf8lib.c
    ${lua_SOURCE_DIR}/src/lvm.c
    ${lua_SOURCE_DIR}/src/lzio.c
)

add_library(lua::lua ALIAS lua_static)
target_include_directories(lua_static PUBLIC ${lua_SOURCE_DIR}/src)

if(UNIX AND NOT APPLE)
    target_link_libraries(lua_static PUBLIC m dl)
endif()

# --- Настройка WebGPU ---
set(WEBGPU_BACKEND "WGPU" CACHE STRING "WebGPU backend" FORCE)
set(WGPU_LINK_TYPE "STATIC" CACHE STRING "Link wgpu-native statically" FORCE)
CPMAddPackage(
    NAME webgpu_distribution
    URL https://github.com/eliemichel/WebGPU-distribution/archive/refs/tags/wgpu-v24.0.0.2.zip
)

# --- Настройка ImGui ---
CPMAddPackage(
    NAME imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.3.zip
    DOWNLOAD_ONLY YES
)
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
)
set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC
    webgpu
    glfw
)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
set(GLFW_EXPOSE_NATIVE_WAYLAND 0)

# --- Настройка ImGuiFileDialog ---
CPMAddPackage(
    NAME ImGuiFileDialog
    URL https://github.com/aiekick/ImGuiFileDialog/archive/refs/tags/v0.6.8.zip
    DOWNLOAD_ONLY YES
)

add_library(ImGuiFileDialog_lib STATIC
    ${ImGuiFileDialog_SOURCE_DIR}/ImGuiFileDialog.cpp
)
target_include_directories(ImGuiFileDialog_lib PUBLIC
    ${ImGuiFileDialog_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)
target_link_libraries(ImGuiFileDialog_lib PUBLIC imgui)
target_compile_options(ImGuiFileDialog_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>
)

# --- Настройка GLM ---
CPMAddPackage(
    NAME glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
)

# --- Настройка zpp_bits ---
CPMAddPackage(
    NAME zpp_bits
    URL https://github.com/eyalz800/zpp_bits/archive/refs/tags/v4.7.zip
    DOWNLOAD_ONLY YES
)
if(NOT TARGET zpp_bits)
    add_library(zpp_bits INTERFACE)
    target_include_directories(zpp_bits INTERFACE "${zpp_bits_SOURCE_DIR}")
endif()

# --- Настройка zstd ---
# zstd is linked as a static dependency in this project, so keep its local
# BUILD_SHARED_LIBS consistent to avoid upstream configuration warnings.
set(_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
CPMAddPackage(
    NAME zstd
    URL https://github.com/facebook/zstd/archive/refs/tags/v1.5.6.zip
    SOURCE_SUBDIR  build/cmake
    OPTIONS
        "ZSTD_BUILD_SHARED OFF"
        "ZSTD_BUILD_STATIC ON"
        "ZSTD_BUILD_PROGRAMS OFF"
        "ZSTD_BUILD_TESTS OFF"
        "ZSTD_BUILD_CONTRIB OFF"
        "ZSTD_BUILD_CONTRIB_TESTS OFF"
        "ZSTD_BUILD_CONTRIB_EXAMPLES OFF"
        "ZSTD_BUILD_CONTRIB_LIBS OFF"
        "ZSTD_INSTALL OFF"
)
set(BUILD_SHARED_LIBS "${_saved_build_shared_libs}")
unset(_saved_build_shared_libs)

if(TARGET libzstd_static)
    target_compile_options(libzstd_static PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
    )
endif()

# --- Настройка тестов ---
option(LATTICE_BUILD_TESTS "Build tests" OFF)

if(LATTICE_BUILD_TESTS)
    enable_testing()

    # --- Настройка Catch2 ---
    CPMAddPackage(
        NAME Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.8.1.zip
        OPTIONS
            "CATCH_BUILD_TESTING OFF"
    )
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

# --- Настройка toml++ ---
CPMAddPackage(
    NAME tomlplusplus
    GITHUB_REPOSITORY marzer/tomlplusplus
    VERSION 3.4.0
)