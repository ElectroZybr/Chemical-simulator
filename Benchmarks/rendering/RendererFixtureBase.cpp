#include <stdexcept>

#include <GLFW/glfw3.h>

#include "Benchmarks/fixtures/RendererFixture.h"
#include "Rendering/WGPUContext.h"

namespace {
    GLFWwindow* benchmarkWindow = nullptr;

    void ensureBenchmarkContext() {
        if (benchmarkWindow != nullptr) {
            return;
        }

        if (!glfwInit()) {
            throw std::runtime_error("glfw: failed to initialize benchmark window");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        benchmarkWindow = glfwCreateWindow(800, 600, "LatticeLab benchmark", nullptr, nullptr);
        if (benchmarkWindow == nullptr) {
            throw std::runtime_error("glfw: failed to create benchmark window");
        }

        WGPUContext::instance().init(benchmarkWindow, 800, 600);
    }
}

wgpu::Device benchmarkDevice() {
    ensureBenchmarkContext();
    return *WGPUContext::instance().device();
}

wgpu::TextureFormat benchmarkSurfaceFormat() {
    ensureBenchmarkContext();
    return WGPUContext::instance().surfaceFormat();
}

void RendererFixtureBase::prepareAtoms(benchmark::State& state) {
    ensureBenchmarkContext();
    if (simulation_.worldCount() == 0) {
        simulation_.createWorld(Vec3f(300, 300, 300));
    }
    simulation_.world().getAtomStorage() = makeGridAtoms(state.range(0));
}

void RendererFixtureBase::createRenderTargets(wgpu::Device device, wgpu::TextureFormat colorFormat) {
    wgpu::TextureDescriptor colorDesc{};
    colorDesc.size = {800, 600, 1};
    colorDesc.format = colorFormat;
    colorDesc.usage = wgpu::TextureUsage::RenderAttachment;
    colorDesc.mipLevelCount = 1;
    colorDesc.sampleCount = 1;
    colorDesc.dimension = wgpu::TextureDimension::_2D;
    targetTexture_ = device.createTexture(colorDesc);
    targetTextureView_ = targetTexture_->createView();

    wgpu::TextureDescriptor depthDesc{};
    depthDesc.size = {800, 600, 1};
    depthDesc.format = wgpu::TextureFormat::Depth24Plus;
    depthDesc.usage = wgpu::TextureUsage::RenderAttachment;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.dimension = wgpu::TextureDimension::_2D;
    depthTexture_ = device.createTexture(depthDesc);

    wgpu::TextureViewDescriptor depthViewDesc{};
    depthViewDesc.format = wgpu::TextureFormat::Depth24Plus;
    depthViewDesc.dimension = wgpu::TextureViewDimension::_2D;
    depthViewDesc.mipLevelCount = 1;
    depthViewDesc.arrayLayerCount = 1;
    depthViewDesc.aspect = wgpu::TextureAspect::DepthOnly;
    depthTextureView_ = depthTexture_->createView(depthViewDesc);
}

void RendererFixtureBase::drawFrame() {
    renderer_->drawShot(*targetTextureView_, *depthTextureView_, simulation_);
    renderer_->endFrame();
}

void RendererFixtureBase::setCounters(benchmark::State& state) const {
    state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(simulation_.world().getAtomStorage().size()));
}

AtomStorage RendererFixtureBase::makeGridAtoms(int count) {
    AtomStorage atoms;
    atoms.reserve(count);
    const int side = static_cast<int>(std::cbrt(count)) + 1;
    for (int i = 0; i < count; ++i) {
        atoms.addAtom(Vec3f((i % side) * 3.0, ((i / side) % side) * 3.0, (i / static_cast<double>(side * side)) * 3.0),
                      Vec3f::Random() * 0.5, AtomData::Type::H);
    }
    return atoms;
}
