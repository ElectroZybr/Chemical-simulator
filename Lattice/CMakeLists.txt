file(GLOB_RECURSE LATTICE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE LATTICE_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

foreach(lattice_file_list IN ITEMS LATTICE_SOURCES LATTICE_PUBLIC_HEADERS)
    list(FILTER ${lattice_file_list} EXCLUDE REGEX ".*/(CLI|benchmarks|build|tests|_deps)/.*")
endforeach()

add_library(Lattice STATIC
    ${LATTICE_SOURCES}
    ${LATTICE_PUBLIC_HEADERS}
)

# ДОБАВЛЕНО (форк): наш GPU-compute код (Lattice/Engine/physics/gpu/*.cpp) использует
# generated/shaders/*.wgsl.h → зависимость на цель генерации шейдеров.
add_dependencies(Lattice wgsl_shaders)

add_library(LatticeLab::Lattice ALIAS Lattice)

target_compile_options(Lattice PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
    # ДОБАВЛЕНО (форк): -ffast-math gate на физику движка (по умолчанию OFF, см. корневой CMake).
    $<$<AND:$<BOOL:${FASTMATH_PHYSICS}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-ffast-math>
)

target_compile_definitions(Lattice PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
    # GLM_FORCE_CTOR_INIT (zero-init дефолт-векторов, нужен после миграции с Vec3f) ставится
    # на сам glm interface-таргет в cmake/dependencies.cmake — глобально для всех потребителей,
    # включая Rendering (линкует glm напрямую, не Lattice).
)

target_include_directories(Lattice PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    # ДОБАВЛЕНО (форк): пути для нашего GPU-compute кода в движке.
    ${GENERATED_ROOT}                       # generated/shaders/*.wgsl.h
    ${webgpu_cpp_SOURCE_DIR}/wgpu-native     # <webgpu/webgpu.hpp>, webgpu-raii.hpp
)

target_link_libraries(Lattice PUBLIC glm::glm)

# ДОБАВЛЕНО (форк): движок (через gpu/*.cpp и Simulation.cpp) реально использует webgpu.
# webgpu — леаф-зависимость, линковка её в Lattice НЕ создаёт цикла с Rendering.
target_link_libraries(Lattice PUBLIC webgpu)
# ДОБАВЛЕНО (форк): Rendering/backend/WGPUContext.h (его инклудят gpu/*.cpp) тянет GLFW/glfw3.h —
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

add_executable(Lattice-cli CLI/main.cpp)

set_target_properties(Lattice-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Lattice-cli PRIVATE
    Lattice
    CLI11::CLI11
)

add_subdirectory(tests)

if(BUILD_BENCHMARKS)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.9.5
        GIT_SHALLOW ON
    )

    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR      OFF CACHE BOOL "" FORCE)   # ДОБАВЛЕНО (форк)

    FetchContent_MakeAvailable(googlebenchmark)
    add_subdirectory(benchmarks)
endif()
