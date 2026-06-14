#pragma once

#include <memory>

#include "App/capture/CaptureController.h"
#include "App/debug/CreateDebugPanels.h"
#include "App/viewport/SimulationSceneSource.h"
#include "Rendering/BaseRenderer.h"

namespace Lattice {
    class Simulation;
}
class Interface;

class SceneViewport {
public:
    enum class RendererType : unsigned char {
        Renderer2D = 0,
        Renderer3D = 1,
    };

    SceneViewport(RendererType type, CaptureController& captureController);

    [[nodiscard]] BaseRenderer& renderer() noexcept { return *renderer_; }
    [[nodiscard]] const BaseRenderer& renderer() const noexcept { return *renderer_; }
    [[nodiscard]] std::unique_ptr<BaseRenderer>& rendererHandle() noexcept { return renderer_; }
    [[nodiscard]] const std::unique_ptr<BaseRenderer>& rendererHandle() const noexcept { return renderer_; }

    void setScreenSize(int width, int height);
    void resetView();

    void syncScene(const Lattice::Simulation& simulation);
    void renderFrame(Lattice::Simulation& simulation, Interface& appInterface, const DebugViews& debugViews);

    bool setRendererType(RendererType type, const Lattice::Simulation& simulation);

private:
    struct Cached2DCameraState {
        bool valid = false;
        glm::vec2 position{0.0f, 0.0f};
        float zoom = 1.0f;
        glm::vec3 direction{0.0f, 0.0f, 1.0f};
        glm::vec3 up{0.0f, 1.0f, 0.0f};
        glm::vec3 center{0.0f, 0.0f, 0.0f};
    };

    static std::unique_ptr<BaseRenderer> createRenderer(RendererType type);
    static void copyRenderSettings(BaseRenderer& destination, const BaseRenderer& source);

    CaptureController* captureController_ = nullptr;
    std::unique_ptr<BaseRenderer> renderer_;
    Cached2DCameraState cached2DCameraState_{};
};
