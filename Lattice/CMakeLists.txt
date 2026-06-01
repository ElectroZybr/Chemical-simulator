# ПРЕДЛАГАЕМАЯ версия Lattice/CMakeLists.txt.
#
# ЭТОТ ФАЙЛ НЕ БЫЛ UU-КОНФЛИКТОМ: git принял upstream-версию «чисто» (он создал её
# в новой раскладке). НО собранный как есть проект НЕ соберётся, потому что наш форк
# добавил в движок GPU-резидентную физику (Lattice/Engine/physics/gpu/*), которой у
# upstream нет, и она тянет webgpu + generated-шейдеры. Плюс наш googlebenchmark-блок
# (который upstream перенёс СЮДА) у нас имел доп. флаг BENCHMARK_ENABLE_WERROR=OFF, а
# движок — единственный потребитель TBB. Поэтому это «объединить both» и для данного файла.
#
# Что взято у upstream (THEIRS) без изменений:
#   - GLOB Lattice/**/*.cpp|*.h с исключением CLI|benchmarks|build|_deps;
#   - add_library(Lattice STATIC ...), add_executable(Lattice-cli ...) + CLI11;
#   - if(BUILD_BENCHMARKS) { FetchContent googlebenchmark; add_subdirectory(benchmarks) }.
#
# НАШИ добавления (помечены «ДОБАВЛЕНО (форк)»):
#   1) webgpu + GENERATED_ROOT в include-путях Lattice и линковка webgpu — чтобы
#      компилировались наши Lattice/Engine/physics/gpu/*.cpp (включают
#      <webgpu/webgpu.hpp>, "Rendering/WGPUContext.h" (header-only) и
#      generated/shaders/*.wgsl.h). ВАЖНО: цикла Lattice<->Rendering НЕТ — gpu-код
#      использует ТОЛЬКО header-only WGPUContext + сам webgpu (леаф-зависимость), и
#      НИ ОДНОГО символа из .cpp Rendering (проверено: 0 ссылок на Renderer*/Camera/
#      BaseRenderer в gpu/*.cpp). Simulation.cpp зовёт gpu-методы — они в ТОМ ЖЕ
#      архиве Lattice, резолвятся внутри. Rendering по-прежнему линкует Lattice (PUBLIC).
#   2) add_dependencies(Lattice wgsl_shaders) — gpu-код использует сгенерированные хедеры.
#   3) if(USE_TBB) линковка TBB::tbb + LATTICELAB_USE_TBB=1 (движок — потребитель;
#      сам onetbb выкачан в корневом CMake до add_subdirectory(Lattice)).
#   4) BENCHMARK_ENABLE_WERROR OFF в googlebenchmark FetchContent.
#   5) -ffast-math gate на цель Lattice (физический код движка) под FASTMATH_PHYSICS.

cmake_minimum_required(VERSION 3.28)
project(LatticeEngine LANGUAGES CXX)

file(GLOB_RECURSE LATTICE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
list(FILTER LATTICE_SOURCES EXCLUDE REGEX ".*/(CLI|benchmarks|build|_deps)/.*")

file(GLOB_RECURSE LATTICE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
list(FILTER LATTICE_HEADERS EXCLUDE REGEX ".*/(CLI|benchmarks|build|_deps)/.*")

add_library(Lattice STATIC
    ${LATTICE_SOURCES}
    ${LATTICE_HEADERS}
)

# ДОБАВЛЕНО (форк): наш GPU-compute код (Lattice/Engine/physics/gpu/*.cpp,
# подхватывается GLOB выше после переезда из старого Engine/physics/gpu/) использует
# generated/shaders/*.wgsl.h → зависимость на цель генерации шейдеров.
add_dependencies(Lattice wgsl_shaders)

add_executable(Lattice-cli CLI/main.cpp)
target_link_libraries(Lattice-cli PRIVATE Lattice)
set_target_properties(Lattice-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(Lattice-cli Lattice PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

target_compile_options(Lattice PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    # ДОБАВЛЕНО (форк): -ffast-math gate на физику движка (по умолчанию OFF, см. корневой CMake).
    $<$<AND:$<BOOL:${FASTMATH_PHYSICS}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-ffast-math>
)

target_compile_definitions(Lattice PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_include_directories(Lattice PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    # ДОБАВЛЕНО (форк): пути для нашего GPU-compute кода в движке.
    ${GENERATED_ROOT}                       # generated/shaders/*.wgsl.h
    ${webgpu_cpp_SOURCE_DIR}/wgpu-native     # <webgpu/webgpu.hpp>, webgpu-raii.hpp
)

# ДОБАВЛЕНО (форк): движок (через gpu/*.cpp и Simulation.cpp) реально использует webgpu.
# webgpu — леаф-зависимость (определена в dependencies.cmake / FetchContent в корне),
# поэтому линковка её в Lattice НЕ создаёт цикла с Rendering.
target_link_libraries(Lattice PUBLIC webgpu)
# ДОБАВЛЕНО (форк): Rendering/WGPUContext.h (его инклудят gpu/*.cpp) тянет GLFW/glfw3.h —
# glfw леаф, цикла с Rendering не создаёт.
target_link_libraries(Lattice PUBLIC glfw)

# ДОБАВЛЕНО (форк): TBB — движок единственный потребитель (Simulation.cpp, ForceField.cpp).
# onetbb уже выкачан в корневом CMake (FetchContent) ДО add_subdirectory(Lattice).
if(USE_TBB)
    target_link_libraries(Lattice PUBLIC TBB::tbb)
    target_compile_definitions(Lattice PUBLIC LATTICELAB_USE_TBB=1)
endif()

include(FetchContent)

FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)

FetchContent_MakeAvailable(CLI11)

target_link_libraries(Lattice-cli PRIVATE CLI11::CLI11)

if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.5
        GIT_SHALLOW    ON
    )
    set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS       OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR      OFF CACHE BOOL "" FORCE)   # ДОБАВЛЕНО (форк)

    FetchContent_MakeAvailable(googlebenchmark)
    add_subdirectory(benchmarks)
endif()
