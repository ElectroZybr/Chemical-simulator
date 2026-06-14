#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <webgpu/webgpu-raii.hpp>

struct AtomVec4 {
    float x, y, z, pad = 0.f;
};

struct MemoryOrderVertex {
    glm::vec3 pos;
    float t = 0.0f;
};

struct GridInstance {
    glm::vec4 origin;
    float cellSize;
    float atomCount;
    float pad[2] = {};
};

struct FieldInstance {
    glm::vec4 origin;
    glm::vec4 potentials;
    glm::vec2 cellSize;
    float pad[2] = {};
};

template <size_t N>
struct LineLayerState {
    wgpu::raii::BindGroupLayout bindGroupLayout;
    std::array<wgpu::raii::Buffer, N> uniformBuffers;
    std::array<wgpu::raii::BindGroup, N> bindGroups;

    wgpu::raii::Buffer bondVb;
    wgpu::raii::Buffer boxVb;
    wgpu::raii::Buffer memoryOrderVb;

    size_t bondVbCapacity = 0;
    size_t memoryOrderVbCapacity = 0;

    std::array<float, 24 * 3> boxVertices{};
    glm::vec3 cachedBoxSize{-1.0f, -1.0f, -1.0f};
};

struct AtomLayerState {
    wgpu::raii::BindGroupLayout bindGroupLayout;
    wgpu::raii::BindGroup bindGroup;

    wgpu::raii::Buffer atomQuadVb;
    wgpu::raii::Buffer sbPos;
    wgpu::raii::Buffer sbVel;
    wgpu::raii::Buffer sbType;
    wgpu::raii::Buffer sbRadius;
    wgpu::raii::Buffer sbSel;

    size_t storageCapacity = 0;

    std::vector<AtomVec4> posData;
    std::vector<AtomVec4> velData;
    std::vector<glm::vec4> typeColorsData;
    std::vector<float> selectedData;
    std::vector<float> radii;
    std::vector<float> typeData;
};

template <size_t N>
struct GridLayerState {
    wgpu::raii::BindGroupLayout bindGroupLayout;
    std::array<wgpu::raii::Buffer, N> uniformBuffers;
    std::array<wgpu::raii::BindGroup, N> bindGroups;

    wgpu::raii::Buffer gridLineVb;
    wgpu::raii::Buffer gridInstVb;
    size_t gridInstVbCapacity = 0;

    std::vector<GridInstance> gridData;
};

struct FieldLayerState {
    wgpu::raii::Buffer fieldArrowVb;
    wgpu::raii::Buffer potentialFieldQuadVb;
    wgpu::raii::Buffer potentialFieldInstVb;

    size_t fieldArrowVbCapacity = 0;
    size_t potentialFieldInstVbCapacity = 0;

    std::vector<FieldInstance> fieldData;
};
