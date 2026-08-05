set(RENDERING_BENCHMARK_SOURCES)
set(RENDERING_BENCHMARK_INCLUDE_DIRS)

if(EXISTS "${CMAKE_SOURCE_DIR}/Rendering/benchmarks/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/Rendering/benchmarks" "${CMAKE_BINARY_DIR}/Rendering/benchmarks")
endif()

file(GLOB_RECURSE BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/Lattice/Engine/*/tests/bench/*.cpp"
    "${CMAKE_SOURCE_DIR}/Lattice/Plugins/*/tests/bench/*.cpp"
    "rendering/*.cpp"
    "BmRunner/Support.cpp"
    "SceneBuilder.cpp"
    "BmRunner/Main.cpp"
)

list(APPEND BENCHMARK_SOURCES ${RENDERING_BENCHMARK_SOURCES})

add_executable(benchmarks ${BENCHMARK_SOURCES})

add_executable(bench
    BmRunner/Support.cpp
    BmRunner/Data.cpp
    BmRunner/Style.cpp
    BmRunner/Output.cpp
    BmRunner/Runner.cpp
)

target_compile_options(benchmarks PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
)

set_target_properties(benchmarks PROPERTIES
    C_COMPILER_LAUNCHER ""
    CXX_COMPILER_LAUNCHER ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

set_target_properties(bench PROPERTIES
    C_COMPILER_LAUNCHER ""
    CXX_COMPILER_LAUNCHER ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
    ${RENDERING_BENCHMARK_INCLUDE_DIRS}
)

target_include_directories(bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(benchmarks PRIVATE
    benchmark::benchmark
    latticelab_lib
    LatticeClassicMD
    LatticeClassicMDMT
)

target_link_libraries(bench PRIVATE
    benchmark::benchmark
)
