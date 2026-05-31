#pragma once

#include <webgpu/webgpu-raii.hpp>

struct CaptureSettings {
    enum class Preset { Ultrafast, Veryfast, Faster, Fast, Medium };
    enum class PixelFormat { Yuv420p, Yuv444p };

    int fps = 30;
    int crf = 20;
    Preset preset = Preset::Veryfast;
    PixelFormat pixelFormat = PixelFormat::Yuv444p;
};

namespace capture_utils {
    inline const char* toInputPixelFormat(wgpu::TextureFormat format) {
        switch (format) {
        case wgpu::TextureFormat::RGBA8Unorm:
        case wgpu::TextureFormat::RGBA8UnormSrgb:
            return "rgba";
        case wgpu::TextureFormat::BGRA8Unorm:
        case wgpu::TextureFormat::BGRA8UnormSrgb:
            return "bgra";
        default:
            return "rgba";
        }
    }

    inline const char* toPixelFormatArg(CaptureSettings::PixelFormat pf) {
        switch (pf) {
        case CaptureSettings::PixelFormat::Yuv420p:
            return "yuv420p";
        case CaptureSettings::PixelFormat::Yuv444p:
            return "yuv444p";
        }
        return "yuv444p";
    }

    inline const char* toPresetArg(CaptureSettings::Preset preset) {
        switch (preset) {
        case CaptureSettings::Preset::Ultrafast:
            return "ultrafast";
        case CaptureSettings::Preset::Veryfast:
            return "veryfast";
        case CaptureSettings::Preset::Faster:
            return "faster";
        case CaptureSettings::Preset::Fast:
            return "fast";
        case CaptureSettings::Preset::Medium:
            return "medium";
        }
        return "veryfast";
    }

}