#pragma once

// Kernel dependences
#include <Lattice/Kernel/Plugin.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Node.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences

class WindowAPI;
namespace GPU {
    class WGPU;
}

class Render {
    static constexpr std::string_view tag = "Render";
public:
    explicit Render(Lattice::Node& renderer);
    void configure(Lattice::Node& renderer);

    void setup();
    void frame();
    ~Render();

private:
    struct FrameState;
    std::unique_ptr<FrameState> frameState;

    void ensureSurface(WindowAPI& window);
    void resize(uint32_t w, uint32_t h);
    void releaseFrameResources();

    Ref<Lattice::Settings> settings_;
    Slot<WindowAPI> window_;
    Ref<GPU::WGPU> gpu_;
};