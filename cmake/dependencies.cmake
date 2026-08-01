include(FetchContent)

unset(IMGUI_INCLUDE_DIR CACHE)
unset(imgui_SOURCE_DIR CACHE)
unset(imgui_BINARY_DIR CACHE)

# --- Настройка TBB ---
find_package(TBB QUIET)
if(NOT TARGET TBB::tbb)
    FetchContent_Declare(
        oneTBB
        GIT_REPOSITORY https://github.com/oneapi-src/oneTBB.git
        GIT_TAG        v2021.13.0
        GIT_SHALLOW    ON
    )
    set(TBB_TEST OFF CACHE BOOL "Build TBB tests" FORCE)
    set(TBB_EXAMPLES OFF CACHE BOOL "Build TBB examples" FORCE)
    set(TBB_STRICT OFF CACHE BOOL "Treat compiler warnings as errors" FORCE)
    set(TBB_INSTALL OFF CACHE BOOL "Enable TBB install targets" FORCE)
    FetchContent_MakeAvailable(oneTBB)
endif()

# --- Настройка GLFW ---
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    ON
)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND  OFF CACHE BOOL "" FORCE)
set(GLFW_EXPOSE_NATIVE_WAYLAND  OFF CACHE BOOL "" FORCE)

# Keep GLFW linked into the app binary instead of shipping a separate runtime.
set(_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(glfw)
set(BUILD_SHARED_LIBS "${_saved_build_shared_libs}")
unset(_saved_build_shared_libs)

# --- Настройка Lua ---
FetchContent_Declare(
    lua
    URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua)

# --- Настройка sol2 ---
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        v3.3.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(sol2)

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
FetchContent_Declare(
    webgpu_distribution
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution.git
    GIT_TAG        wgpu-v24.0.0.2
    GIT_SHALLOW    ON
)
set(WEBGPU_BACKEND "WGPU" CACHE STRING "WebGPU backend" FORCE)
set(WGPU_LINK_TYPE "STATIC" CACHE STRING "Link wgpu-native statically" FORCE)
FetchContent_MakeAvailable(webgpu_distribution)

FetchContent_Declare(
    webgpu_cpp
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-Cpp.git
    GIT_TAG        wgpu-v24.0.3.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(webgpu_cpp)

# --- Настройка ImGui ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.92.3
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(imgui)
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
FetchContent_Declare(
    ImGuiFileDialog
    GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
    GIT_TAG        v0.6.8
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(ImGuiFileDialog)

if(TARGET ImGuiFileDialog)
    target_include_directories(ImGuiFileDialog PUBLIC
        ${imguifiledialog_SOURCE_DIR}
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
    )
    target_link_libraries(ImGuiFileDialog PUBLIC imgui)
endif()

add_library(ImGuiFileDialog_lib STATIC
    ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp
)
target_include_directories(ImGuiFileDialog_lib PUBLIC
    ${imguifiledialog_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)
target_link_libraries(ImGuiFileDialog_lib PUBLIC imgui)
target_compile_options(ImGuiFileDialog_lib PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wno-stringop-overflow>
)

# --- Настройка GLM ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(glm)

# --- Настройка zpp_bits ---
FetchContent_Declare(
    zpp_bits
    GIT_REPOSITORY https://github.com/eyalz800/zpp_bits.git
    GIT_TAG        v4.7
    GIT_SHALLOW    ON

)
FetchContent_MakeAvailable(zpp_bits)
if(NOT TARGET zpp_bits)
    add_library(zpp_bits INTERFACE)
    target_include_directories(zpp_bits INTERFACE "${zpp_bits_SOURCE_DIR}")
endif()

# --- Настройка zstd ---
FetchContent_Declare(
    zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG        v1.5.6
    SOURCE_SUBDIR  build/cmake
)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "Build zstd shared library" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "Build zstd static library" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build zstd programs")
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "Build zstd tests")
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "Build zstd contrib")
set(ZSTD_BUILD_CONTRIB_TESTS OFF CACHE BOOL "Build zstd contrib tests")
set(ZSTD_BUILD_CONTRIB_EXAMPLES OFF CACHE BOOL "Build zstd contrib examples")
set(ZSTD_BUILD_CONTRIB_LIBS OFF CACHE BOOL "Build zstd contrib libs")
set(ZSTD_INSTALL OFF CACHE BOOL "Build zstd install")

# zstd is linked as a static dependency in this project, so keep its local
# BUILD_SHARED_LIBS consistent to avoid upstream configuration warnings.
set(_saved_build_shared_libs "${BUILD_SHARED_LIBS}")
set(BUILD_SHARED_LIBS OFF)
FetchContent_MakeAvailable(zstd)
set(BUILD_SHARED_LIBS "${_saved_build_shared_libs}")
unset(_saved_build_shared_libs)

if(TARGET libzstd_static)
    target_compile_options(libzstd_static PRIVATE
        $<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
    )
endif()
