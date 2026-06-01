#include "PeriodicPanel.h"

#include <algorithm>

// clang-format off
static const char* KEYS[] = {
    "H",  "Ze",  "##2",  "##3",  "##4",  "##5",  "##6",  "He",
    "Li", "Be", "B",  "C",  "N",  "O",  "F",  "Ne",
    "Na", "Mg", "Al", "Si", "P",  "S",  "Cl", "Ar"
};
// clang-format on

static const ImVec4 ACTIVE_COLOR = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

int PeriodicPanel::decodeAtom(int index) {
    // clang-format off
    static int decode[] = {
        1,  0, -1, -1, -1, -1, -1,  2,
        3,  4,  5,  6,  7,  8,  9, 10,
        11, 12, 13, 14, 15, 16, 17, 18
    };
    // clang-format on
    if (index >= 0 && index < 24) {
        return decode[index];
    }
    return -1;
}

void PeriodicPanel::draw(float scale, Vec2i windowSize, int& selectedAtom) {
    const float panelW = 387.0f * scale;
    const float panelH = 142.0f * scale;
    const float panelX = windowSize.x * 0.5f - panelW * 0.5f;
    const float tabW = 180.0f * scale;
    const float tabH = 8.0f * scale;
    const float tabX = windowSize.x * 0.5f - tabW * 0.5f;

    // Анимация выезда
    ImVec2 mouse = ImGui::GetMousePos();
    const float hiddenY = -panelH;
    const float currentY = hiddenY + animProgress * panelH;

    const bool overTab = mouse.x >= tabX && mouse.x <= tabX + tabW && mouse.y >= 0.0f && mouse.y <= tabH;
    const bool overPanel = mouse.x >= panelX && mouse.x <= panelX + panelW && mouse.y >= currentY && mouse.y <= currentY + panelH;

    const float target = (overTab || overPanel) ? 1.0f : 0.0f;
    const float step = std::min(ImGui::GetIO().DeltaTime * 12.0f, 1.f);
    animProgress += (target - animProgress) * step;

    const float animY = hiddenY + animProgress * panelH;

    // Невидимый таб-триггер
    ImGui::SetNextWindowPos(ImVec2(tabX, 0));
    ImGui::SetNextWindowSize(ImVec2(tabW, tabH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("PeriodicTab", nullptr, PANEL_FLAGS);
    ImGui::End();
    ImGui::PopStyleVar(2);

    // Сама панель
    ImGui::SetNextWindowPos(ImVec2(panelX, animY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::Begin("Periodic", nullptr, PANEL_FLAGS);

    bool clicked = false;
    int clickedIdx = -1;

    for (int i = 0; i < 24; i++) {
        if (i == selectedAtom) {
            ImGui::PushStyleColor(ImGuiCol_Button, ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ACTIVE_COLOR);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ACTIVE_COLOR);
        }

        if (ImGui::Button(KEYS[i], ImVec2(40 * scale, 40 * scale))) {
            clicked = true;
            clickedIdx = i;
        }

        if (i == selectedAtom) {
            ImGui::PopStyleColor(3);
        }

        if ((i + 1) % 8 != 0) {
            ImGui::SameLine(0.0f, 7.5f * scale);
        }
    }

    if (clicked) {
        selectedAtom = (selectedAtom == clickedIdx) ? -1 : clickedIdx;
    }

    ImGui::End();
}
