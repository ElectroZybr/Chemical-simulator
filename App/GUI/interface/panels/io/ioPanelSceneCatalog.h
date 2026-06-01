#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <webgpu/webgpu-raii.hpp>

#include "Lattice/Engine/math/Vec2.h"

struct IOPanelSceneTile {
    std::string path;
    std::string title;
    std::string description;
    wgpu::raii::Texture previewTexture;
    wgpu::raii::TextureView previewTextureView;
    Vec2u previewSize;
    bool hasPreview = false;
};

std::vector<IOPanelSceneTile> loadIOPanelSceneTiles(std::string_view scenesDirectory);
