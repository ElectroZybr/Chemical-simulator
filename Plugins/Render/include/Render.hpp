#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences

class WindowAPI;
namespace GPU {
    class WGPU;
}

class Render {
    static constexpr std::string_view tag = "Render";
public:
    explicit Render(Lattice::Components& renderer);
    void setup();
    void frame();
    ~Render();

private:
    struct FrameState;
    std::unique_ptr<FrameState> frameState;

    void ensureSurface(WindowAPI& window);
    void resize(uint32_t w, uint32_t h);
    void releaseFrameResources();

    Lattice::Settings* settings = nullptr;
    Lattice::Slot<WindowAPI> window_;
    GPU::WGPU* gpu_ = nullptr;
};