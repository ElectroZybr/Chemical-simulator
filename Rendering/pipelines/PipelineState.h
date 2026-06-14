#pragma once

#include <webgpu/webgpu-raii.hpp>

struct PipelineState {
    wgpu::raii::RenderPipeline atomSphere;
    wgpu::raii::RenderPipeline bond;
    wgpu::raii::RenderPipeline box;
    wgpu::raii::RenderPipeline grid;
    wgpu::raii::RenderPipeline potentialField;
    wgpu::raii::RenderPipeline fieldArrows;
    wgpu::raii::RenderPipeline fieldContours;
    wgpu::raii::RenderPipeline memoryOrder;
};
