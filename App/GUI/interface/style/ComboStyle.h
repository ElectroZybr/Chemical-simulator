#pragma once

#include <imgui.h>

namespace ComboStyle {

    bool beginCenteredCombo(const char* id, float width, float uiScale = 1.0f, ImGuiComboFlags flags = 0);
    bool beginCombo(const char* id, const char* preview, float width = 0.0f, float uiScale = 1.0f, ImGuiComboFlags flags = 0);
    void drawCenteredComboPreview(const char* selectedLabel);
    void pushCenteredSelectableText();
    void popCenteredSelectableText();

}
