#include "ioPanelSceneCatalog.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include <webgpu/webgpu-raii.hpp>
#include <zstd.h>

#include "App/save_system/AppSaveState.h"
#include "Rendering/WGPUContext.h"

namespace {
    struct ParsedSceneInfo {
        std::string title;
        std::string description;
        unsigned imageWidth = 0;
        unsigned imageHeight = 0;
        wgpu::TextureFormat imageFormat;
        std::vector<std::byte> imageBytes;
        bool hasEmbeddedPreview = false;
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

    std::string valueAfterTag(std::string_view line, std::string_view tag) {
        if (!line.starts_with(tag)) {
            return {};
        }
        return trim(line.substr(tag.size()));
    }

    std::vector<std::byte> decodeBase64(std::string_view encoded) {
        std::array<int, 256> decodeTable{};
        decodeTable.fill(-1);

        constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) {
            decodeTable[static_cast<unsigned char>(alphabet[i])] = i;
        }

        std::vector<std::byte> decoded;
        decoded.reserve((encoded.size() / 4) * 3);

        int val = 0;
        int valb = -8;
        for (unsigned char c : encoded) {
            if (std::isspace(c)) {
                continue;
            }
            if (c == '=') {
                break;
            }
            const int decodedChar = decodeTable[c];
            if (decodedChar < 0) {
                continue;
            }
            val = (val << 6) + decodedChar;
            valb += 6;
            if (valb >= 0) {
                decoded.emplace_back(static_cast<std::byte>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }

        return decoded;
    }

    ParsedSceneInfo parseTxtSceneInfo(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return {};
        }

        ParsedSceneInfo info;
        std::string section;
        std::string imageEncoding;
        wgpu::TextureFormat imageFormat = wgpu::TextureFormat::RGBA8Unorm;
        std::string imageDataBase64;
        bool readingImageData = false;

        std::string line;
        while (std::getline(file, line)) {
            const std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed.starts_with('#')) {
                continue;
            }

            if (readingImageData) {
                if (trimmed == "data_end") {
                    readingImageData = false;
                }
                else {
                    imageDataBase64 += trimmed;
                }
                continue;
            }

            if (trimmed.front() == '[') {
                section = trimmed;
                continue;
            }

            if (section == "[meta]") {
                if (trimmed.starts_with("title ")) {
                    info.title = valueAfterTag(trimmed, "title");
                }
                else if (trimmed.starts_with("description ")) {
                    info.description = valueAfterTag(trimmed, "description");
                }
            }
            else if (section == "[image]") {
                try {
                    if (trimmed.starts_with("encoding ")) {
                        imageEncoding = valueAfterTag(trimmed, "encoding");
                    }
                    else if (trimmed.starts_with("format ")) {
                        imageFormat = wgpu::TextureFormat(static_cast<WGPUTextureFormat>(std::stoi(valueAfterTag(trimmed, "format"))));
                    }
                    else if (trimmed.starts_with("width ")) {
                        info.imageWidth = static_cast<unsigned>(std::max(0, std::stoi(valueAfterTag(trimmed, "width"))));
                    }
                    else if (trimmed.starts_with("height ")) {
                        info.imageHeight = static_cast<unsigned>(std::max(0, std::stoi(valueAfterTag(trimmed, "height"))));
                    }
                    else if (trimmed == "data_begin") {
                        readingImageData = true;
                    }
                }
                catch (const std::exception&) {
                    return {};
                }
            }
        }

        if (info.title.empty()) {
            info.title = path.stem().string();
        }

        if (imageEncoding == "base64" && info.imageWidth > 0 && info.imageHeight > 0) {
            info.imageFormat = imageFormat;
            info.imageBytes = decodeBase64(imageDataBase64);
            const size_t expectedSize = static_cast<size_t>(info.imageWidth) * static_cast<size_t>(info.imageHeight) * 4;
            info.hasEmbeddedPreview = (info.imageBytes.size() == expectedSize);
            if (!info.hasEmbeddedPreview) {
                info.imageBytes.clear();
            }
        }

        return info;
    }

    ParsedSceneInfo parseBinSceneInfo(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return {};
        }

        uint32_t originalSize = 0;
        if (!file.read(reinterpret_cast<char*>(&originalSize), sizeof(originalSize))) {
            return {};
        }

