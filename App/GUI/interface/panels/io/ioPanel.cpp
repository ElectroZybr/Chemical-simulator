#include "ioPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include "App/AppSignals.h"
#include "Lattice/Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "GUI/interface/file_dialog/FileDialogManager.h"
#include "GUI/interface/panels/io/ioPanelWidgets.h"

namespace {
    constexpr float kSceneTileRounding = 10.0f;
    constexpr int kRecordingFormatVideo = static_cast<int>(IOPanel::RecordingFormat::MP4);
    constexpr int kRecordingFormatXYZ = static_cast<int>(IOPanel::RecordingFormat::XYZ);
}

void IOPanel::ensureSceneCatalogLoaded() {
    if (sceneCatalogLoaded_) {
        return;
    }

    try {
        sceneTiles_ = loadIOPanelSceneTiles(scenesDirectory_.string());
    }
    catch (const std::exception&) {
        sceneTiles_.clear();
    }
    sceneCatalogLoaded_ = true;
}

void IOPanel::clearPendingDeleteState() {
    pendingDeleteScenePath_.clear();
    pendingDeleteSceneTitle_.clear();
    pendingDeleteError_.clear();
}

void IOPanel::setScenesDirectory(std::filesystem::path scenesDirectory) {
    scenesDirectory_ = std::move(scenesDirectory);
    sceneCatalogLoaded_ = false;
}

void IOPanel::removeSceneTileByPath(std::string_view path) {
    const auto it =
        std::find_if(sceneTiles_.begin(), sceneTiles_.end(), [&](const IOPanelSceneTile& sceneTile) { return sceneTile.path == path; });
    if (it != sceneTiles_.end()) {
        sceneTiles_.erase(it);
    }
}

