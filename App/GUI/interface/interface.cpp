#include "interface.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_wgpu.h>

#include "App/capture/CaptureController.h"
#include "Rendering/BaseRenderer.h"
#include "Rendering/WGPUContext.h"

#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf897

#define ICON_FA_FLASK "\uf0c3"
#define ICON_FA_COG "\uf013"
#define ICON_FA_PAUSE "\uf04c"
#define ICON_FA_PLAY "\uf04b"
#define ICON_FA_FORWARD "\uf04e"
#define ICON_FA_BACKWARD "\uf04a"
#define ICON_FA_FAST_FORWARD "\uf050"
#define ICON_FA_FAST_BACKWARD "\uf049"
#define ICON_FA_BUG "\uf188"

namespace {
    PreviewFrameRect computeScenePreviewRect(const ImVec2& displaySize, float uiScale) {
        const float frameAspect = 1.282f;
        const float horizontalMargin = 36.0f * uiScale;
        const float verticalMargin = 88.0f * uiScale;
        const float maxWidth = std::max(120.0f, displaySize.x - horizontalMargin * 2.0f);
        const float maxHeight = std::max(120.0f, displaySize.y - verticalMargin * 2.0f);

        float frameWidth = maxWidth * 0.7f;
        float frameHeight = frameWidth / frameAspect;
        if (frameHeight > maxHeight) {
            frameHeight = maxHeight * 0.72f;
            frameWidth = frameHeight * frameAspect;
        }

        return {
            .x = (displaySize.x - frameWidth) * 0.5f,
            .y = (displaySize.y - frameHeight) * 0.5f,
            .width = frameWidth,
            .height = frameHeight,
        };
    }

    void drawScenePreviewFrame(float uiScale, UiState& uiState) {
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        uiState.scenePreviewRect = computeScenePreviewRect(displaySize, uiScale);

        const ImVec2 frameMin(uiState.scenePreviewRect.x, uiState.scenePreviewRect.y);
        const ImVec2 frameMax(frameMin.x + uiState.scenePreviewRect.width, frameMin.y + uiState.scenePreviewRect.height);
        const ImU32 shadeColor = ImGui::GetColorU32(ImVec4(0.02f, 0.03f, 0.05f, 0.42f));
        const ImU32 frameColor = ImGui::GetColorU32(ImVec4(0.80f, 0.88f, 1.00f, 0.95f));

        drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(displaySize.x, frameMin.y), shadeColor);
        drawList->AddRectFilled(ImVec2(0.0f, frameMin.y), ImVec2(frameMin.x, frameMax.y), shadeColor);
        drawList->AddRectFilled(ImVec2(frameMax.x, frameMin.y), ImVec2(displaySize.x, frameMax.y), shadeColor);
        drawList->AddRectFilled(ImVec2(0.0f, frameMax.y), ImVec2(displaySize.x, displaySize.y), shadeColor);

        drawList->AddRect(frameMin, frameMax, frameColor, 10.0f, 0, 1.5f);
    }
}

Interface::Interface(GLFWwindow* w, Lattice::Simulation& s, std::unique_ptr<BaseRenderer>& r, CaptureController& c)
    : window_(w), simulation_(&s), renderer_(&r), captureController_(&c) {}

int Interface::init() {
    ImGui::CreateContext();

    styleManager.applyCustomStyle();

    if (!fontManager.load(styleManager.getScale())) {
        return EXIT_FAILURE;
    }
    ImGui_ImplGlfw_InitForOther(window_, true);

    auto& ctx = WGPUContext::instance();
    ImGui_ImplWGPU_InitInfo wgpuInfo{};
    wgpuInfo.Device = (WGPUDevice)(*ctx.device());
    wgpuInfo.RenderTargetFormat = (WGPUTextureFormat)ctx.surfaceFormat();
    wgpuInfo.DepthStencilFormat = WGPUTextureFormat_Depth24Plus;
    ImGui_ImplWGPU_Init(&wgpuInfo);

    return EXIT_SUCCESS;
}

void Interface::shutdown() {
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

int Interface::update() {
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    const auto currentTime = std::chrono::high_resolution_clock::now();
    const float deltaTime = std::chrono::duration<float>(currentTime - lastTime_).count();
    io.DeltaTime = deltaTime;
    lastTime_ = currentTime;

    int width, height;
    glfwGetFramebufferSize(window_, &width, &height);

    ImGui::PushFont(fontManager.main);
    toolsPanel.draw(styleManager.getScale(), debugPanel, settingsPanel, ioPanel);
    periodicPanel.draw(styleManager.getScale(), Vec2i(width, height), uiState_.selectedAtom);
    simControlPanel.draw(styleManager.getScale(), Vec2i(width, height), uiState_.pause, uiState_.simulationSpeed, uiState_.simStep,
                         deltaTime);
    sideToolsPanel.draw(styleManager.getScale(), Vec2i(width, height), fontManager.icons, fontManager.dialog);
    statsPanel.draw(styleManager.getScale(), Vec2i(width, height));
    if (uiState_.drawToolTrip) {
        const ImVec2 mouse = ImGui::GetMousePos();
        ImGui::SetNextWindowPos(ImVec2(mouse.x + 3 * styleManager.getScale(), mouse.y + 3 * styleManager.getScale()));

        ImGui::BeginTooltip();
        if (!uiState_.toolTooltipText.empty()) {
            ImGui::TextUnformatted(uiState_.toolTooltipText.data());
        }
        else {
            ImGui::Text("Selected: %d", uiState_.selectedAtomCount);
        }
        ImGui::EndTooltip();
    }
    ImGui::PopFont();

    ImGui::PushFont(fontManager.dialog);
    fileDialog.draw(styleManager.getScale());
    debugPanel.draw(styleManager.getScale(), Vec2i(width, height));
    settingsPanel.draw(styleManager.getScale(), Vec2i(width, height), *simulation_, *renderer_, *captureController_, fileDialog);
    ioPanel.draw(styleManager.getScale(), Vec2i(width, height), *simulation_, fileDialog, uiState_);
    ImGui::PopFont();

    uiState_.scenePreviewMode = fileDialog.isSaveDialogOpen();
    if (uiState_.scenePreviewMode) {
        drawScenePreviewFrame(styleManager.getScale(), uiState_);
    }
    else {
        uiState_.scenePreviewRect = {};
    }

    uiState_.cursorHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) ||
                             ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
    return EXIT_SUCCESS;
}

void Interface::draw(BaseRenderer& renderer) {
    ImGui::Render();

    if (wgpu::raii::RenderPassEncoder* currentPass = renderer.currentRenderPass(); currentPass != nullptr) {
        ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), **currentPass);
    }
}

UiState& Interface::state() { return uiState_; }

const UiState& Interface::state() const { return uiState_; }

void Interface::setScenesDirectory(std::filesystem::path scenesDirectory) {
    ioPanel.setScenesDirectory(std::move(scenesDirectory));
    fileDialog.setSimulationDirectory(ioPanel.scenesDirectory().string());
}

const std::filesystem::path& Interface::scenesDirectory() const { return ioPanel.scenesDirectory(); }
