#pragma once

#include <string_view>

#include <webgpu/webgpu-raii.hpp>

namespace Lattice {
    class Simulation;
}
class BaseRenderer;
struct PreviewFrameRect;
class CaptureController;

class AppStateIO {
public:
    static void save(CaptureController& captureController, const PreviewFrameRect& previewRect, const Lattice::Simulation& simulation,
                     const BaseRenderer& renderer, std::string_view path);
    static void load(Lattice::Simulation& simulation, BaseRenderer& renderer, std::string_view path);

private:
    static void saveText(CaptureController& captureController, const PreviewFrameRect& previewRect, const Lattice::Simulation& simulation,
                         const BaseRenderer& renderer, std::string_view path);
    static void saveBinary(CaptureController& captureController, const PreviewFrameRect& previewRect, const Lattice::Simulation& simulation,
                           const BaseRenderer& renderer, std::string_view path);

    static void loadText(Lattice::Simulation& simulation, BaseRenderer& renderer, std::string_view path);
    static void loadBinary(Lattice::Simulation& simulation, BaseRenderer& renderer, std::string_view path);
};
