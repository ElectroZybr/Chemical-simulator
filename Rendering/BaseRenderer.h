#pragma once

#include <cstdint>
#include <vector>

#include <webgpu/webgpu-raii.hpp>

#include "Rendering/camera/Camera.h"
#include "Rendering/RenderData.h"

class BaseRenderer {
public:
    virtual ~BaseRenderer() = default;

    virtual void drawShot(wgpu::TextureView targetView, wgpu::TextureView depthView) = 0;
    virtual void endFrame() = 0;
    virtual wgpu::raii::RenderPassEncoder* currentRenderPass() { return nullptr; }
    virtual const wgpu::raii::RenderPassEncoder* currentRenderPass() const { return nullptr; }

    void resizeRenderData(size_t count) { renderData_.resize(count); }
    void clearRenderData() { renderData_.clear(); }
    RenderData& getRenderData(size_t index) { return renderData_[index]; }
    const RenderData& getRenderData(size_t index) const { return renderData_[index]; }
    size_t getRenderDataCount() const { return renderData_.size(); }

    Camera camera;

protected:
    BaseRenderer() = default;

private:
    std::vector<RenderData> renderData_;
};
