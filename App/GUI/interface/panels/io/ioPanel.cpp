#include "ioPanel.h"

#include <algorithm>
#include <array>
#include <cfloat>
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
    constexpr float kGasDensity3DMin = 0.001f;
    constexpr float kGasDensity3DMax = 0.010f;
    constexpr float kGasDensity2DMin = 0.005f;
    constexpr float kGasDensity2DMax = 0.060f;

    const char* generatorLabel(IOPanel::GeneratorKind kind) {
        switch (kind) {
        case IOPanel::GeneratorKind::Massive:
            return "Massive crystal";
        case IOPanel::GeneratorKind::Gas:
            return "Random gas";
        case IOPanel::GeneratorKind::MixedGas:
            return "Mixed gas";
        case IOPanel::GeneratorKind::HexLattice:
            return "Hex lattice";
        case IOPanel::GeneratorKind::TriangularBipyramid:
            return "Triangular bipyramid";
        }
        return "Massive crystal";
    }

    void syncMixedGasCountsFromPercents(std::vector<IOPanel::MixedGasEntry>& entries, int totalCount) {
        for (IOPanel::MixedGasEntry& entry : entries) {
            entry.concentrationPercent = std::clamp(entry.concentrationPercent, 0.0f, 100.0f);
            entry.absoluteCount = totalCount > 0 ? static_cast<int>(std::round(static_cast<float>(totalCount) * entry.concentrationPercent / 100.0f)) : 0;
        }
    }

    void syncMixedGasPercentsFromCounts(std::vector<IOPanel::MixedGasEntry>& entries, int totalCount) {
        for (IOPanel::MixedGasEntry& entry : entries) {
            entry.absoluteCount = std::max(entry.absoluteCount, 0);
            entry.concentrationPercent = totalCount > 0 ? (100.0f * static_cast<float>(entry.absoluteCount) / static_cast<float>(totalCount)) : 0.0f;
        }
    }

    void rebalanceMixedGasCounts(std::vector<IOPanel::MixedGasEntry>& entries, int totalCount, size_t lockedIndex) {
        if (entries.empty()) {
            return;
        }

        totalCount = std::max(totalCount, 0);
        if (lockedIndex >= entries.size()) {
            lockedIndex = entries.size() - 1;
        }

        entries[lockedIndex].absoluteCount = std::clamp(entries[lockedIndex].absoluteCount, 0, totalCount);
        const int lockedCount = entries[lockedIndex].absoluteCount;
        const int remainingTotal = std::max(0, totalCount - lockedCount);

        std::vector<size_t> otherIndices;
        otherIndices.reserve(entries.size() > 0 ? entries.size() - 1 : 0);
        int otherWeightSum = 0;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i == lockedIndex) {
                continue;
            }
            entries[i].absoluteCount = std::max(entries[i].absoluteCount, 0);
            otherIndices.push_back(i);
            otherWeightSum += entries[i].absoluteCount;
        }

        if (!otherIndices.empty()) {
            std::vector<int> newCounts(otherIndices.size(), 0);
            int assigned = 0;

            if (otherWeightSum > 0) {
                std::vector<float> remainders(otherIndices.size(), 0.0f);
                for (size_t n = 0; n < otherIndices.size(); ++n) {
                    const float exact = static_cast<float>(remainingTotal) * static_cast<float>(entries[otherIndices[n]].absoluteCount) /
                                        static_cast<float>(otherWeightSum);
                    newCounts[n] = static_cast<int>(std::floor(exact));
                    remainders[n] = exact - static_cast<float>(newCounts[n]);
                    assigned += newCounts[n];
                }
                while (assigned < remainingTotal) {
                    size_t bestIndex = 0;
                    for (size_t n = 1; n < otherIndices.size(); ++n) {
                        if (remainders[n] > remainders[bestIndex]) {
                            bestIndex = n;
                        }
                    }
                    ++newCounts[bestIndex];
                    remainders[bestIndex] = -1.0f;
                    ++assigned;
                }
            }
            else {
                const int base = remainingTotal / static_cast<int>(otherIndices.size());
                int rest = remainingTotal % static_cast<int>(otherIndices.size());
                for (size_t n = 0; n < otherIndices.size(); ++n) {
                    newCounts[n] = base + (rest > 0 ? 1 : 0);
                    if (rest > 0) {
                        --rest;
                    }
                }
            }

            for (size_t n = 0; n < otherIndices.size(); ++n) {
                entries[otherIndices[n]].absoluteCount = newCounts[n];
            }
        }

        syncMixedGasPercentsFromCounts(entries, totalCount);
    }

    void normalizeMixedGasToTotal(std::vector<IOPanel::MixedGasEntry>& entries, int totalCount) {
        if (entries.empty()) {
            return;
        }

        totalCount = std::max(totalCount, 0);
        int sum = 0;
        for (const IOPanel::MixedGasEntry& entry : entries) {
            sum += std::max(entry.absoluteCount, 0);
        }

        if (sum <= 0) {
            entries.front().absoluteCount = totalCount;
            for (size_t i = 1; i < entries.size(); ++i) {
                entries[i].absoluteCount = 0;
            }
            syncMixedGasPercentsFromCounts(entries, totalCount);
            return;
        }

        std::vector<int> newCounts(entries.size(), 0);
        std::vector<float> remainders(entries.size(), 0.0f);
        int assigned = 0;
        for (size_t i = 0; i < entries.size(); ++i) {
            const float exact = static_cast<float>(totalCount) * static_cast<float>(std::max(entries[i].absoluteCount, 0)) / static_cast<float>(sum);
            newCounts[i] = static_cast<int>(std::floor(exact));
            remainders[i] = exact - static_cast<float>(newCounts[i]);
            assigned += newCounts[i];
        }
        while (assigned < totalCount) {
            size_t bestIndex = 0;
            for (size_t i = 1; i < entries.size(); ++i) {
                if (remainders[i] > remainders[bestIndex]) {
                    bestIndex = i;
                }
            }
            ++newCounts[bestIndex];
            remainders[bestIndex] = -1.0f;
            ++assigned;
        }

        for (size_t i = 0; i < entries.size(); ++i) {
            entries[i].absoluteCount = newCounts[i];
        }
        syncMixedGasPercentsFromCounts(entries, totalCount);
    }

    int mixedGasAssignedCount(const std::vector<IOPanel::MixedGasEntry>& entries) {
        int sum = 0;
        for (const IOPanel::MixedGasEntry& entry : entries) {
            sum += std::max(entry.absoluteCount, 0);
        }
        return sum;
    }

    float clampUiGasDensity(float density, bool is3d) {
        return std::clamp(density, is3d ? kGasDensity3DMin : kGasDensity2DMin, is3d ? kGasDensity3DMax : kGasDensity2DMax);
    }

    void drawGasDensitySlider(const char* id, float& density, bool is3d, float scale) {
        density = clampUiGasDensity(density, is3d);
        ImGui::SetNextItemWidth(120.0f * scale);
        ImGui::SliderFloat(id, &density, is3d ? kGasDensity3DMin : kGasDensity2DMin, is3d ? kGasDensity3DMax : kGasDensity2DMax,
                           is3d ? "%.4f A^-3" : "%.3f A^-2", ImGuiSliderFlags_Logarithmic);
    }

    void drawAxisCountControls(const char* prefix, glm::ivec3& counts, float scale, int minValue, int maxValue, bool enableZ) {
        ImGui::SetNextItemWidth(160.0f * scale);
        ImGui::SliderInt((std::string("X##") + prefix).c_str(), &counts.x, minValue, maxValue);
        ImGui::SetNextItemWidth(160.0f * scale);
        ImGui::SliderInt((std::string("Y##") + prefix).c_str(), &counts.y, minValue, maxValue);
        ImGui::BeginDisabled(!enableZ);
        if (!enableZ) {
            counts.z = 1;
        }
        ImGui::SetNextItemWidth(160.0f * scale);
        ImGui::SliderInt((std::string("Z##") + prefix).c_str(), &counts.z, minValue, maxValue);
        ImGui::EndDisabled();
    }

    glm::ivec3 makeUniformAxisCounts(int count, bool enableZ) {
        return glm::ivec3(count, count, enableZ ? count : 1);
    }
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

