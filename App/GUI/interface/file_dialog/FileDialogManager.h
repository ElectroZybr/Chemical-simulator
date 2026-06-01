#pragma once

#include <string>

class FileDialogManager {
public:
    void openSave();
    void openLoad();
    void openCaptureDirectory(const std::string& currentPath);
    void openSceneDirectory(const std::string& currentPath);
    void setSimulationDirectory(const std::string& currentPath);
    void draw(float scale);
    [[nodiscard]] bool isSaveDialogOpen() const { return saveDialogOpen_; }
    [[nodiscard]] bool hasSelectedSceneDirectory() const { return sceneDirectorySelected_; }
    [[nodiscard]] bool hasSavedSimulationPath() const { return savedSimulationPathReady_; }
    std::string consumeSelectedSceneDirectory();
    std::string consumeSavedSimulationPath();

private:
    bool saveDialogOpen_ = false;
    bool sceneDirectorySelected_ = false;
    bool savedSimulationPathReady_ = false;
    std::string selectedSceneDirectory_;
    std::string savedSimulationPath_;
    std::string simulationDirectory_;
};
