#pragma once

#include <imgui.h>

#include "Lattice/Engine/physics/AtomData.h"

struct UiState;

void drawIOPanelRecordingStatusLine(bool isRecording, float fps, uint64_t frameCount);
void drawIOPanelAtomTypeCombo(const char* id, AtomData::Type& atomType, float width, float uiScale);
void drawIOPanelRecordingFormatCombo(const char* id, int& selectedFormat, float width, float uiScale);
void drawIOPanelCaptureStatus(const UiState& uiState);
void drawIOPanelScenePreviewFallback(const ImVec2& previewSize);
