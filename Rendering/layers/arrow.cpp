#include "Rendering/Renderer.h"

#include "Rendering/backend/WGPUContext.h"

void RendererWGPU::initFieldArrowBuffer() {
    fieldLayer_.fieldArrowVb = WGPUContext::instance().createBuffer(128, wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst, "Field_Arrow_Vectors");
    fieldLayer_.fieldArrowVbCapacity = 128;
}
