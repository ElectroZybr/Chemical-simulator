#include "ComboStyle.h"

namespace ComboStyle {

    namespace {
        constexpr float kComboPaddingX = 8.0f;
    }

    bool beginCombo(const char* id, const char* preview, float width, float uiScale, ImGuiComboFlags flags) {
        if (width > 0.0f) {
            ImGui::SetNextItemWidth(width);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(kComboPaddingX * uiScale, ImGui::GetStyle().FramePadding.y));
        const bool opened = ImGui::BeginCombo(id, preview, flags);
        ImGui::PopStyleVar();
        return opened;
    }

    bool beginCenteredCombo(const char* id, float width, float uiScale, ImGuiComboFlags flags) {
        return beginCombo(id, "", width, uiScale, flags);
    }

    void drawCenteredComboPreview(const char* selectedLabel) {
        const ImVec2 comboMin = ImGui::GetItemRectMin();
        const ImVec2 comboMax = ImGui::GetItemRectMax();
        const float arrowWidth = ImGui::GetFrameHeight();
        const ImVec2 textSize = ImGui::CalcTextSize(selectedLabel);
        const float textX = comboMin.x + ((comboMax.x - comboMin.x - arrowWidth) - textSize.x) * 0.5f;
        const float textY = comboMin.y + ((comboMax.y - comboMin.y) - textSize.y) * 0.5f;
        ImGui::GetWindowDrawList()->AddText(ImVec2(textX, textY), ImGui::GetColorU32(ImGuiCol_Text), selectedLabel);
    }

    void pushCenteredSelectableText() { ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f)); }

    void popCenteredSelectableText() { ImGui::PopStyleVar(); }

}
