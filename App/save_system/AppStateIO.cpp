#include "AppStateIO.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <zstd.h>

#include "App/capture/CaptureController.h"
#include "App/save_system/AppSaveState.h"
#include "Engine/Simulation.h"
#include "Engine/io/SimulationStateIO.h"
#include "GUI/interface/UiState.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/WGPUContext.h"

namespace {
    constexpr const char* kBlockIndent = "  ";
    constexpr unsigned kPreviewMaxSize = 256;

    struct LoadedRendererData {
        bool drawGrid = false;
        bool drawBonds = false;
        bool drawBox = true;
        int speedColorMode = static_cast<int>(IRenderer::SpeedColorMode::AtomColor);
        float speedGradientMax = 5.0f;
        float alpha = 0.05f;
    };

    std::string trim(std::string_view value) {
        size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
            ++begin;
        }

        size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
            --end;
        }

        return std::string(value.substr(begin, end - begin));
    }

    std::string encodeBase64(std::span<const std::byte> data) {
        static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        const size_t input_len = data.size();
        if (input_len == 0) {
            return "";
        }

        const size_t output_len = ((input_len + 2) / 3) * 4;
        std::string encoded;
        encoded.resize(output_len);

        const std::byte* bytes = data.data();
        char* dst = &encoded[0];

        size_t i = 0;

        const size_t fast_len = (input_len / 3) * 3;
        for (; i < fast_len; i += 3) {
            uint32_t n =
                (static_cast<uint32_t>(bytes[i]) << 16) | (static_cast<uint32_t>(bytes[i + 1]) << 8) | static_cast<uint32_t>(bytes[i + 2]);

            dst[0] = kAlphabet[(n >> 18) & 0x3F];
            dst[1] = kAlphabet[(n >> 12) & 0x3F];
            dst[2] = kAlphabet[(n >> 6) & 0x3F];
            dst[3] = kAlphabet[n & 0x3F];
            dst += 4;
        }

        if (i < input_len) {
            uint32_t n = static_cast<uint32_t>(bytes[i]) << 16;
            if (i + 1 < input_len) {
                n |= static_cast<uint32_t>(bytes[i + 1]) << 8;
            }

            dst[0] = kAlphabet[(n >> 18) & 0x3F];
            dst[1] = kAlphabet[(n >> 12) & 0x3F];
            dst[2] = (i + 1 < input_len) ? kAlphabet[(n >> 6) & 0x3F] : '=';
            dst[3] = '=';
        }

        return encoded;
    }

    ImageData capturePreviewImage(uint32_t srcWidth, uint32_t srcHeight, const void* rawData, uint32_t pitch, bool yflip,
                                  const PreviewFrameRect& previewRect) {
        if (srcWidth == 0 || srcHeight == 0 || rawData == nullptr) {
            return {};
        }

        const std::byte* src = static_cast<const std::byte*>(rawData);

        const unsigned cropX =
            std::min(srcWidth - 1, static_cast<unsigned>(std::clamp(previewRect.x, 0.0f, static_cast<float>(srcWidth - 1))));
        const unsigned cropY =
            std::min(srcHeight - 1, static_cast<unsigned>(std::clamp(previewRect.y, 0.0f, static_cast<float>(srcHeight - 1))));
        const unsigned cropWidth = std::max(
            1u, std::min(srcWidth - cropX, static_cast<unsigned>(std::clamp(previewRect.width, 1.0f, static_cast<float>(srcWidth)))));
        const unsigned cropHeight = std::max(
            1u, std::min(srcHeight - cropY, static_cast<unsigned>(std::clamp(previewRect.height, 1.0f, static_cast<float>(srcHeight)))));

        const float scale = std::min(1.0f, std::min(static_cast<float>(kPreviewMaxSize) / static_cast<float>(cropWidth),
                                                    static_cast<float>(kPreviewMaxSize) / static_cast<float>(cropHeight)));
        const unsigned previewWidth = std::max(1u, static_cast<unsigned>(cropWidth * scale));
        const unsigned previewHeight = std::max(1u, static_cast<unsigned>(cropHeight * scale));

        ImageData result;
        result.width = previewWidth;
        result.height = previewHeight;
        result.pixels.resize(static_cast<size_t>(previewWidth) * previewHeight * 4);

        for (unsigned y = 0; y < previewHeight; ++y) {
            for (unsigned x = 0; x < previewWidth; ++x) {
                unsigned srcSampleX =
                    cropX + std::min(cropWidth - 1, static_cast<unsigned>((static_cast<std::uint64_t>(x) * cropWidth) / previewWidth));
                unsigned srcSampleY =
                    cropY + std::min(cropHeight - 1, static_cast<unsigned>((static_cast<std::uint64_t>(y) * cropHeight) / previewHeight));

                unsigned flippedY = yflip ? (srcHeight - 1 - srcSampleY) : srcSampleY;

                const std::byte* srcPixel = src + flippedY * pitch + srcSampleX * 4;
                std::byte* dstPixel = result.pixels.data() + (y * previewWidth + x) * 4;

                dstPixel[0] = srcPixel[0];
                dstPixel[1] = srcPixel[1];
                dstPixel[2] = srcPixel[2];
                dstPixel[3] = srcPixel[3];
            }
        }
        return result;
    }

    void saveImageState(CaptureController& captureController, const PreviewFrameRect& previewRect, std::string_view path) {
        auto& ctx = WGPUContext::instance();

        captureController.requestScreenshot([previewRect, filePath = std::string(path), format = ctx.surfaceFormat()](ImageData img) {
            const uint32_t pitch = img.width * 4;
            const ImageData preview = capturePreviewImage(img.width, img.height, img.pixels.data(), pitch, false, previewRect);
            if (preview.width == 0 || preview.height == 0) {
                return;
            }

            const size_t byteCount = static_cast<size_t>(preview.width) * preview.height * 4;
            const std::string encoded = encodeBase64({preview.pixels.data(), byteCount});

            std::ofstream file(filePath, std::ios::app);
            if (!file.is_open()) {
                return;
            }

            file << "\n[image]\n";
            file << kBlockIndent << "encoding base64\n";
            file << kBlockIndent << "format " << static_cast<int>(img.format) << "\n";
            file << kBlockIndent << "width " << preview.width << "\n";
            file << kBlockIndent << "height " << preview.height << "\n";
            file << kBlockIndent << "data_begin\n";

            constexpr size_t lineWidth = 120;
            for (size_t offset = 0; offset < encoded.size(); offset += lineWidth) {
                file << kBlockIndent << encoded.substr(offset, lineWidth) << "\n";
            }

            file << kBlockIndent << "data_end\n";
        });
    }

    void saveRendererState(const IRenderer& renderer, std::string_view path) {
        std::ofstream file(path.data(), std::ios::app);
        if (!file.is_open()) {
            return;
        }

        file << "\n[renderer]\n";
        file << kBlockIndent << "draw_grid " << static_cast<int>(renderer.drawGrid) << "\n";
        file << kBlockIndent << "draw_bonds " << static_cast<int>(renderer.drawBonds) << "\n";
        file << kBlockIndent << "draw_box " << static_cast<int>(renderer.drawBox) << "\n";
        file << kBlockIndent << "speed_color_mode " << static_cast<int>(renderer.speedColorMode) << "\n";
        file << kBlockIndent << "speed_gradient_max " << renderer.speedGradientMax << "\n";
        file << kBlockIndent << "renderer_alpha " << renderer.alpha << "\n";
    }

    void loadRendererState(IRenderer& renderer, std::string_view path) {
        std::ifstream file(path.data());
        if (!file.is_open()) {
            return;
        }

        LoadedRendererData loadedRenderer{
            .drawGrid = renderer.drawGrid,
            .drawBonds = renderer.drawBonds,
            .drawBox = renderer.drawBox,
            .speedColorMode = static_cast<int>(renderer.speedColorMode),
            .speedGradientMax = renderer.speedGradientMax,
            .alpha = renderer.alpha,
        };

        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == '[') {
                continue;
            }

            std::istringstream stream(trimmed);
            std::string tag;
            stream >> tag;

            if (tag == "draw_grid") {
                int value = 0;
                stream >> value;
                loadedRenderer.drawGrid = (value != 0);
            }
            else if (tag == "draw_bonds") {
                int value = 0;
                stream >> value;
                loadedRenderer.drawBonds = (value != 0);
            }
            else if (tag == "draw_box") {
                int value = 0;
                stream >> value;
                loadedRenderer.drawBox = (value != 0);
            }
            else if (tag == "speed_color_mode") {
                stream >> loadedRenderer.speedColorMode;
            }
            else if (tag == "speed_gradient_max") {
                stream >> loadedRenderer.speedGradientMax;
            }
            else if (tag == "renderer_alpha") {
                stream >> loadedRenderer.alpha;
            }
        }

        renderer.drawGrid = loadedRenderer.drawGrid;
        renderer.drawBonds = loadedRenderer.drawBonds;
        renderer.drawBox = loadedRenderer.drawBox;
        renderer.speedColorMode = static_cast<IRenderer::SpeedColorMode>(loadedRenderer.speedColorMode);
        renderer.speedGradientMax = loadedRenderer.speedGradientMax;
        renderer.alpha = loadedRenderer.alpha;
    }
}

