#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <glm/vec2.hpp>

#include <webgpu/webgpu-raii.hpp>

struct IOPanelSceneTile {
    std::string path;
    std::string title;
    std::string description;
    wgpu::raii::Texture previewTexture;
    wgpu::raii::TextureView previewTextureView;
    glm::uvec2 previewSize;
    bool hasPreview = false;
};

std::vector<IOPanelSceneTile> loadIOPanelSceneTiles(std::string_view scenesDirectory);
