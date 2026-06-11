#pragma once

#include <cstdint>
#include <string_view>

#include <glm/vec3.hpp>
#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "GUI/interface/panels/tools/ToolsPanel.h"
#include "Rendering/camera/Camera.h"
#include "App/Signals.h"

namespace AppSignals {
    namespace UI {
        inline Signals::Signal<void(const glm::vec3& newSize)> ResizeBox;
        inline Signals::Signal<void(const glm::vec3& newSize, float maxSpeed)> SmoothResizeBox;

        inline Signals::Signal<void()> ClearSimulation;
        inline Signals::Signal<void(int atomCount, AtomData::Type atomType, bool is3D, float density)> CreateGas;
        inline Signals::Signal<void(int axisCount, AtomData::Type atomType, bool is3D)> CreateCrystal;

        inline Signals::Signal<void(RendererType type)> SetRender;
        inline Signals::Signal<void(Camera::Mode mode)> SetCameraMode;

        inline Signals::Signal<void(std::string_view path)> SaveSimulation;
        inline Signals::Signal<void(std::string_view path)> LoadSimulation;
        inline Signals::Signal<void()> ExitApplication;

        inline Signals::Signal<void()> StepPhysics;
    }

    namespace Capture {
        inline Signals::Signal<void(std::string_view path)> SetOutputDirectory;
        inline Signals::Signal<void()> ToggleRecording;
        inline Signals::Signal<void()> ToggleXYZRecording;
    }

    namespace Keyboard {
        inline Signals::Signal<void()> StepPhysics;
    }
}
