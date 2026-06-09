file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
    "*.cpp"
)

# Тесты могут включать .cpp бенч-сцен напрямую: они не зависят от googlebenchmark и
# тянут только символы движка. WebGPUImpl (единственная TU с WEBGPU_CPP_IMPLEMENTATION —
# out-of-line тела обёртки wgpu::*, нужные GPU-коду движка) теперь живёт ВНУТРИ Lattice
# (Lattice/Engine/physics/gpu/WebGPUImpl.cpp) рядом с потребителями → линкуется через Lattice
# автоматически, отдельно подключать не надо.
set(TEST_EXTRA_SOURCES
    "${CMAKE_SOURCE_DIR}/Lattice/benchmarks/BenchmarkScenes.cpp"
)

add_executable(latticelab_tests ${TEST_SOURCES} ${TEST_EXTRA_SOURCES})
target_compile_options(latticelab_tests PRIVATE
    $<$<AND:$<BOOL:${OPTIMIZE_FOR_NATIVE}>,$<NOT:$<CONFIG:Debug>>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>:-march=native>
)
set_target_properties(latticelab_tests PROPERTIES
    C_COMPILER_LAUNCHER ""
    CXX_COMPILER_LAUNCHER ""
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(latticelab_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# MERGE-ФИКС: линкуем ДВИЖОК (Lattice), а не App (latticelab_lib). После апстрим-
# рефактора latticelab_lib = App/* (тянет Rendering + ImGui + glfw + wgpu) — тяжёлый
# линк, на котором BFD ld падал (ld returned 5/1, пустой stderr — OOM/креш). Тесты
# юнит-тестируют движок: ни одного include из App/ нет (проверено). Lattice PUBLIC-
# линкует webgpu+glfw+TBB, поэтому GPU-тесты резолвятся.
target_link_libraries(latticelab_tests PRIVATE
    GTest::gtest_main
    Lattice
)

# Lattice тянет webgpu (GPU-физика); тестовому бинарю нужен wgpu_native.dll
# рядом, иначе загрузка падает с STATUS_DLL_NOT_FOUND.
target_copy_webgpu_binaries(latticelab_tests)