void IOPanel::draw(float scale, Vec2i windowSize, Lattice::Simulation& simulation, FileDialogManager& fileDialog, UiState& uiState) {
    const float target = visible_ ? 1.f : 0.f;
    const float step = ImGui::GetIO().DeltaTime * 12.f;
    animProgress_ += (target - animProgress_) * std::min(step, 1.f);

    if (animProgress_ < 0.01f) {
        return;
    }

    if (fileDialog.hasSelectedSceneDirectory()) {
        setScenesDirectory(fileDialog.consumeSelectedSceneDirectory());
    }
    if (fileDialog.hasSavedSimulationPath()) {
        const std::filesystem::path savedSimulationPath = fileDialog.consumeSavedSimulationPath();
        if (savedSimulationPath.parent_path().lexically_normal() == scenesDirectory_.lexically_normal()) {
            pendingReloadFrames_ = 1;
        }
    }
    // Задержка, что бы изображение успело записаться в файл сохранения .lat
    if (pendingReloadFrames_ > 0 && --pendingReloadFrames_ == 0) {
        sceneCatalogLoaded_ = false;
    }

    fileDialog.setSimulationDirectory(scenesDirectory_.string());
    ensureSceneCatalogLoaded();
    boxSize_ = simulation.world().getWorldSize();

    const float panelWidth = 300.f * scale;
    const float topOffset = 65.f * scale;
    const float panelHeight = static_cast<float>(windowSize.y) - topOffset;
    const float x = -panelWidth + panelWidth * animProgress_;
    const float buttonWidth = 140.f;
    const float saveButtonWidth = 84.f;

    ImGui::SetNextWindowPos(ImVec2(x, topOffset));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("##IOPanel", nullptr, PANEL_FLAGS);

    ImGui::SeparatorText("Файл");
    if (ImGui::Button("Загрузить", ImVec2(saveButtonWidth * scale, 0.f))) {
        fileDialog.openLoad();
    }
    ImGui::SameLine();
    if (ImGui::Button("Сохранить", ImVec2(saveButtonWidth * scale, 0.f))) {
        fileDialog.openSave();
    }
    ImGui::SameLine();
    if (ImGui::Button("Очистить", ImVec2(saveButtonWidth * scale, 0.f))) {
        AppSignals::UI::ClearSimulation.emit();
    }

    int recordingFormat = static_cast<int>(recordingFormat_);
    drawIOPanelRecordingFormatCombo("##recording_format", recordingFormat, saveButtonWidth * scale, scale);
    recordingFormat_ = static_cast<RecordingFormat>(recordingFormat);

    const bool videoSelected = recordingFormat == kRecordingFormatVideo;
    const bool xyzSelected = recordingFormat == kRecordingFormatXYZ;
    const bool videoRecording = uiState.captureRecording;
    const bool xyzRecording = uiState.xyzRecording;
    const bool selectedRecording = videoSelected ? videoRecording : xyzRecording;
    const bool selectedFormatAvailable = videoSelected ? uiState.captureAvailable : true;
    const char* captureLabel = selectedRecording ? "Стоп" : "Запись";

    ImGui::SameLine();
    ImGui::BeginDisabled(!selectedFormatAvailable);
    if (ImGui::Button(captureLabel, ImVec2(saveButtonWidth * scale, 0.f))) {
        if (videoSelected) {
            AppSignals::Capture::ToggleRecording.emit();
        }
        else {
            AppSignals::Capture::ToggleXYZRecording.emit();
        }
    }
    ImGui::EndDisabled();

    if (videoSelected) {
        if (uiState.captureAvailable) {
            drawIOPanelCaptureStatus(uiState);
        }
    }
    else if (xyzSelected) {
        drawIOPanelRecordingStatusLine(xyzRecording, uiState.xyzFps, uiState.xyzFrameCount);
    }

    ImGui::SeparatorText("Размер бокса");
    bool boxSizeChanged = false;
    const auto drawBoxSizeDrag = [&](const char* label, const char* id, float& value) {
        ImGui::SetNextItemWidth(150.0f * scale);
        const bool changed = ImGui::DragFloat(id, &value, 0.5f, 1.0f, 0.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
        return changed;
    };
    boxSizeChanged |= drawBoxSizeDrag("Size X", "##box_size_x", boxSize_.x);
    boxSizeChanged |= drawBoxSizeDrag("Size Y", "##box_size_y", boxSize_.y);
    boxSizeChanged |= drawBoxSizeDrag("Size Z", "##box_size_z", boxSize_.z);

    if (boxSizeChanged) {
        boxSize_.x = std::max(boxSize_.x, 1.0f);
        boxSize_.y = std::max(boxSize_.y, 1.0f);
        boxSize_.z = std::max(boxSize_.z, 1.0f);
        AppSignals::UI::ResizeBox.emit(boxSize_);
    }

    ImGui::SeparatorText("Массивогенератор");
    ImGui::SliderInt("##atoms_per_axis", &sceneAxisCount_, 2, 200);
    ImGui::SameLine();
    drawIOPanelAtomTypeCombo("##atom_type", atomType_, 80.f * scale, scale);

    if (ImGui::Button("Создать##crystal", ImVec2(buttonWidth * scale, 0.f))) {
        AppSignals::UI::CreateCrystal.emit(sceneAxisCount_, atomType_, sceneIs3D_);
    }
    ImGui::SameLine();
    ImGui::Checkbox("3D", &sceneIs3D_);

    ImGui::SeparatorText("Газогенератор");
    ImGui::SliderInt("##gas_atom_count", &gasAtomCount_, 100, 300000);
    ImGui::SameLine();
    drawIOPanelAtomTypeCombo("##atom_type_gas", gasAtomType_, 80.f * scale, scale);
    if (ImGui::Button("Создать##gas", ImVec2(buttonWidth * scale, 0.f))) {
        AppSignals::UI::CreateGas.emit(gasAtomCount_, gasAtomType_, gasIs3D_, gasDensity_);
    }
    ImGui::SameLine();
    ImGui::Checkbox("3D##gas", &gasIs3D_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f * scale);
    ImGui::SliderFloat("##gas_density", &gasDensity_, 0.25f, 3.0f, "%.2f");

    ImGui::SeparatorText("Сцены");
    std::array<char, 512> scenesDirBuffer{};
    const std::string scenesDir = scenesDirectory_.string();
    std::snprintf(scenesDirBuffer.data(), scenesDirBuffer.size(), "%s", scenesDir.data());
    const float browseButtonWidth = ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseButtonWidth - ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputText("##scenes_dir", scenesDirBuffer.data(), scenesDirBuffer.size(), ImGuiInputTextFlags_ReadOnly);
    ImGui::SameLine();
    if (ImGui::Button("...##scenes_dir_browse", ImVec2(browseButtonWidth, 0.0f))) {
        fileDialog.openSceneDirectory(scenesDir);
    }

    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float tileSpacing = ImGui::GetStyle().ItemSpacing.x;
    const float tileWidth = std::max(80.0f, (availableWidth - tileSpacing) * 0.5f);
    const float tileHeight = tileWidth * 0.78f;
    const ImVec2 previewSize(tileWidth, tileHeight);
    bool openDeleteConfirmation = false;
    constexpr const char* kDeletePopupId = "DeleteSceneConfirm";

    for (size_t i = 0; i < sceneTiles_.size();) {
        IOPanelSceneTile& tile = sceneTiles_[i];
        ImGui::PushID(static_cast<int>(i));

        if (i % 2 != 0) {
            ImGui::SameLine();
        }

        ImGui::InvisibleButton("scene_tile", ImVec2(tileWidth, tileHeight));
        const ImVec2 tileMin = ImGui::GetItemRectMin();
        const ImVec2 tileMax = ImGui::GetItemRectMax();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(tileMin, tileMax, ImGui::GetColorU32(ImVec4(0.14f, 0.17f, 0.21f, 1.0f)), kSceneTileRounding);

        const bool isHovered = ImGui::IsItemHovered();

        if (tile.hasPreview) {
            const ImTextureID textureId = (ImTextureID)(WGPUTextureView)*tile.previewTextureView;
            const Vec2i textureSize(tile.previewSize);
            ImVec2 uvMin(0.0f, 0.0f);
            ImVec2 uvMax(1.0f, 1.0f);

            if (textureSize.x > 0 && textureSize.y > 0) {
                const float textureAspect = static_cast<float>(textureSize.x) / static_cast<float>(textureSize.y);
                const float tileAspect = tileWidth / tileHeight;

                if (textureAspect > tileAspect) {
                    const float visibleWidth = tileAspect / textureAspect;
                    const float crop = (1.0f - visibleWidth) * 0.5f;
                    uvMin.x = crop;
                    uvMax.x = 1.0f - crop;
                }
                else if (textureAspect < tileAspect) {
                    const float visibleHeight = textureAspect / tileAspect;
                    const float crop = (1.0f - visibleHeight) * 0.5f;
                    uvMin.y = crop;
                    uvMax.y = 1.0f - crop;
                }
            }

            drawList->AddImageRounded(textureId, tileMin, tileMax, uvMin, uvMax, IM_COL32_WHITE, kSceneTileRounding,
                                      ImDrawFlags_RoundCornersAll);
        }
        else {
            ImGui::SetCursorScreenPos(tileMin);
            drawIOPanelScenePreviewFallback(previewSize);
        }

        const ImVec4 borderColor = isHovered ? ImVec4(0.38f, 0.64f, 1.00f, 1.0f) : ImVec4(0.30f, 0.36f, 0.42f, 1.0f);
        drawList->AddRect(tileMin, tileMax, ImGui::GetColorU32(borderColor), kSceneTileRounding, 0, isHovered ? 1.5f : 1.0f);

        const float deleteButtonSize = 20.0f * scale;
        const float deleteButtonPadding = 8.0f * scale;
        const ImVec2 deleteMin(tileMax.x - deleteButtonSize - deleteButtonPadding, tileMin.y + deleteButtonPadding);
        const ImVec2 deleteMax(deleteMin.x + deleteButtonSize, deleteMin.y + deleteButtonSize);
        const bool deleteHovered = ImGui::IsMouseHoveringRect(deleteMin, deleteMax);
        const bool deleteClicked = deleteHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

        drawList->AddRectFilled(deleteMin, deleteMax,
                                ImGui::GetColorU32(deleteHovered ? ImVec4(0.76f, 0.24f, 0.28f, 0.96f) : ImVec4(0.10f, 0.12f, 0.16f, 0.86f)),
                                6.0f * scale);
        const ImVec2 deleteCenter(deleteMin.x + deleteButtonSize * 0.5f - 0.5f * scale,
                                  deleteMin.y + deleteButtonSize * 0.5f - 0.5f * scale);
        const float crossHalfExtent = deleteButtonSize * 0.18f;
        const ImU32 crossColor = ImGui::GetColorU32(ImVec4(0.96f, 0.97f, 0.99f, 1.0f));
        drawList->AddLine(ImVec2(deleteCenter.x - crossHalfExtent, deleteCenter.y - crossHalfExtent),
                          ImVec2(deleteCenter.x + crossHalfExtent, deleteCenter.y + crossHalfExtent), crossColor, 1.4f * scale);
        drawList->AddLine(ImVec2(deleteCenter.x - crossHalfExtent, deleteCenter.y + crossHalfExtent),
                          ImVec2(deleteCenter.x + crossHalfExtent, deleteCenter.y - crossHalfExtent), crossColor, 1.4f * scale);

        if (!deleteHovered && isHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            AppSignals::UI::LoadSimulation.emit(tile.path);
        }

        if (deleteClicked) {
            pendingDeleteScenePath_ = tile.path;
            pendingDeleteSceneTitle_ = tile.title;
            pendingDeleteError_.clear();
            pendingDeletePopupPos_ = ImVec2(deleteMax.x + 8.0f * scale, deleteMin.y);
            openDeleteConfirmation = true;
        }

        const ImVec2 titlePos(tileMin.x + 10.0f, tileMax.y - 25.0f);
        drawList->AddText(ImVec2(titlePos.x + 1.0f, titlePos.y + 1.0f), ImGui::GetColorU32(ImVec4(0.02f, 0.03f, 0.05f, 0.85f)),
                          tile.title.data());
        drawList->AddText(titlePos, ImGui::GetColorU32(ImVec4(0.95f, 0.96f, 0.98f, 1.0f)), tile.title.data());
        if (!tile.description.empty()) {
            const ImVec2 descriptionPos(tileMin.x + 10.0f, tileMax.y - 15.0f);
            drawList->AddText(ImVec2(descriptionPos.x + 1.0f, descriptionPos.y + 1.0f),
                              ImGui::GetColorU32(ImVec4(0.02f, 0.03f, 0.05f, 0.80f)), tile.description.data());
            drawList->AddText(descriptionPos, ImGui::GetColorU32(ImVec4(0.82f, 0.86f, 0.90f, 0.98f)), tile.description.data());
        }

        ImGui::PopID();
        ++i;
    }

    if (openDeleteConfirmation) {
        ImGui::OpenPopup(kDeletePopupId);
    }

    if (!pendingDeleteScenePath_.empty()) {
        ImGui::SetNextWindowPos(pendingDeletePopupPos_, ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f * scale, 10.0f * scale));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f * scale);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f * scale);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.08f, 0.10f, 0.13f, 0.96f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.33f, 0.38f, 0.46f, 0.95f));

        if (ImGui::BeginPopup(kDeletePopupId,
                              ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            const char* deleteTitle = "Удалить сцену?";
            const float titleWidth = ImGui::CalcTextSize(deleteTitle).x;
            const float titleOffsetX = std::max(0.0f, (ImGui::GetContentRegionAvail().x - titleWidth) * 0.5f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + titleOffsetX);
            ImGui::TextUnformatted(deleteTitle);
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.15f, 0.17f, 0.92f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.77f, 0.20f, 0.24f, 0.98f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.58f, 0.14f, 0.18f, 1.0f));
            if (ImGui::Button("Удалить", ImVec2(100.0f * scale, 0.0f))) {
                const std::string deletedScenePath = pendingDeleteScenePath_;
                std::error_code removeError;
                const bool removed = std::filesystem::remove(deletedScenePath, removeError);
                if (!removeError && (removed || !std::filesystem::exists(deletedScenePath))) {
                    removeSceneTileByPath(deletedScenePath);
                    clearPendingDeleteState();
                    ImGui::CloseCurrentPopup();
                }
                else {
                    pendingDeleteError_ = removeError ? removeError.message() : "Не удалось удалить файл сцены";
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            if (ImGui::Button("Отмена", ImVec2(100.0f * scale, 0.0f))) {
                clearPendingDeleteState();
                ImGui::CloseCurrentPopup();
            }

            if (!pendingDeleteError_.empty()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.48f, 0.48f, 1.0f));
                ImGui::TextWrapped("%s", pendingDeleteError_.data());
                ImGui::PopStyleColor();
            }

            ImGui::EndPopup();
        }

        if (!ImGui::IsPopupOpen(kDeletePopupId)) {
            clearPendingDeleteState();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
    }

    ImGui::End();
}
