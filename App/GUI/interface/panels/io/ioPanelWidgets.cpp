#include "ioPanelWidgets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "GUI/interface/UiState.h"
#include "GUI/interface/style/ComboStyle.h"

namespace {
    struct AtomTypeOption {
        const char* label;
        AtomData::Type type;
    };

    struct RecordingFormatOption {
        const char* label;
        int value;
    };

    constexpr std::array<AtomTypeOption, 19> kAtomTypeOptions{{
        {"Zerium", AtomData::Type::Z}, {"H", AtomData::Type::H},   {"He", AtomData::Type::He}, {"Li", AtomData::Type::Li},
        {"Be", AtomData::Type::Be},    {"B", AtomData::Type::B},   {"C", AtomData::Type::C},   {"N", AtomData::Type::N},
        {"O", AtomData::Type::O},      {"F", AtomData::Type::F},   {"Ne", AtomData::Type::Ne}, {"Na", AtomData::Type::Na},
        {"Mg", AtomData::Type::Mg},    {"Al", AtomData::Type::Al}, {"Si", AtomData::Type::Si}, {"P", AtomData::Type::P},
        {"S", AtomData::Type::S},      {"Cl", AtomData::Type::Cl}, {"Ar", AtomData::Type::Ar},
    }};

    constexpr std::array<RecordingFormatOption, 2> kRecordingFormatOptions{{
        {"MP4", 0},
        {"XYZ", 1},
    }};

    constexpr float kSceneTileRounding = 10.0f;

    int findTypeIndex(AtomData::Type type) {
        for (int i = 0; i < static_cast<int>(kAtomTypeOptions.size()); ++i) {
            if (kAtomTypeOptions[static_cast<size_t>(i)].type == type) {
                return i;
            }
        }
        return 0;
    }

    int findRecordingFormatIndex(int format) {
        for (int i = 0; i < static_cast<int>(kRecordingFormatOptions.size()); ++i) {
            if (kRecordingFormatOptions[static_cast<size_t>(i)].value == format) {
                return i;
            }
        }
        return 0;
    }
}

void drawIOPanelRecordingStatusLine(bool isRecording, float fps, uint64_t frameCount) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float lineHeight = ImGui::GetFrameHeight();
    const float radius = std::max(4.0f, lineHeight * 0.24f);
    const float dotWidth = radius * 2.0f + style.ItemInnerSpacing.x * 0.35f;
    const float alpha = isRecording ? 0.85f : 0.18f;

    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    const ImVec2 center(cursor.x + radius, cursor.y + lineHeight * 0.5f);

    ImGui::Dummy(ImVec2(dotWidth, lineHeight));
    ImGui::GetWindowDrawList()->AddCircleFilled(center, radius, ImGui::GetColorU32(ImVec4(0.95f, 0.16f, 0.16f, alpha)));
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("fps: %.1f", fps);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("frame: %llu", static_cast<unsigned long long>(frameCount));
}

void drawIOPanelAtomTypeCombo(const char* id, AtomData::Type& atomType, float width, float uiScale) {
    int selectedTypeIndex = findTypeIndex(atomType);
    const char* selectedLabel = kAtomTypeOptions[static_cast<size_t>(selectedTypeIndex)].label;

    if (ComboStyle::beginCenteredCombo(id, width, uiScale)) {
        ComboStyle::pushCenteredSelectableText();
        for (int i = 0; i < static_cast<int>(kAtomTypeOptions.size()); ++i) {
            const bool selected = (i == selectedTypeIndex);
            if (ImGui::Selectable(kAtomTypeOptions[static_cast<size_t>(i)].label, selected)) {
                atomType = kAtomTypeOptions[static_cast<size_t>(i)].type;
                selectedTypeIndex = i;
                selectedLabel = kAtomTypeOptions[static_cast<size_t>(i)].label;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ComboStyle::popCenteredSelectableText();
        ImGui::EndCombo();
    }

    ComboStyle::drawCenteredComboPreview(selectedLabel);
}

void drawIOPanelRecordingFormatCombo(const char* id, int& selectedFormat, float width, float uiScale) {
    int selectedIndex = findRecordingFormatIndex(selectedFormat);
    const char* selectedLabel = kRecordingFormatOptions[static_cast<size_t>(selectedIndex)].label;

    if (ComboStyle::beginCenteredCombo(id, width, uiScale)) {
        ComboStyle::pushCenteredSelectableText();
        for (int i = 0; i < static_cast<int>(kRecordingFormatOptions.size()); ++i) {
            const bool selected = (i == selectedIndex);
            if (ImGui::Selectable(kRecordingFormatOptions[static_cast<size_t>(i)].label, selected)) {
                selectedFormat = kRecordingFormatOptions[static_cast<size_t>(i)].value;
                selectedIndex = i;
                selectedLabel = kRecordingFormatOptions[static_cast<size_t>(i)].label;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ComboStyle::popCenteredSelectableText();
        ImGui::EndCombo();
    }

    ComboStyle::drawCenteredComboPreview(selectedLabel);
}

void drawIOPanelCaptureStatus(const UiState& uiState) {
    drawIOPanelRecordingStatusLine(uiState.captureRecording, uiState.captureFps, uiState.captureFrameCount);
}

void drawIOPanelScenePreviewFallback(const ImVec2& previewSize) {
    const ImVec2 min = ImGui::GetCursorScreenPos();
    const ImVec2 max(min.x + previewSize.x, min.y + previewSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImVec4(0.24f, 0.28f, 0.33f, 1.0f)), kSceneTileRounding);
    drawList->AddRect(min, max, ImGui::GetColorU32(ImVec4(0.36f, 0.42f, 0.50f, 1.0f)), kSceneTileRounding, 0, 1.0f);

    const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    drawList->AddLine(ImVec2(min.x + 12.0f, max.y - 12.0f), ImVec2(center.x - 8.0f, center.y + 6.0f),
                      ImGui::GetColorU32(ImVec4(0.55f, 0.62f, 0.70f, 0.7f)), 2.0f);
    drawList->AddLine(ImVec2(center.x - 8.0f, center.y + 6.0f), ImVec2(center.x + 6.0f, center.y - 8.0f),
                      ImGui::GetColorU32(ImVec4(0.55f, 0.62f, 0.70f, 0.7f)), 2.0f);
    drawList->AddLine(ImVec2(center.x + 6.0f, center.y - 8.0f), ImVec2(max.x - 12.0f, max.y - 20.0f),
                      ImGui::GetColorU32(ImVec4(0.55f, 0.62f, 0.70f, 0.7f)), 2.0f);
    drawList->AddCircleFilled(ImVec2(max.x - 22.0f, min.y + 20.0f), 7.0f, ImGui::GetColorU32(ImVec4(0.60f, 0.68f, 0.78f, 0.75f)));
    ImGui::Dummy(previewSize);
}
