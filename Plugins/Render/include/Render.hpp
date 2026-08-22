#pragma once

// Kernel dependences
#include <Lattice/Kernel/PluginAPI.hpp>
#include <Lattice/Kernel/ServiceAPI.hpp>
#include <Lattice/Kernel/Components.hpp>
#include <Lattice/Kernel/Settings.hpp>

// Plugin dependences
namespace GPU {
    class WGPU;
}

class Render final : public ServiceAPI {
public:
    explicit Render(Lattice::Components& renderer);
    void configure();
    void run() override;
    ~Render();

private:
    Lattice::Component<Lattice::Settings> settings;
    Lattice::Component<GPU::WGPU> gpu;
};