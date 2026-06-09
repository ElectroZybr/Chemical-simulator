#include "BufferPool.h"

#include <cassert>

#include <webgpu/webgpu-raii.hpp>

#include "Rendering/backend/WGPUContext.h"

void BufferPool::init(size_t bufferSize, size_t initialCount) {
    this->bufferSize = bufferSize;
    for (size_t i = 0; i < initialCount; ++i) {
        free.emplace_back(createBuffer());
    }
}

wgpu::Buffer BufferPool::acquire() {
    if (!free.empty()) {
        wgpu::Buffer buf = std::move(free.front());
        free.pop_front();
        return buf;
    }
    return createBuffer();
}

void BufferPool::release(wgpu::Buffer buffer) { free.emplace_back(std::move(buffer)); }

wgpu::Buffer BufferPool::createBuffer() const {
    return WGPUContext::instance().createBuffer(bufferSize, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead, "CaptureReadback");
}
