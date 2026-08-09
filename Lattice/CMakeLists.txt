file(GLOB_RECURSE LATTICE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

file(GLOB_RECURSE LATTICE_PUBLIC_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

foreach(lattice_file_list IN ITEMS LATTICE_SOURCES LATTICE_PUBLIC_HEADERS)
    list(FILTER ${lattice_file_list} EXCLUDE REGEX ".*/(CLI|benchmarks|build|tests|_deps|Plugins)/.*")
endforeach()

add_library(Lattice SHARED
    ${LATTICE_SOURCES}
    ${LATTICE_PUBLIC_HEADERS}
)

set_target_properties(Lattice PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_library(LatticeLab::Lattice ALIAS Lattice)

target_compile_options(Lattice PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
)

include(CheckIPOSupported)
if(ENABLE_IPO)
    check_ipo_supported(RESULT lattice_lto_supported OUTPUT lattice_lto_error)
    if(lattice_lto_supported AND NOT MINGW)
        enable_non_debug_ipo(Lattice)
    endif()
endif()

target_compile_definitions(Lattice PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN>
)

target_include_directories(Lattice PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(TARGET tomlplusplus::tomlplusplus)
    message(STATUS "toml++ OK")
else()
    message(FATAL_ERROR "toml++ MISSING")
endif()

target_link_libraries(Lattice PUBLIC
    glm::glm
    lua::lua
    sol2
    tomlplusplus::tomlplusplus
)

CPMAddPackage(
    NAME CLI11
    URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.5.0.zip
)

add_executable(Lattice-cli
    LogStyle.h
    CLI/main.cpp
    CLI/SystemInfo.cpp
    CLI/SystemInfo.h
)

set_target_properties(Lattice-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(Lattice-cli PRIVATE
    Lattice
    CLI11::CLI11
)

target_compile_definitions(Lattice-cli PRIVATE
    LL_CLI_ENGINE_VERSION="${APP_VERSION_STRING}"
    LL_CLI_BUILD_TYPE="$<CONFIG>"
    LL_CLI_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
)

if(LATTICE_BUILD_TESTS)
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    CPMAddPackage(
        NAME google_benchmark
        URL  https://github.com/google/benchmark/archive/refs/tags/v1.9.5.zip
        OPTIONS
            "BENCHMARK_ENABLE_TESTING OFF"
            "BENCHMARK_ENABLE_INSTALL OFF"
    )

    add_subdirectory(benchmarks)
endif()

add_library(LatticeKernel INTERFACE)

target_include_directories(LatticeKernel INTERFACE
${CMAKE_SOURCE_DIR}
)

target_link_libraries(LatticeKernel INTERFACE
Lattice
)

add_library(Lattice::Kernel ALIAS LatticeKernel)
add_subdirectory(Plugins)