void IOPanel::draw(float scale, glm::ivec2 windowSize, Lattice::Simulation& simulation, FileDialogManager& fileDialog, UiState& uiState) {
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
    ImGui::SeparatorText("Генератор");
    if (ImGui::BeginCombo("##generator_kind", generatorLabel(generatorKind_))) {
        constexpr IOPanel::GeneratorKind generators[] = {
            IOPanel::GeneratorKind::Massive,
            IOPanel::GeneratorKind::Gas,
            IOPanel::GeneratorKind::MixedGas,
            IOPanel::GeneratorKind::HexLattice,
            IOPanel::GeneratorKind::TriangularBipyramid,
        };
        for (IOPanel::GeneratorKind kind : generators) {
            const bool selected = kind == generatorKind_;
            if (ImGui::Selectable(generatorLabel(kind), selected)) {
                generatorKind_ = kind;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    bool createRequested = false;
    if (generatorAtomCount_ != mixedGasLastTotalCount_) {
        normalizeMixedGasToTotal(mixedGasEntries_, generatorAtomCount_);
        mixedGasLastTotalCount_ = generatorAtomCount_;
    }
    switch (generatorKind_) {
    case GeneratorKind::Massive:
        if (!massiveSeparateAxes_) {
            drawAxisCountControls("massive_axis", generatorAxisCounts_, scale, 2, 200, generatorIs3D_);
        }
        else {
            ImGui::SetNextItemWidth(160.0f * scale);
            ImGui::SliderInt("Size##atoms_per_axis", &generatorAxisCount_, 2, 200);
            generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, generatorIs3D_);
        }
        if (massiveSeparateAxes_) {
            ImGui::SameLine();
        }
        drawIOPanelAtomTypeCombo("##atom_type", atomType_, 80.f * scale, scale);
        createRequested = ImGui::Button("Create##massive", ImVec2(buttonWidth * scale, 0.f));
        if (createRequested) {
            AppSignals::UI::CreateMassive.emit(generatorAxisCounts_, atomType_, generatorIs3D_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("3D##massive", &generatorIs3D_);
        ImGui::SameLine();
        if (ImGui::Checkbox("Scalar##massive", &massiveSeparateAxes_)) {
            if (!massiveSeparateAxes_) {
                generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, generatorIs3D_);
            }
            else {
                generatorAxisCount_ = std::max(generatorAxisCounts_.x, 2);
                generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, generatorIs3D_);
            }
        }
        break;
    case GeneratorKind::Gas:
        ImGui::SliderInt("##gas_atom_count", &generatorAtomCount_, 100, 300000);
        ImGui::SameLine();
        drawIOPanelAtomTypeCombo("##atom_type_gas", gasAtomType_, 80.f * scale, scale);
        createRequested = ImGui::Button("Create##gas", ImVec2(buttonWidth * scale, 0.f));
        if (createRequested) {
            AppSignals::UI::CreateGas.emit(generatorAtomCount_, gasAtomType_, generatorIs3D_, generatorDensity_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("3D##gas", &generatorIs3D_);
        ImGui::SameLine();
        drawGasDensitySlider("##gas_density", generatorDensity_, generatorIs3D_, scale);
        break;
    case GeneratorKind::MixedGas:
        ImGui::SetNextItemWidth(140.0f * scale);
        ImGui::SliderInt("##mixed_gas_atom_count", &generatorAtomCount_, 100, 300000);
        ImGui::SameLine();
        drawGasDensitySlider("##mixed_gas_density", generatorDensity_, generatorIs3D_, scale);
        for (size_t i = 0; i < mixedGasEntries_.size(); ++i) {
            MixedGasEntry& entry = mixedGasEntries_[i];
            ImGui::PushID(static_cast<int>(i));
            drawIOPanelAtomTypeCombo("##mixed_type", entry.type, 80.f * scale, scale);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f * scale);
            const bool percentChanged = ImGui::DragFloat("##mixed_percent", &entry.concentrationPercent, 0.25f, 0.0f, 100.0f, "%.1f%%");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(72.0f * scale);
            const bool countChanged = ImGui::DragInt("##mixed_count", &entry.absoluteCount, 1.0f, 0, INT_MAX, "%d");
            ImGui::SameLine();
            ImGui::BeginDisabled(mixedGasEntries_.size() <= 1);
            const bool removeEntry = ImGui::Button("x##mixed_remove", ImVec2(22.0f * scale, 0.0f));
            ImGui::EndDisabled();
            if (percentChanged) {
                entry.concentrationPercent = std::clamp(entry.concentrationPercent, 0.0f, 100.0f);
                entry.absoluteCount = generatorAtomCount_ > 0
                                          ? static_cast<int>(std::round(static_cast<float>(generatorAtomCount_) * entry.concentrationPercent / 100.0f))
                                          : 0;
                rebalanceMixedGasCounts(mixedGasEntries_, generatorAtomCount_, i);
            }
            if (countChanged) {
                generatorAtomCount_ = std::max(generatorAtomCount_, mixedGasAssignedCount(mixedGasEntries_));
                mixedGasLastTotalCount_ = generatorAtomCount_;
                rebalanceMixedGasCounts(mixedGasEntries_, generatorAtomCount_, i);
            }
            ImGui::PopID();
            if (removeEntry) {
                mixedGasEntries_.erase(mixedGasEntries_.begin() + static_cast<std::ptrdiff_t>(i));
                normalizeMixedGasToTotal(mixedGasEntries_, generatorAtomCount_);
                --i;
            }
        }
        if (ImGui::Button("Add type##mixed_gas", ImVec2(buttonWidth * scale, 0.f))) {
            mixedGasEntries_.push_back({.type = AtomData::Type::Z, .concentrationPercent = 0.0f, .absoluteCount = 0});
            normalizeMixedGasToTotal(mixedGasEntries_, generatorAtomCount_);
        }
        createRequested = ImGui::Button("Create##mixed_gas", ImVec2(buttonWidth * scale, 0.f));
        if (createRequested) {
            std::vector<Generators::AtomTypeSpec> atomSpecs;
            atomSpecs.reserve(mixedGasEntries_.size());
            for (const MixedGasEntry& entry : mixedGasEntries_) {
                atomSpecs.push_back(Generators::AtomTypeSpec{
                    .type = entry.type,
                    .absoluteCount = std::max(entry.absoluteCount, 0),
                    .concentrationPercent = 0.0f,
                });
            }
            AppSignals::UI::CreateMixedGas.emit(generatorAtomCount_, std::move(atomSpecs), generatorIs3D_, generatorDensity_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("3D##mixed_gas", &generatorIs3D_);
        break;
    case GeneratorKind::HexLattice:
        if (!hexSeparateAxes_) {
            drawAxisCountControls("hex_axis", generatorAxisCounts_, scale, 2, 200, true);
        }
        else {
            ImGui::SetNextItemWidth(120.0f * scale);
            ImGui::SliderInt("Size##atoms_per_axis_ic", &generatorAxisCount_, 2, 200);
            generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, true);
        }
        if (hexSeparateAxes_) {
            ImGui::SameLine();
        }
        drawIOPanelAtomTypeCombo("##atom_type_ic", icAtomType_, 80.f * scale, scale);
        createRequested = ImGui::Button("Create##hex", ImVec2(buttonWidth * scale, 0.f));
        if (createRequested) {
            AppSignals::UI::CreateHexLattice.emit(generatorAxisCounts_, icAtomType_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("3D##hex", &generatorIs3D_);
        ImGui::SameLine();
        if (ImGui::Checkbox("Scalar##hex", &hexSeparateAxes_)) {
            if (!hexSeparateAxes_) {
                generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, true);
            }
            else {
                generatorAxisCount_ = std::max(generatorAxisCounts_.x, 2);
                generatorAxisCounts_ = makeUniformAxisCounts(generatorAxisCount_, true);
            }
        }
        break;
    case GeneratorKind::TriangularBipyramid:
        ImGui::SliderInt("##atoms_per_axis_tbp", &generatorAxisCount_, 2, 100);
        ImGui::SameLine();
        drawIOPanelAtomTypeCombo("##atom_type_tbp", tbpAtomType_, 80.f * scale, scale);
        createRequested = ImGui::Button("Create##tbp", ImVec2(buttonWidth * scale, 0.f));
        if (createRequested) {
            AppSignals::UI::CreateTriangularBipyramidCrystal.emit(generatorAxisCount_, tbpAtomType_);
        }
        break;
    }

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
            const glm::ivec2 textureSize(static_cast<int>(tile.previewSize.x), static_cast<int>(tile.previewSize.y));
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
