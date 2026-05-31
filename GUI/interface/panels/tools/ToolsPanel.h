#pragma once
#include <cstdint>

#include <imgui.h>

enum class RendererType : uint8_t {
    Renderer2D,
    Renderer3D,
};

class DebugPanel;
class FileDialogManager;
class SettingsPanel;
class IOPanel;

class ToolsPanel {
public:
    static constexpr ImGuiWindowFlags PANEL_FLAGS = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;

    void draw(float scale, DebugPanel& debug, SettingsPanel& settings, IOPanel& ioPanel);
    void setRendererType(RendererType type);

private:
    bool is3D = false;
    bool isFree = false;
};
