#pragma once

#include <cstdint>
#include <string>

struct PreviewFrameRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct UiState {
    bool pause = false;
    bool cursorHovered = false;

    int selectedAtom = 0;
    int selectedAtomCount = 0;
    float simulationSpeed = 100.0f;
    double averageEnergy = 0.0;
    int simStep = 0;

    bool drawToolTrip = false;
    std::string toolTooltipText;

    bool captureRecording = false;
    bool captureAvailable = false;
    uint64_t captureFrameCount = 0;
    float captureFps = 0.0f;
    double captureBlinkElapsed = 0.0;

    bool xyzRecording = false;
    uint64_t xyzFrameCount = 0;
    float xyzFps = 0.0f;

    bool scenePreviewMode = false;
    PreviewFrameRect scenePreviewRect;
};