void AppStateIO::save(CaptureController& captureController, const PreviewFrameRect& previewRect, const Simulation& simulation,
                      const IRenderer& renderer, std::string_view path) {
    if (path.ends_with(".lat")) {
        AppStateIO::saveText(captureController, previewRect, simulation, renderer, path);
    }
    else if (path.ends_with(".latbin")) {
        AppStateIO::saveBinary(captureController, previewRect, simulation, renderer, path);
    }
}

void AppStateIO::load(Simulation& simulation, IRenderer& renderer, std::string_view path) {
    if (path.ends_with(".lat")) {
        AppStateIO::loadText(simulation, renderer, path);
    }
    else if (path.ends_with(".latbin")) {
        AppStateIO::loadBinary(simulation, renderer, path);
    }
}

void AppStateIO::saveText(CaptureController& captureController, const PreviewFrameRect& previewRect, const Simulation& simulation,
                          const IRenderer& renderer, std::string_view path) {
    SimulationStateIO::save(simulation, path);
    saveRendererState(renderer, path);
    saveImageState(captureController, previewRect, path);
}

void AppStateIO::saveBinary(CaptureController& captureController, const PreviewFrameRect& previewRect, const Simulation& simulation,
                            const IRenderer& renderer, std::string_view path) {
    SimulationSaveState simState;
    simState.dt = simulation.getDt();
    simState.time_ns = simulation.simTimeNs();
    simState.step = simulation.getSimStep();
    simState.integrator = simulation.getIntegrator();
    simState.gravity = simulation.getGravity();
    simState.bondFormationEnabled = simulation.isBondFormationEnabled();
    simState.LJEnabled = simulation.isLJEnabled();
    simState.coulombEnabled = simulation.isCoulombEnabled();
    simState.boxSize = simulation.world().getWorldSize();
    simState.gridCellSize = simulation.world().getGridCellSize();
    simState.neighborListCutoff = simulation.getNeighborListCutoff();
    simState.neighborListSkin = simulation.getNeighborListSkin();
    simState.maxParticleSpeed = simulation.getMaxParticleSpeed();
    simState.accelDamping = simulation.getAccelDamping();
    simState.atomMobileCount = simulation.atoms().mobileCount();
    simState.x.assign(simulation.atoms().xDataSpan().begin(), simulation.atoms().xDataSpan().end());
    simState.y.assign(simulation.atoms().yDataSpan().begin(), simulation.atoms().yDataSpan().end());
    simState.z.assign(simulation.atoms().zDataSpan().begin(), simulation.atoms().zDataSpan().end());
    simState.vx.assign(simulation.atoms().vxDataSpan().begin(), simulation.atoms().vxDataSpan().end());
    simState.vy.assign(simulation.atoms().vyDataSpan().begin(), simulation.atoms().vyDataSpan().end());
    simState.vz.assign(simulation.atoms().vzDataSpan().begin(), simulation.atoms().vzDataSpan().end());
    simState.atomType.assign(simulation.atoms().atomTypeDataSpan().begin(), simulation.atoms().atomTypeDataSpan().end());
    simState.atomCharge.assign(simulation.atoms().chargeDataSpan().begin(), simulation.atoms().chargeDataSpan().end());

    auto view = simulation.bonds() | std::views::transform([](const Bond& b) { return std::pair{b.aIndex, b.bIndex}; });
    simState.bonds.assign(view.begin(), view.end());

    RendererSaveState rendState;
    rendState.drawGrid = renderer.drawGrid;
    rendState.drawBonds = renderer.drawBonds;
    rendState.drawBox = renderer.drawBox;
    rendState.speedColorMode = renderer.speedColorMode;
    rendState.speedGradientMax = renderer.speedGradientMax;
    rendState.alpha = renderer.alpha;

    AppSaveState appState;
    appState.simulation = std::move(simState);
    appState.renderer = std::move(rendState);

    std::string title = simulation.worldTitle();
    if (title.empty() && !path.empty()) {
        title = std::filesystem::path(path).stem().string();
    }
    appState.header.title = title;
    appState.header.description = simulation.worldDescription();

    captureController.requestScreenshot([previewRect, filePath = std::string(path), appState = std::move(appState)](ImageData img) mutable {
        const uint32_t pitch = img.width * 4;
        const ImageData preview = capturePreviewImage(img.width, img.height, img.pixels.data(), pitch, false, previewRect);

        appState.header.previewWidth = preview.width;
        appState.header.previewHeight = preview.height;
        appState.header.previewFormat = img.format;

        if (preview.width > 0 && preview.height > 0) {
            const size_t byteCount = static_cast<size_t>(preview.width) * preview.height * 4;
            const auto* pixels = reinterpret_cast<const std::byte*>(preview.pixels.data());
            appState.header.previewPixels.assign(pixels, pixels + byteCount);
        }

        auto [bytes, out] = zpp::bits::data_out();
        try {
            out(appState).or_throw();
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to serialize app state: " << e.what() << "\n";
            return;
        }

        const size_t maxCompressedSize = ZSTD_compressBound(bytes.size());
        std::vector<std::byte> compressed(maxCompressedSize);
        const size_t compressedSize = ZSTD_compress(compressed.data(), compressed.size(), bytes.data(), bytes.size(), 3);
        if (ZSTD_isError(compressedSize)) {
            std::cerr << "Failed to compress: " << ZSTD_getErrorName(compressedSize) << "\n";
            return;
        }

        std::ofstream file(filePath, std::ios::binary);
        uint32_t originalSize = static_cast<uint32_t>(bytes.size());
        file.write(reinterpret_cast<const char*>(&originalSize), sizeof(originalSize));
        file.write(reinterpret_cast<const char*>(compressed.data()), compressedSize);
    });
}

