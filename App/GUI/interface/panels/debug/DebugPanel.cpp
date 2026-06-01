#include "DebugPanel.h"

#include <imgui.h>

DebugView* DebugPanel::addView(DebugView view) {
    views.emplace_back(std::move(view));
    return &views.back();
}

void DebugPanel::draw(float uiScale, Vec2i windowSize) {
    float target = visible ? 1.f : 0.f;
    float step = ImGui::GetIO().DeltaTime * 12.f;
    animProgress += (target - animProgress) * std::min(step, 1.f);

    if (animProgress < 0.01f) {
        return;
    }

    const float panelWidth = 300.f * uiScale;
    const float topOffset = 65.f * uiScale;
    const float rawHeight = static_cast<float>(windowSize.y) - topOffset;
    const float panelHeight = (rawHeight > 0.f) ? rawHeight : 0.f;
    const float x = -panelWidth + panelWidth * animProgress;

    ImGui::SetNextWindowPos(ImVec2(x, topOffset));
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight));
    ImGui::Begin("##DebugPanel", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    if (ImGui::BeginTabBar("##DebugTabs")) {
        for (auto& view : views) {
            if (view.visible) {
                if (ImGui::BeginTabItem(view.getTitle())) {
                    view.draw(uiScale);
                    ImGui::EndTabItem();
                }
            }
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}
