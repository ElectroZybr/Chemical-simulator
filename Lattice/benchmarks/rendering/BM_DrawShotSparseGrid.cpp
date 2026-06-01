#include <benchmark/benchmark.h>

#include "fixtures/RendererFixture.h"
#include "Rendering/3d/Renderer3DWGPU.h"
#include "Rendering/WGPUContext.h"
using namespace Lattice;

// Бенч рендера сцены с включённой сеткой (drawGrid=true) и крайне разреженным
// распределением атомов: ячеек много (от размера мира / cellSize), но
// большая часть пустые. drawGridImpl сейчас обходит все ячейки за O(cells)
// независимо от заполненности — на больших разреженных сценах это видно
// дороже, чем сам drawAtoms. Это точка измерения D4 (skip empty cells).
//
// @bench_meta {"id":"RendererFixture<Renderer3D>/DrawShotSparseGrid","ru":"Отрисовка кадра 3D с разреженной сеткой","group":"Рендер/3D"}
BENCHMARK_TEMPLATE_DEFINE_F(RendererFixture, DrawShotSparseGrid, Renderer3DWGPU)(benchmark::State& state) {
    WGPUContext& ctx = WGPUContext::instance();
    renderer_->drawGrid = true;
    for (auto _ : state) {
        renderer_->drawShot(*colorTextureView_, *ctx.depthView(), simulation_);
        renderer_->endFrame();
        ctx.device()->poll(true, nullptr);
    }
    setCounters(state);
}

// 125 атомов на сцене 300^3 при cellSize=5 → ~216k ячеек, ~125 не пустых.
// Отношение пусто/полно > 1000, идеальный сценарий для D4.
BENCHMARK_REGISTER_F(RendererFixture, DrawShotSparseGrid)->Arg(125)->Arg(1000)->Arg(8000);
