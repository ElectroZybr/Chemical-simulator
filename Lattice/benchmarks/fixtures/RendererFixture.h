#pragma once
#include <glm/gtc/random.hpp>

#include <cmath>
#include <memory>

#include <benchmark/benchmark.h>

#include "App/interaction/ToolsManager.h"
#include "App/interaction/picking/PickingSystem.h"
#include "Engine/Simulation.h"
#include "Engine/physics/Atom/AtomData.h"
#include "Engine/physics/Atom/AtomStorage.h"
#include "Engine/physics/Bond.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/backend/WGPUContext.h"

// MERGE: апстрим заменил интерфейс IRenderer базовым классом BaseRenderer.
template <typename T>
concept IsRenderer = std::derived_from<T, BaseRenderer>;

class RendererFixtureBase : public benchmark::Fixture {
protected:
    void prepareAtoms(benchmark::State& state);
    void createRenderTargets(wgpu::Device device, wgpu::TextureFormat colorFormat);
    void drawFrame();
    void setCounters(benchmark::State& state) const;

    std::unique_ptr<BaseRenderer> renderer_;
    Lattice::Simulation simulation_;

private:
    static AtomStorage makeGridAtoms(int count);

    wgpu::raii::Texture targetTexture_;
    wgpu::raii::TextureView targetTextureView_;
    wgpu::raii::Texture depthTexture_;
    wgpu::raii::TextureView depthTextureView_;
};

wgpu::Device benchmarkDevice();
wgpu::TextureFormat benchmarkSurfaceFormat();

template <IsRenderer TRenderer> class RendererFixture : public RendererFixtureBase {
public:
    void SetUp(benchmark::State& state) override {
        auto& ctx = WGPUContext::instance();
        ctx.initHeadless(800, 600);

        // Offscreen render target
        wgpu::TextureDescriptor colorDesc{};
        colorDesc.size = {800, 600, 1};
        colorDesc.format = ctx.surfaceFormat();
        colorDesc.usage = wgpu::TextureUsage::RenderAttachment;
        colorDesc.mipLevelCount = 1;
        colorDesc.sampleCount = 1;
        colorDesc.dimension = wgpu::TextureDimension::_2D;
        colorTexture_ = ctx.device()->createTexture(colorDesc);
        colorTextureView_ = colorTexture_->createView();

        simulation_.createWorld(glm::vec3(300, 300, 300));
        simulation_.world().getAtomStorage() = makeGridAtoms(static_cast<int>(state.range(0)));
        renderer_ = std::make_unique<TRenderer>(simulation_.world(), ctx.surfaceFormat());
        renderer_->camera.setScreenSize(glm::vec2{800.0f, 600.0f});  // MERGE: апстрим добавил glm::vec2-перегрузку — braced-init стал неоднозначным
        renderer_->camera.resetView();
        createRenderTargets(*ctx.device(), ctx.surfaceFormat());

        ToolsManager::pickingSystem = new PickingSystem(simulation_.world().getAtomStorage(), simulation_.world(), renderer_);
    }

    void TearDown(benchmark::State&) override {
        // Дожидаемся завершения GPU работы
        WGPUContext::instance().device()->poll(true, nullptr);
        renderer_.reset();
    }

protected:
    void setCounters(benchmark::State& state) const {
        state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(simulation_.world().getAtomStorage().size()));
    }

    wgpu::raii::Texture colorTexture_;
    wgpu::raii::TextureView colorTextureView_;

private:
    static AtomStorage makeGridAtoms(int count) {
        AtomStorage atoms;
        atoms.reserve(count);
        const int side = static_cast<int>(std::cbrt(count)) + 1;
        for (int i = 0; i < count; ++i) {
            atoms.addAtom(glm::vec3((i % side) * 3.0f, ((i / side) % side) * 3.0f, (i / static_cast<float>(side * side)) * 3.0f),
                          glm::sphericalRand(0.5f), AtomData::Type::H);
        }
        return atoms;
    }
};
