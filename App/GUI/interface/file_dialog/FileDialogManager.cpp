#include "FileDialogManager.h"

#include <filesystem>
#include <utility>

#include <ImGuiFileDialog.h>
#include <imgui.h>

#include "App/AppPaths.h"
#include "App/AppSignals.h"

namespace {
    std::string defaultSimulationPath(const std::string& currentPath) {
        if (!currentPath.empty() && std::filesystem::exists(currentPath)) {
            return currentPath;
        }
        constexpr std::string_view defaultScenesPath = AppPaths::kDefaultScenesDirectory;
        if (std::filesystem::exists(defaultScenesPath)) {
            return std::string(defaultScenesPath);
        }
        return ".";
    }
}

void FileDialogManager::openSave() {
    IGFD::FileDialogConfig config;
    config.path = defaultSimulationPath(simulationDirectory_);
    config.fileName = "scene";
    config.countSelectionMax = 1;
    config.flags = ImGuiFileDialogFlags_ConfirmOverwrite;
    ImGuiFileDialog::Instance()->OpenDialog("SaveDlg", "Save scene", ".latbin,.lat", config);
    saveDialogOpen_ = true;
}

void FileDialogManager::openLoad() {
    IGFD::FileDialogConfig config;
    config.path = defaultSimulationPath(simulationDirectory_);
    config.countSelectionMax = 1;
    ImGuiFileDialog::Instance()->OpenDialog("LoadDlg", "Load scene", ".latbin,.lat,.sim,.xyz", config);
}

void FileDialogManager::openCaptureDirectory(const std::string& currentPath) {
    IGFD::FileDialogConfig config;
    config.path = currentPath.empty() ? "." : currentPath;
    config.countSelectionMax = 1;
    ImGuiFileDialog::Instance()->OpenDialog("CaptureDirDlg", "Select capture directory", nullptr, config);
}

void FileDialogManager::openSceneDirectory(const std::string& currentPath) {
    IGFD::FileDialogConfig config;
    config.path = currentPath.empty() ? "." : currentPath;
    config.countSelectionMax = 1;
    ImGuiFileDialog::Instance()->OpenDialog("SceneDirDlg", "Select scenes directory", nullptr, config);
}

void FileDialogManager::setSimulationDirectory(const std::string& currentPath) { simulationDirectory_ = currentPath; }

std::string FileDialogManager::consumeSelectedSceneDirectory() {
    sceneDirectorySelected_ = false;
    return std::exchange(selectedSceneDirectory_, {});
}

std::string FileDialogManager::consumeSavedSimulationPath() {
    savedSimulationPathReady_ = false;
    return std::exchange(savedSimulationPath_, {});
}

void FileDialogManager::draw(float scale) {
    ImVec2 dlgSize(400 * scale, 300 * scale);

    if (ImGuiFileDialog::Instance()->Display("SaveDlg", ImGuiWindowFlags_NoCollapse, dlgSize)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            simulationDirectory_ = ImGuiFileDialog::Instance()->GetCurrentPath();
            savedSimulationPath_ = ImGuiFileDialog::Instance()->GetFilePathName();
            savedSimulationPathReady_ = true;
            AppSignals::UI::SaveSimulation.emit(savedSimulationPath_);
        }
        ImGuiFileDialog::Instance()->Close();
        saveDialogOpen_ = false;
    }

    if (ImGuiFileDialog::Instance()->Display("LoadDlg", ImGuiWindowFlags_NoCollapse, dlgSize)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            simulationDirectory_ = ImGuiFileDialog::Instance()->GetCurrentPath();
            AppSignals::UI::LoadSimulation.emit(ImGuiFileDialog::Instance()->GetFilePathName());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("CaptureDirDlg", ImGuiWindowFlags_NoCollapse, dlgSize)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            AppSignals::Capture::SetOutputDirectory.emit(ImGuiFileDialog::Instance()->GetCurrentPath());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("SceneDirDlg", ImGuiWindowFlags_NoCollapse, dlgSize)) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            selectedSceneDirectory_ = ImGuiFileDialog::Instance()->GetCurrentPath();
            sceneDirectorySelected_ = true;
            simulationDirectory_ = selectedSceneDirectory_;
        }
        ImGuiFileDialog::Instance()->Close();
    }
}
