cmake_minimum_required(VERSION 3.17)
cmake_policy(SET CMP0069 NEW)
project(LatticeLab VERSION 0.1.2 LANGUAGES CXX)

set(APP_NAME "${PROJECT_NAME}")
set(APP_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(APP_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(APP_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(APP_VERSION_STRING "${PROJECT_VERSION}")

option(OPTIMIZE_FOR_NATIVE "Enable native CPU optimizations outside Debug builds" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_TESTS "Build unit tests" OFF)
option(USE_TBB "Use Intel oneTBB for parallel CPU paths" ON)
option(ENABLE_IPO "Enable link-time optimization for non-Debug builds when supported" ON)
option(BUILD_RENDERING_TESTS "Build Rendering module tests" ON)
# -ffast-math ТОЛЬКО на физический код (latticelab_lib + Lattice + Rendering),
# не на зависимости. ИЗМЕРЕНО (2026-05-30, см. RESULTS.md «-ffast-math — измерено,
# отклонено»): физику НЕ ломает (LJ/Morse остаются бит-в-бит CPU==GPU, Coulomb
# сдвигается на ~5e-7 от reassociation — много меньше порога гейтов, все 36 тестов
# + parity-гейты зелёные), НО и ускорения НЕ даёт (force-loop memory-bound и уже
# векторизован на -O3 -march=native). Оставлено OFF как задокументированный тумблер.
# Внимание: -ffinite-math-only снимает гарантии на NaN/Inf — не для test/parity-сборок.
option(FASTMATH_PHYSICS "Physics lib with -ffast-math (measured: physics-safe but NO perf win; default OFF)" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps" CACHE PATH "FetchContent base directory" FORCE)

# CMake 4.x: allow legacy dependencies that declare very old minimum policies.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "4.0")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

if (WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries on Windows" FORCE)
else()
    set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries on Unix" FORCE)
endif()

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER   ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

include(dependencies)
include(CTest)

# Upstream split: App-only glob. Engine -> Lattice/ (Lattice lib),
# Rendering -> Rendering/ (LatticeLab::Rendering). GUI/ merged into App/GUI/.
file(GLOB_RECURSE CORE_SOURCES CONFIGURE_DEPENDS
    "App/*.cpp"
)

set(GENERATED_ROOT "${CMAKE_BINARY_DIR}/gen_includes")
set(GENERATED_DIR "${GENERATED_ROOT}/generated")
file(MAKE_DIRECTORY "${GENERATED_DIR}")
configure_file(
    "${CMAKE_SOURCE_DIR}/App/AppVersion.h.in"
    "${GENERATED_DIR}/AppVersion.h"
    @ONLY
)

add_library(latticelab_lib STATIC ${CORE_SOURCES})

set(G_RES_HEADERS "")
include(embedded_resources)
add_dependencies(latticelab_lib embedded_resources)

# wgsl_shaders генерирует И апстрим-шейдеры, И наши physics_*.wgsl / gpu_cell_list /
# integrate_verlet / nl_displacement (все в Rendering/shaders/, GLOB в shaders.cmake
# берёт их автоматически). Зависимость нужна трём таргетам: Lattice (наш GPU-compute
# код Lattice/Engine/physics/gpu/*.cpp включает generated/shaders/*.wgsl.h), Rendering
# (RendererWGPU тоже) и latticelab_lib. include(shaders) ДОЛЖЕН быть до add_subdirectory,
# чтобы цель wgsl_shaders существовала, когда Lattice/Rendering вешают на неё add_dependencies.
include(shaders)
add_dependencies(latticelab_lib wgsl_shaders)

# ── НАШЕ добавление: oneTBB через FetchContent (engine-only потребитель) ────────────
# ВАЖНО: блок ДО add_subdirectory(Lattice), потому что движок (Lattice lib) — реальный
# потребитель TBB (Engine/Simulation.cpp, Engine/physics/ForceField.cpp). Линковка
# TBB::tbb + define LATTICELAB_USE_TBB происходит ВНУТРИ Lattice/CMakeLists.txt
# (см. правку там), но сама цель TBB::tbb должна уже существовать к тому моменту.
if(USE_TBB)
    # TBB v2023.0.0 безусловно добавляет -fPIC в Clang.cmake (Linux-парадигма);
    # clang на Windows MSVC-таргете не понимает этот флаг. PATCH_COMMAND убирает
    # его, и только на Windows-clang — другим платформам ничего не трогается.
    if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(_TBB_PATCH_CMD
            ${CMAKE_COMMAND} -DSRC_DIR=<SOURCE_DIR>
            -P ${CMAKE_SOURCE_DIR}/cmake/patch_tbb_clang_fpic.cmake)
    else()
        set(_TBB_PATCH_CMD "")
    endif()

    FetchContent_Declare(
        onetbb
        GIT_REPOSITORY https://github.com/uxlfoundation/oneTBB.git
        GIT_TAG        v2023.0.0
        GIT_SHALLOW    ON
        PATCH_COMMAND  ${_TBB_PATCH_CMD}
    )
    set(TBB_TEST       OFF CACHE BOOL "" FORCE)
    set(TBB_EXAMPLES   OFF CACHE BOOL "" FORCE)
    set(TBB_INSTALL    OFF CACHE BOOL "" FORCE)
    set(TBB_STRICT     OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(onetbb)
endif()

add_subdirectory(Lattice)
add_subdirectory(Rendering)

include(embedded_translate)
add_dependencies(latticelab_lib embedded_translate)

target_compile_options(latticelab_lib PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    $<$<AND:$<BOOL:${FASTMATH_PHYSICS}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-ffast-math>
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
            Rendering/MetalBackend.mm
    )

    target_link_libraries(latticelab_lib PUBLIC
        "-framework Cocoa"
        "-framework QuartzCore"
        "-framework Metal"
        ${COCOA_FRAMEWORK}
        ${METAL_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
endif ()


target_link_libraries(latticelab_lib PUBLIC
    Lattice
    LatticeLab::Rendering
    glfw
    imgui ImGuiFileDialog_lib
    glm::glm
    webgpu
    zpp_bits
    libzstd_static
)

# Примечание про TBB: FetchContent(onetbb) выполнен ВЫШЕ (до add_subdirectory(Lattice)).
# Линковка TBB::tbb + define LATTICELAB_USE_TBB=1 сделана внутри Lattice/CMakeLists.txt
# (engine — единственный потребитель). latticelab_lib получает их ТРАНЗИТИВНО через
# PUBLIC-линковку Lattice, поэтому здесь повторно линковать TBB не нужно.

if (WIN32)
    add_executable(${APP_NAME} WIN32 main.cpp)
    # WIN32 ставит /subsystem:windows, чей дефолтный entry — WinMainCRTStartup
    # (ждёт WinMain). У нас обычный int main(), поэтому под clang+lld-link
    # явно указываем mainCRTStartup — CRT-entry, который инициализирует
    # рантайм и зовёт main(), сохраняя windows-subsystem (без консоли).
    # MSVC-линкер сам разрешает это, clang+lld-link — нет.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_link_options(${APP_NAME} PRIVATE -Xlinker /entry:mainCRTStartup)
    endif()
else()
    add_executable(${APP_NAME} main.cpp)
endif()

include(CheckIPOSupported)
if(ENABLE_IPO)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported AND NOT MINGW)
        set_target_properties(latticelab_lib PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
        )
        set_target_properties(${APP_NAME} PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL TRUE
        )
    elseif(MINGW)
        message(STATUS "IPO is disabled for MinGW")
    else()
        message(STATUS "IPO is not supported: ${lto_error}")
    endif()
endif()

target_link_libraries(${APP_NAME} PRIVATE latticelab_lib)
add_dependencies(${APP_NAME} Lattice-cli)
target_copy_webgpu_binaries(${APP_NAME})

include(windows_resources)

install(TARGETS ${APP_NAME} RUNTIME DESTINATION .)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets" DESTINATION .)
if(EXISTS "${CMAKE_SOURCE_DIR}/demo/scenes")
    install(DIRECTORY "${CMAKE_SOURCE_DIR}/demo/scenes" DESTINATION demo)
endif()

# ── НАШИ добавления: GoogleBenchmark + GoogleTest FetchContent ──────────────────────
# ВНИМАНИЕ ОБ ИЗМЕНЕНИИ РАСКЛАДКИ: апстрим перенёс googlebenchmark + add_subdirectory(benchmarks)
# ВНУТРЬ Lattice/CMakeLists.txt (под if(BUILD_BENCHMARKS)). Чтобы не дублировать
# FetchContent googlebenchmark и не плодить два вызова add_subdirectory(benchmarks),
# benchmark-блок здесь УБРАН — его роль исполняет Lattice/CMakeLists.txt.
# Здесь в корне остаётся ТОЛЬКО наш googletest-блок (тесты — наш форк-онли).
if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    ON
    )
    set(BUILD_GMOCK     OFF CACHE BOOL "" FORCE)
    set(INSTALL_GTEST   OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    add_subdirectory(Tests)
endif()
