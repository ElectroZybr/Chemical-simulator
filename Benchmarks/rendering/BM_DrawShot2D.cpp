#include <benchmark/benchmark.h>

#include "Benchmarks/fixtures/RendererFixture.h"
#include "Rendering/2d/Renderer2DWGPU.h"

// @bench_meta {"id":"RendererFixture<Renderer2D>/DrawShot2D","ru":"Отрисовка кадра 2D","group":"Рендер/2D"}
BENCHMARK_TEMPLATE_DEFINE_F(RendererFixture, DrawShot2D, Renderer2DWGPU)(benchmark::State& state) {
    auto& ctx = WGPUContext::instance();
    for (auto _ : state) {
        renderer_->drawShot(*colorTextureView_, *ctx.depthView(), simulation_);
        renderer_->endFrame();
        // Ждём GPU чтобы измерить реальное время а не только submit
        ctx.device()->poll(true, nullptr);
    }
    setCounters(state);
}

BENCHMARK_REGISTER_F(RendererFixture, DrawShot2D)->RangeMultiplier(8)->Range(125, 8000);
