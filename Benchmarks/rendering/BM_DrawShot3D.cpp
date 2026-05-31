#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h"
#include "Rendering/3d/Renderer3DWGPU.h"
#include "Rendering/WGPUContext.h"

// @bench_meta {"id":"RendererFixture<Renderer3D>/DrawShot3D","ru":"Отрисовка кадра 3D","group":"Рендер/3D"}
BENCHMARK_TEMPLATE_DEFINE_F(RendererFixture, DrawShot3D, Renderer3DWGPU)(benchmark::State& state) {
    WGPUContext& ctx = WGPUContext::instance();
    for (auto _ : state) {
        renderer_->drawShot(*colorTextureView_, *ctx.depthView(), simulation_);
        renderer_->endFrame();
        // Ждём GPU чтобы измерить реальное время а не только submit
        ctx.device()->poll(true, nullptr);
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(RendererFixture, DrawShot3D)->RangeMultiplier(8)->Range(125, 8000);