        constexpr size_t compressedChunkSize = 512 * 1024;
        std::vector<std::byte> compressedBuffer(compressedChunkSize);
        file.read(reinterpret_cast<char*>(compressedBuffer.data()), compressedChunkSize);
        size_t bytesRead = static_cast<size_t>(file.gcount());

        if (bytesRead == 0) {
            return {};
        }

        size_t decompressedLimit = std::min(size_t(originalSize), size_t(4 * 1024 * 1024));
        std::vector<std::byte> decompBuffer(decompressedLimit);

        ZSTD_decompress(decompBuffer.data(), decompBuffer.size(), compressedBuffer.data(), bytesRead);

        AppSaveHeader header;
        try {
            auto in = zpp::bits::in(decompBuffer);
            uint32_t _;
            in(_).or_throw(); // Пропускаем AppSaveState::version
            in(header).or_throw();
        }
        catch (const std::exception& e) {
            return {};
        }

        ParsedSceneInfo info;
        info.title = header.title;
        info.description = header.description;
        info.imageWidth = header.previewWidth;
        info.imageHeight = header.previewHeight;
        info.imageFormat = header.previewFormat;
        info.imageBytes = header.previewPixels;
        info.hasEmbeddedPreview = true;

        return info;
    }

    ParsedSceneInfo parseSceneInfo(const std::filesystem::path& path) {
        if (path.extension() == ".lat") {
            return parseTxtSceneInfo(path);
        }
        else if (path.extension() == ".latbin") {
            return parseBinSceneInfo(path);
        }
        return {};
    }
}

std::vector<IOPanelSceneTile> loadIOPanelSceneTiles(std::string_view scenesDirectory) {
    std::vector<IOPanelSceneTile> sceneTiles;

    const std::filesystem::path scenesDir = scenesDirectory.empty() ? std::filesystem::path(".") : std::filesystem::path(scenesDirectory);
    std::error_code fsError;
    if (!std::filesystem::exists(scenesDir, fsError) || fsError || !std::filesystem::is_directory(scenesDir, fsError) || fsError) {
        return sceneTiles;
    }

    std::vector<std::filesystem::path> scenePaths;
    for (std::filesystem::directory_iterator it(scenesDir, fsError), end; !fsError && it != end; it.increment(fsError)) {
        if (fsError) {
            break;
        }
        const auto& entry = *it;
        if (!entry.is_regular_file(fsError) || fsError) {
            continue;
        }
        if (entry.path().extension() == ".lat" || entry.path().extension() == ".latbin") {
            scenePaths.emplace_back(entry.path());
        }
    }

    std::sort(scenePaths.begin(), scenePaths.end());
    sceneTiles.reserve(scenePaths.size());

    for (const auto& path : scenePaths) {
        ParsedSceneInfo parsed = parseSceneInfo(path);

        IOPanelSceneTile tile;
        tile.path = path.string();
        tile.title = std::move(parsed.title);
        tile.description = std::move(parsed.description);

        if (parsed.hasEmbeddedPreview && parsed.imageWidth > 0 && parsed.imageHeight > 0) {
            if (!WGPUContext::instance().device()) {
                sceneTiles.emplace_back(std::move(tile));
                continue;
            }
            wgpu::TextureDescriptor texDesc{};
            texDesc.size = {parsed.imageWidth, parsed.imageHeight, 1};
            texDesc.format = parsed.imageFormat;
            texDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
            texDesc.mipLevelCount = 1;
            texDesc.sampleCount = 1;
            texDesc.dimension = wgpu::TextureDimension::_2D;
            wgpu::raii::Texture texture = WGPUContext::instance().device()->createTexture(texDesc);

            wgpu::TexelCopyTextureInfo dst{};
            dst.texture = *texture;
            dst.mipLevel = 0;
            dst.aspect = wgpu::TextureAspect::All;

            wgpu::TexelCopyBufferLayout layout{};
            layout.bytesPerRow = parsed.imageWidth * 4;
            layout.rowsPerImage = parsed.imageHeight;

            wgpu::Extent3D extent{parsed.imageWidth, parsed.imageHeight, 1};
            WGPUContext::instance().device()->getQueue().writeTexture(dst, parsed.imageBytes.data(), parsed.imageBytes.size(), layout,
                                                                      extent);

            tile.previewTexture = texture;
            tile.previewTextureView = texture->createView();
            tile.previewSize = Vec2u(parsed.imageWidth, parsed.imageHeight);
            tile.hasPreview = true;
        }

        sceneTiles.emplace_back(std::move(tile));
    }

    return sceneTiles;
}
