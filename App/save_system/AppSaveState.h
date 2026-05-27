#include <webgpu/webgpu-raii.hpp>
#include <zpp_bits.h>

#include "Engine/math/Vec3.h"
#include "Engine/physics/Integrator.h"
#include "Rendering/BaseRenderer.h"

template <typename T> constexpr auto serialize(auto& archive, const Vec3<T>& self) { return archive(self.x, self.y, self.z); }
template <typename T> constexpr auto serialize(auto& archive, Vec3<T>& self) { return archive(self.x, self.y, self.z); }

struct SimulationSaveState {
    uint32_t version = 1;

    // Симуляция
    float dt;
    float time_ns;
    uint64_t step;
    Integrator::Scheme integrator;

    // Силы
    Vec3f gravity;
    bool bondFormationEnabled;
    bool LJEnabled;
    bool coulombEnabled;

    // Сетка
    Vec3f boxSize;
    int gridCellSize;
    float neighborListCutoff;
    float neighborListSkin;
    float maxParticleSpeed;
    float accelDamping;

    // Атомы
    uint64_t atomMobileCount = 0;
    std::vector<float> x;
    std::vector<float> y;
    std::vector<float> z;
    std::vector<float> vx;
    std::vector<float> vy;
    std::vector<float> vz;
    std::vector<AtomData::Type> atomType;
    std::vector<float> atomCharge;

    // Связи
    std::vector<std::pair<uint64_t, uint64_t>> bonds;

    constexpr static zpp::bits::errc serialize(auto& archive, auto& self) {
        if (auto err = archive(self.version); zpp::bits::failure(err.code)) {
            return err;
        }

        // v1
        if (auto err = archive(self.dt, self.time_ns, self.step, self.integrator, self.gravity, self.bondFormationEnabled, self.LJEnabled,
                               self.coulombEnabled, self.boxSize, self.gridCellSize, self.neighborListCutoff, self.neighborListSkin,
                               self.maxParticleSpeed, self.accelDamping, self.atomMobileCount, self.x, self.y, self.z, self.vx, self.vy,
                               self.vz, self.atomType, self.atomCharge, self.bonds);
            zpp::bits::failure(err.code)) {
            return err;
        }

        // v2: if (self.version >= 2) { ... }

        return zpp::bits::errc{};
    }
};

struct RendererSaveState {
    uint32_t version = 2;

    bool drawGrid = false;
    bool drawBonds = false;
    bool drawBox = true;
    IRenderer::SpeedColorMode speedColorMode = IRenderer::SpeedColorMode::AtomColor;
    float speedGradientMax = 5.0f;
    float alpha = 0.05f;

    constexpr static zpp::bits::errc serialize(auto& archive, auto& self) {
        if (auto err = archive(self.version); zpp::bits::failure(err.code)) {
            return err;
        }

        // v1
        if (self.version == 1) {
            if (auto err = archive(self.drawGrid, self.drawBonds, self.speedColorMode, self.speedGradientMax, self.alpha);
                zpp::bits::failure(err.code)) {
                return err;
            }
            self.drawBox = true;
            return zpp::bits::errc{};
        }

        // v2
        if (self.version >= 2) {
            if (auto err = archive(self.drawGrid, self.drawBonds, self.drawBox, self.speedColorMode, self.speedGradientMax, self.alpha);
                zpp::bits::failure(err.code)) {
                return err;
            }
        }

        return zpp::bits::errc{};
    }
};

struct AppSaveHeader {
    uint32_t version = 1;

    std::string title;
    std::string description;
    uint32_t previewWidth = 0;
    uint32_t previewHeight = 0;
    WGPUTextureFormat previewFormat;
    std::vector<std::byte> previewPixels;

    constexpr static zpp::bits::errc serialize(auto& archive, auto& self) {
        if (auto err = archive(self.version); zpp::bits::failure(err.code)) {
            return err;
        }

        if (auto err = archive(self.title, self.description, self.previewWidth, self.previewHeight, self.previewFormat, self.previewPixels);
            zpp::bits::failure(err.code)) {
            return err;
        }

        // v2: if (self.version >= 2) { ... }

        return zpp::bits::errc{};
    }
};

struct AppSaveState {
    uint32_t version = 1;

    AppSaveHeader header;
    SimulationSaveState simulation;
    RendererSaveState renderer;

    constexpr static zpp::bits::errc serialize(auto& archive, auto& self) {
        if (auto err = archive(self.version); zpp::bits::failure(err.code)) {
            return err;
        }

        // v1
        if (auto err = archive(self.header, self.simulation, self.renderer); zpp::bits::failure(err.code)) {
            return err;
        }

        // v2: if (self.version >= 2) { ... }

        return zpp::bits::errc{};
    }
};
