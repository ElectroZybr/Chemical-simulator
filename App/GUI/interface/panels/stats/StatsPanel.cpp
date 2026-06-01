#include "StatsPanel.h"

void StatsPanel::draw(float scale, Vec2i windowSize) {
    // FPS
    ImGui::SetNextWindowPos(ImVec2(windowSize.x - 150 * scale, windowSize.y - 50 * scale));
    ImGui::SetNextWindowSize(ImVec2(windowSize.x, windowSize.y));
    ImGui::Begin("Stats", nullptr, PANEL_FLAGS);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::End();
}
