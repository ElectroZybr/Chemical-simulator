#pragma once

#include "Render.hpp"

// Plugin dependences
#include "WGPU.hpp"


Render::Render(Lattice::Components& renderer) {
    settings = renderer.require<Lattice::Settings>();
    gpu = renderer.addComponent<GPU::WGPU>();
    Logger::info("Render", "render created");
}

void Render::configure() {
    // surface/device после того, как окно уже есть
    // gpu->init(window_.handle(), w, h);  // если у тебя такой API
}

void Render::run() {
    // main thread: только события окна
    while (!stopRequested()) {
        Logger::info("Render", "looping");

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

Render::~Render() {
    stop();
    Logger::info("Render", "destroying object");
}