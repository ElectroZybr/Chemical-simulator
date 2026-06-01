# РАЗРЕШЁННАЯ версия Lattice/benchmarks/CMakeLists.txt (UU: объединить OURS + THEIRS).
#
# Основа — THEIRS (upstream): новый scene-aware bench_main.cpp + отдельный target `bench`
# (BmRunner). Из OURS перенесено единственное наше добавление, которого нет у апстрима:
#   target_copy_webgpu_binaries(benchmarks)  — GPU-бенчам (BM_Gpu*) нужен wgpu_native.dll
#   рядом с бинарём, иначе загрузка падает STATUS_DLL_NOT_FOUND.
#
# Почему НЕ benchmark::benchmark_main (как было в OURS): upstream's bench_main.cpp сам
# определяет int main() (разбирает --scene), поэтому линкуем benchmark::benchmark (без
# встроенного main), чтобы не было двойного main().
#
# GLOB по physics/*.cpp + rendering/*.cpp подхватит ВСЕ наши перемещённые бенчи
# (26 physics, включая BM_Gpu*, + rendering/BM_DrawShot*). Список файлов см. PLAN.md.

file(GLOB_RECURSE BENCHMARK_SOURCES CONFIGURE_DEPENDS
    "physics/*.cpp"
    "rendering/*.cpp"
    "BenchmarkScenes.cpp"
    "bench_main.cpp"
)

# ОТЛОЖЕНО (merge): render-бенчи BM_DrawShot* написаны под старый render API
# (drawShot(view,view,Simulation), конструкторы Renderer2D/3DWGPU(world,format),
# renderer->drawGrid). Апстрим развязал render<->sim — атомы идут рендереру через
# RenderData, рендереры default-конструируются. Их порт = отдельная задача (та же
# проводка RenderData, что zero-copy re-wire), НЕ часть гейта parity (это perf-бенчи
# отрисовки, не физ-результаты). RendererFixtureBase.cpp оставлен: в нём benchmarkDevice(),
# который нужен GPU-физ-бенчам (BM_Gpu*).
list(FILTER BENCHMARK_SOURCES EXCLUDE REGEX "rendering/BM_DrawShot")

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
)
target_include_directories(bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

# benchmarks линкует latticelab_lib (App lib). Транзитивно тянет Lattice + LatticeLab::Rendering
# + webgpu + TBB (всё PUBLIC у latticelab_lib), поэтому:
#   - физ-бенчи получают Engine/Rendering символы и include-пути (через PUBLIC includes Lattice);
#   - GPU-бенчи получают WGPUContext + наш GPU-compute код (компилируется в Rendering, см.
#     Lattice/CMakeLists.txt правку) + generated/shaders/*.wgsl.h (GENERATED_ROOT у Rendering).
# ВАЖНО: latticelab_lib собирается только когда benchmarks реально к нему линкуется —
# а он добавляется через add_subdirectory(Lattice) ВНУТРИ Lattice (см. Lattice/CMakeLists.txt),
# который сам подключается ПОСЛЕ создания target latticelab_lib в корне → target виден.
target_link_libraries(benchmarks PRIVATE
    benchmark::benchmark
    latticelab_lib
)

target_link_libraries(bench PRIVATE
    benchmark::benchmark
)

# НАШЕ добавление: GPU-бенчам нужен wgpu_native.dll рядом с benchmarks.exe.
target_copy_webgpu_binaries(benchmarks)