void AppStateIO::loadText(Simulation& simulation, IRenderer& renderer, std::string_view path) {
    SimulationStateIO::load(simulation, path);
    loadRendererState(renderer, path);
    renderer.camera.resetView();
}

void AppStateIO::loadBinary(Simulation& simulation, IRenderer& renderer, std::string_view path) {
    std::ifstream file(path.data(), std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("Failed to open save file: " + std::string(path));
    }

    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    uint32_t originalSize = 0;
    file.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize));

    size_t compressedSize = static_cast<size_t>(fileSize) - sizeof(uint32_t);
    std::vector<std::byte> compressedBuffer(compressedSize);
    if (!file.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedSize)) {
        throw std::runtime_error("Failed to read compressed data");
    }

    std::vector<std::byte> decompressedBuffer(originalSize);
    size_t const dSize = ZSTD_decompress(decompressedBuffer.data(), originalSize, compressedBuffer.data(), compressedSize);
    if (ZSTD_isError(dSize)) {
        throw std::runtime_error(std::string("Zstd decompression failed: ") + ZSTD_getErrorName(dSize));
    }

    if (dSize != originalSize) {
        throw std::runtime_error("Decompressed size mismatch");
    }

    AppSaveState appState{};
    try {
        auto in = zpp::bits::in(decompressedBuffer);
        in(appState).or_throw();
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to deserialize save file: " << e.what() << std::endl;
        return;
    }

    // Заголовок
    const auto& header = appState.header;
    simulation.setWorldTitle(header.title);
    simulation.setWorldDescription(header.description);

    // Симуляция
    const auto& simState = appState.simulation;

    simulation.clear();

    simulation.setSizeBox(simState.boxSize, simState.gridCellSize);
    simulation.setNeighborListCutoff(simState.neighborListCutoff);
    simulation.setNeighborListSkin(simState.neighborListSkin);

    simulation.setDt(simState.dt);
    simulation.setIntegrator(simState.integrator);
    simulation.setGravity(simState.gravity);
    simulation.setBondFormationEnabled(simState.bondFormationEnabled);
    simulation.setLJEnabled(simState.LJEnabled);
    simulation.setCoulombEnabled(simState.coulombEnabled);
    simulation.setMaxParticleSpeed(simState.maxParticleSpeed);
    simulation.setAccelDamping(simState.accelDamping);

    const uint64_t atomMobileCount = simState.atomMobileCount;
    const uint64_t atomCount = simState.x.size();

    AtomStorage& atoms = simulation.atoms();
    simulation.reserveAtoms(atoms.size());
    atoms.init(atomCount, atomMobileCount, simState.x, simState.y, simState.z, simState.vx, simState.vy, simState.vz, simState.atomType,
               simState.atomCharge);
    simulation.finalizeAtomBatch();

    for (const auto& [aIndex, bIndex] : simState.bonds) {
        simulation.addBond(aIndex, bIndex);
    }
    simulation.restoreRuntimeState(simState.step, simState.time_ns);

    // Рендер
    const auto& rendState = appState.renderer;
    renderer.drawGrid = rendState.drawGrid;
    renderer.drawBonds = rendState.drawBonds;
    renderer.drawBox = rendState.drawBox;
    renderer.speedColorMode = rendState.speedColorMode;
    renderer.speedGradientMax = rendState.speedGradientMax;
    renderer.alpha = rendState.alpha;
    renderer.camera.resetView();
}
