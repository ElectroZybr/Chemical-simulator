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
    enum class RecordingFormat : uint8_t {
        MP4,
        XYZ,
    };

    static constexpr ImGuiWindowFlags PANEL_FLAGS =
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

    void draw(float scale, glm::ivec2 windowSize, Lattice::Simulation& simulation, FileDialogManager& fileDialog, UiState& uiState);
    void setScenesDirectory(std::filesystem::path scenesDirectory);
    [[nodiscard]] const std::filesystem::path& scenesDirectory() const { return scenesDirectory_; }

    void toggle() { visible_ = !visible_; }
    void close() { visible_ = false; }
    [[nodiscard]] bool isVisible() const { return visible_; }
    [[nodiscard]] int sceneAxisCount() const { return sceneAxisCount_; }
    [[nodiscard]] bool sceneIs3D() const { return sceneIs3D_; }
    [[nodiscard]] int gasAtomCount() const { return gasAtomCount_; }
    [[nodiscard]] bool gasIs3D() const { return gasIs3D_; }
    [[nodiscard]] AtomData::Type atomType() const { return atomType_; }
    [[nodiscard]] AtomData::Type gasAtomType() const { return gasAtomType_; }
    [[nodiscard]] float gasDensity() const { return gasDensity_; }

private:
    void ensureSceneCatalogLoaded();
    void clearPendingDeleteState();
    void removeSceneTileByPath(std::string_view path);

    uint8_t pendingReloadFrames_ = 0;
    bool visible_ = false;
    bool sceneCatalogLoaded_ = false;
    float animProgress_ = 0.f;
    int sceneAxisCount_ = 25;
    bool sceneIs3D_ = true;
    int gasAtomCount_ = 1000;
    bool gasIs3D_ = false;
    float gasDensity_ = 1.0f;
    AtomData::Type atomType_ = AtomData::Type::Z;
    AtomData::Type gasAtomType_ = AtomData::Type::Z;
    RecordingFormat recordingFormat_ = RecordingFormat::MP4;
    std::filesystem::path scenesDirectory_ = AppPaths::kDefaultScenesDirectory;
    std::vector<IOPanelSceneTile> sceneTiles_;
    std::string pendingDeleteScenePath_;
    std::string pendingDeleteSceneTitle_;
    std::string pendingDeleteError_;
    ImVec2 pendingDeletePopupPos_ = ImVec2(0.0f, 0.0f);
};
