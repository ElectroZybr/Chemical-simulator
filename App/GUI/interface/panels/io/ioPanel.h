#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "App/AppPaths.h"

#include "Lattice/Engine/physics/Atom/AtomData.h"
#include "GUI/interface/panels/io/ioPanelSceneCatalog.h"

class FileDialogManager;
namespace Lattice {
    class Simulation;
}
struct UiState;

class IOPanel {
public:
    struct MixedGasEntry {
        AtomData::Type type = AtomData::Type::Z;
        float concentrationPercent = 50.0f;
        int absoluteCount = 500;
    };

    enum class RecordingFormat : uint8_t {
        MP4,
        XYZ,
    };

    enum class GeneratorKind : uint8_t {
        Massive,
        Gas,
        MixedGas,
        HexLattice,
        TriangularBipyramid,
    };

    static constexpr ImGuiWindowFlags PANEL_FLAGS =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    void draw(float scale, glm::ivec2 windowSize, Lattice::Simulation& simulation, FileDialogManager& fileDialog, UiState& uiState);
    void setScenesDirectory(std::filesystem::path scenesDirectory);
    [[nodiscard]] const std::filesystem::path& scenesDirectory() const { return scenesDirectory_; }

    void toggle() { visible_ = !visible_; }
    void close() { visible_ = false; }
    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] int sceneAxisCount() const { return generatorAxisCounts_.x; }
    [[nodiscard]] bool sceneIs3D() const { return generatorIs3D_; }
    [[nodiscard]] int gasAtomCount() const { return generatorAtomCount_; }
    [[nodiscard]] bool gasIs3D() const { return generatorIs3D_; }
    [[nodiscard]] AtomData::Type atomType() const { return atomType_; }
    [[nodiscard]] AtomData::Type gasAtomType() const { return gasAtomType_; }
    [[nodiscard]] float gasDensity() const { return generatorDensity_; }

private:
    void ensureSceneCatalogLoaded();
    void clearPendingDeleteState();
    void removeSceneTileByPath(std::string_view path);

    uint8_t pendingReloadFrames_ = 0;
    bool visible_ = false;
    bool sceneCatalogLoaded_ = false;
    float animProgress_ = 0.f;
    glm::ivec3 generatorAxisCounts_ = glm::ivec3(25, 25, 25);
    int generatorAxisCount_ = 25;
    bool massiveSeparateAxes_ = true;
    bool hexSeparateAxes_ = true;
    int generatorAtomCount_ = 1000;
    bool generatorIs3D_ = true;
    float generatorDensity_ = 0.01f;
    int mixedGasLastTotalCount_ = 1000;
    GeneratorKind generatorKind_ = GeneratorKind::Massive;

    AtomData::Type atomType_ = AtomData::Type::Z;
    AtomData::Type gasAtomType_ = AtomData::Type::Z;
    AtomData::Type icAtomType_ = AtomData::Type::Z;
    AtomData::Type tbpAtomType_ = AtomData::Type::Z;
    std::vector<MixedGasEntry> mixedGasEntries_ = {
        {.type = AtomData::Type::Z, .concentrationPercent = 50.0f, .absoluteCount = 500},
        {.type = AtomData::Type::H, .concentrationPercent = 50.0f, .absoluteCount = 500},
    };
    RecordingFormat recordingFormat_ = RecordingFormat::MP4;
    std::filesystem::path scenesDirectory_ = AppPaths::kDefaultScenesDirectory;
    std::vector<IOPanelSceneTile> sceneTiles_;
    std::string pendingDeleteScenePath_;
    std::string pendingDeleteSceneTitle_;
    std::string pendingDeleteError_;
    ImVec2 pendingDeletePopupPos_ = ImVec2(0.0f, 0.0f);
};
