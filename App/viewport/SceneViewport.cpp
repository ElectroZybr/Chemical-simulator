#include "App/viewport/SceneViewport.h"

#include "App/debug/DebugRuntime.h"
#include "App/interaction/ToolsManager.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/metrics/Profiler.h"
#include "GUI/interface/interface.h"
#include "Rendering/Renderer2D.h"
#include "Rendering/Renderer3D.h"
#include "Rendering/BaseRenderer.h"

namespace {
    struct ViewCubeVertex {
        glm::vec3 local;
        glm::vec3 view;
        ImVec2 screen;
    };

    struct ViewCubeFace {
        std::array<int, 4> corners;
    };

    struct SortedViewCubeFace {
        float depth;
        ViewCubeFace face;
        bool frontFacing;
        glm::vec3 localDirection;
    };

    struct ViewCubeEdge {
        int startVertex;
        int endVertex;
        int firstFace;
        int secondFace;
    };

    float signedArea2D(ImVec2 a, ImVec2 b, ImVec2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }

    bool pointInConvexQuad(ImVec2 point, const std::array<ImVec2, 4>& quad) {
        bool hasPositive = false;
        bool hasNegative = false;
        for (size_t i = 0; i < quad.size(); ++i) {
            const ImVec2 a = quad[i];
            const ImVec2 b = quad[(i + 1) % quad.size()];
            const float side = signedArea2D(a, b, point);
            hasPositive |= side > 0.0f;
            hasNegative |= side < 0.0f;
            if (hasPositive && hasNegative) {
                return false;
            }
        }
        return true;
    }

    ImVec2 bilerp(const std::array<ImVec2, 4>& quad, float u, float v) {
        const ImVec2 top{
            quad[0].x + (quad[1].x - quad[0].x) * u,
            quad[0].y + (quad[1].y - quad[0].y) * u,
        };
        const ImVec2 bottom{
            quad[3].x + (quad[2].x - quad[3].x) * u,
            quad[3].y + (quad[2].y - quad[3].y) * u,
        };
        return ImVec2(
            top.x + (bottom.x - top.x) * v,
            top.y + (bottom.y - top.y) * v
        );
    }

    glm::vec3 bilerp(const std::array<glm::vec3, 4>& quad, float u, float v) {
        const glm::vec3 top = quad[0] + (quad[1] - quad[0]) * u;
        const glm::vec3 bottom = quad[3] + (quad[2] - quad[3]) * u;
        return top + (bottom - top) * v;
    }

    void orderFaceQuadForScreen(std::array<ImVec2, 4>& screenQuad, std::array<glm::vec3, 4>& localQuad) {
        std::array<int, 4> indices = {0, 1, 2, 3};
        std::sort(indices.begin(), indices.end(), [&](int a, int b) {
            if (std::abs(screenQuad[static_cast<size_t>(a)].y - screenQuad[static_cast<size_t>(b)].y) > 1e-4f) {
                return screenQuad[static_cast<size_t>(a)].y < screenQuad[static_cast<size_t>(b)].y;
            }
            return screenQuad[static_cast<size_t>(a)].x < screenQuad[static_cast<size_t>(b)].x;
        });

        int topLeft = indices[0];
        int topRight = indices[1];
        if (screenQuad[static_cast<size_t>(topLeft)].x > screenQuad[static_cast<size_t>(topRight)].x) {
            std::swap(topLeft, topRight);
        }

        int bottomLeft = indices[2];
        int bottomRight = indices[3];
        if (screenQuad[static_cast<size_t>(bottomLeft)].x > screenQuad[static_cast<size_t>(bottomRight)].x) {
            std::swap(bottomLeft, bottomRight);
        }

        const std::array<int, 4> ordered = {topLeft, topRight, bottomRight, bottomLeft};
        const std::array<ImVec2, 4> originalScreen = screenQuad;
        const std::array<glm::vec3, 4> originalLocal = localQuad;
        for (size_t i = 0; i < ordered.size(); ++i) {
            screenQuad[i] = originalScreen[static_cast<size_t>(ordered[i])];
            localQuad[i] = originalLocal[static_cast<size_t>(ordered[i])];
        }
    }

    glm::vec3 axisDirectionFromComponent(int axisIndex, float sign) {
        glm::vec3 direction(0.0f);
        direction[axisIndex] = sign >= 0.0f ? 1.0f : -1.0f;
        return direction;
    }

    glm::vec3 canonicalDirectionSigns(glm::vec3 direction) {
        glm::vec3 canonical(0.0f);
        canonical.x = direction.x >= 0.0f ? 1.0f : -1.0f;
        canonical.y = direction.y >= 0.0f ? 1.0f : -1.0f;
        canonical.z = direction.z >= 0.0f ? 1.0f : -1.0f;
        return glm::normalize(canonical);
    }

    glm::vec3 screenStripeDirection(glm::vec3 faceDirection, const glm::mat3& viewRotation, int row, int col) {
        int normalAxis = 0;
        float maxNormalAbs = std::abs(faceDirection.x);
        if (std::abs(faceDirection.y) > maxNormalAbs) {
            normalAxis = 1;
            maxNormalAbs = std::abs(faceDirection.y);
        }
        if (std::abs(faceDirection.z) > maxNormalAbs) {
            normalAxis = 2;
        }

        float bestScore = -std::numeric_limits<float>::infinity();
        glm::vec3 bestDirection(0.0f);
        for (int axis = 0; axis < 3; ++axis) {
            if (axis == normalAxis) {
                continue;
            }
            for (float sign : {-1.0f, 1.0f}) {
                const glm::vec3 direction = axisDirectionFromComponent(axis, sign);
                const glm::vec3 viewDirection = viewRotation * direction;
                float score = -std::numeric_limits<float>::infinity();
                if (row == 1 && col == 0) {
                    score = -viewDirection.x;
                }
                else if (row == 1 && col == 2) {
                    score = viewDirection.x;
                }
                else if (row == 0 && col == 1) {
                    score = viewDirection.y;
                }
                else if (row == 2 && col == 1) {
                    score = -viewDirection.y;
                }

                if (score > bestScore) {
                    bestScore = score;
                    bestDirection = direction;
                }
            }
        }
        return bestDirection;
    }

    ImU32 edgeAccentColor(glm::vec3 localAxis, bool highlighted) {
        const glm::vec3 absAxis = glm::abs(localAxis);
        if (absAxis.x > absAxis.y && absAxis.x > absAxis.z) {
            return highlighted ? IM_COL32(255, 96, 96, 255) : IM_COL32(240, 72, 72, 235);
        }
        if (absAxis.y > absAxis.z) {
            return highlighted ? IM_COL32(96, 255, 96, 255) : IM_COL32(72, 240, 72, 235);
        }
        return highlighted ? IM_COL32(96, 160, 255, 255) : IM_COL32(72, 136, 240, 235);
    }

    bool isAccentEdge(const ViewCubeEdge& edge) {
        const int a = std::min(edge.startVertex, edge.endVertex);
        const int b = std::max(edge.startVertex, edge.endVertex);
        return (a == 4 && b == 5) || (a == 4 && b == 6) || (a == 0 && b == 4);
    }

    int findSortedFaceByDirection(const std::array<SortedViewCubeFace, 6>& sortedFaces, glm::vec3 direction) {
        float bestDot = -1.0f;
        int bestIndex = -1;
        for (size_t i = 0; i < sortedFaces.size(); ++i) {
            const float dotValue = glm::dot(sortedFaces[i].localDirection, direction);
            if (dotValue > bestDot) {
                bestDot = dotValue;
                bestIndex = static_cast<int>(i);
            }
        }
        return bestDot > 0.9f ? bestIndex : -1;
    }

    std::pair<int, int> cellForFaceDirection(const SortedViewCubeFace& face, const std::array<ViewCubeVertex, 8>& vertices,
                                             glm::vec3 targetDirection) {
        std::array<ImVec2, 4> screenQuad{};
        std::array<glm::vec3, 4> localQuad{};
        for (int i = 0; i < 4; ++i) {
            const size_t cornerIndex = static_cast<size_t>(face.face.corners[static_cast<size_t>(i)]);
            screenQuad[static_cast<size_t>(i)] = vertices[cornerIndex].screen;
            localQuad[static_cast<size_t>(i)] = vertices[cornerIndex].local;
        }
        orderFaceQuadForScreen(screenQuad, localQuad);

        const glm::vec3 uAxis = localQuad[1] - localQuad[0];
        const glm::vec3 vAxis = localQuad[3] - localQuad[0];
        const float uValue = glm::dot(targetDirection, uAxis);
        const float vValue = glm::dot(targetDirection, vAxis);

        int col = 1;
        if (uValue < -1e-4f) {
            col = 0;
        }
        else if (uValue > 1e-4f) {
            col = 2;
        }

        int row = 1;
        if (vValue < -1e-4f) {
            row = 0;
        }
        else if (vValue > 1e-4f) {
            row = 2;
        }

        return {row, col};
    }

    void drawViewCubeOverlay(Camera& camera) {
        const glm::vec2 screenSize = camera.getScreenSize();
        if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
            return;
        }

        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 center(screenSize.x - 200.0f, 75.0f);
        constexpr float cubeScale = 22.0f;
        const ImU32 cubeFillColor = IM_COL32(120, 140, 155, 255);
        const ImU32 cubeFaceHoverColor = IM_COL32(144, 167, 184, 255);
        const ImU32 cubeEdgeColor = IM_COL32(128, 199, 255, 140);
        const ImU32 cubeCutColor = IM_COL32(214, 232, 255, 150);
        constexpr std::array<float, 4> kFaceCuts = {0.0f, 0.22f, 0.78f, 1.0f};

        const glm::mat3 viewRotation(camera.getViewMatrix());
        std::array<ViewCubeVertex, 8> vertices{};
        for (size_t i = 0; i < vertices.size(); ++i) {
            const glm::vec3 local((i & 1) ? 1.0f : -1.0f, (i & 2) ? 1.0f : -1.0f, (i & 4) ? 1.0f : -1.0f);
            const glm::vec3 view = viewRotation * local;
            vertices[i] = {
                .local = local,
                .view = view,
                .screen = ImVec2(center.x + view.x * cubeScale, center.y - view.y * cubeScale),
            };
        }

        const std::array<ViewCubeFace, 6> faces = {{
            {{{0, 2, 6, 4}}},
            {{{1, 5, 7, 3}}},
            {{{0, 1, 3, 2}}},
            {{{4, 6, 7, 5}}},
            {{{0, 4, 5, 1}}},
            {{{2, 3, 7, 6}}},
        }};

        std::array<SortedViewCubeFace, 6> sortedFaces{};
        for (size_t i = 0; i < faces.size(); ++i) {
            float depth = 0.0f;
            glm::vec3 localDirection(0.0f);
            for (int corner : faces[i].corners) {
                depth += vertices[static_cast<size_t>(corner)].view.z;
                localDirection += vertices[static_cast<size_t>(corner)].local;
            }
            const glm::vec3 edgeA = vertices[static_cast<size_t>(faces[i].corners[1])].view - vertices[static_cast<size_t>(faces[i].corners[0])].view;
            const glm::vec3 edgeB = vertices[static_cast<size_t>(faces[i].corners[2])].view - vertices[static_cast<size_t>(faces[i].corners[0])].view;
            const glm::vec3 normal = glm::cross(edgeA, edgeB);
            sortedFaces[i] = {.depth = depth, .face = faces[i], .frontFacing = normal.z < 0.0f, .localDirection = glm::normalize(localDirection)};
        }
        std::sort(sortedFaces.begin(), sortedFaces.end(), [](const SortedViewCubeFace& a, const SortedViewCubeFace& b) { return a.depth < b.depth; });

        constexpr std::array<ViewCubeEdge, 12> edges = {{
            {.startVertex = 0, .endVertex = 1, .firstFace = 2, .secondFace = 4},
            {.startVertex = 1, .endVertex = 3, .firstFace = 1, .secondFace = 2},
            {.startVertex = 3, .endVertex = 2, .firstFace = 2, .secondFace = 5},
            {.startVertex = 2, .endVertex = 0, .firstFace = 0, .secondFace = 2},
            {.startVertex = 4, .endVertex = 5, .firstFace = 3, .secondFace = 4},
            {.startVertex = 5, .endVertex = 7, .firstFace = 1, .secondFace = 3},
            {.startVertex = 7, .endVertex = 6, .firstFace = 3, .secondFace = 5},
            {.startVertex = 6, .endVertex = 4, .firstFace = 0, .secondFace = 3},
            {.startVertex = 0, .endVertex = 4, .firstFace = 0, .secondFace = 4},
            {.startVertex = 1, .endVertex = 5, .firstFace = 1, .secondFace = 4},
            {.startVertex = 2, .endVertex = 6, .firstFace = 0, .secondFace = 5},
            {.startVertex = 3, .endVertex = 7, .firstFace = 1, .secondFace = 5},
        }};
        std::array<int, 12> sortedEdgeIndices{};
        for (size_t i = 0; i < edges.size(); ++i) {
            sortedEdgeIndices[i] = static_cast<int>(i);
        }
        std::sort(sortedEdgeIndices.begin(), sortedEdgeIndices.end(), [&](int lhs, int rhs) {
            const ViewCubeEdge& a = edges[static_cast<size_t>(lhs)];
            const ViewCubeEdge& b = edges[static_cast<size_t>(rhs)];
            const float depthA =
                0.5f * (vertices[static_cast<size_t>(a.startVertex)].view.z + vertices[static_cast<size_t>(a.endVertex)].view.z);
            const float depthB =
                0.5f * (vertices[static_cast<size_t>(b.startVertex)].view.z + vertices[static_cast<size_t>(b.endVertex)].view.z);
            return depthA < depthB;
        });

        const ImVec2 mousePos = ImGui::GetIO().MousePos;
        int hoveredFace = -1;
        int hoveredFaceCell = -1;
        glm::vec3 hoveredFaceDirection(0.0f);
        for (int sortedIndex = static_cast<int>(sortedFaces.size()) - 1; sortedIndex >= 0; --sortedIndex) {
            const SortedViewCubeFace& sortedFace = sortedFaces[static_cast<size_t>(sortedIndex)];
            if (!sortedFace.frontFacing) {
                continue;
            }
            std::array<ImVec2, 4> quad{};
            std::array<glm::vec3, 4> localQuad{};
            for (int i = 0; i < 4; ++i) {
                const size_t cornerIndex = static_cast<size_t>(sortedFace.face.corners[static_cast<size_t>(i)]);
                quad[i] = vertices[cornerIndex].screen;
                localQuad[i] = vertices[cornerIndex].local;
            }
            orderFaceQuadForScreen(quad, localQuad);

            for (int row = 0; row < 3; ++row) {
                for (int col = 0; col < 3; ++col) {
                    const std::array<ImVec2, 4> cellQuad = {{
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col)], kFaceCuts[static_cast<size_t>(row)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col + 1)], kFaceCuts[static_cast<size_t>(row)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col + 1)], kFaceCuts[static_cast<size_t>(row + 1)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col)], kFaceCuts[static_cast<size_t>(row + 1)]),
                    }};
                    if (!pointInConvexQuad(mousePos, cellQuad)) {
                        continue;
                    }

                    hoveredFace = sortedIndex;
                    hoveredFaceCell = row * 3 + col;
                    const float u = 0.5f * (kFaceCuts[static_cast<size_t>(col)] + kFaceCuts[static_cast<size_t>(col + 1)]);
                    const float v = 0.5f * (kFaceCuts[static_cast<size_t>(row)] + kFaceCuts[static_cast<size_t>(row + 1)]);
                    hoveredFaceDirection = glm::normalize(bilerp(localQuad, u, v));
                    if (row != 1 && col != 1) {
                        hoveredFaceDirection = canonicalDirectionSigns(hoveredFaceDirection);
                    }
                    break;
                }
                if (hoveredFace >= 0) {
                    break;
                }
            }
            if (hoveredFace >= 0) {
                break;
            }
        }

        std::array<int, 6> highlightedCells;
        highlightedCells.fill(-1);
        if (hoveredFace >= 0) {
            highlightedCells[static_cast<size_t>(hoveredFace)] = hoveredFaceCell;

            const int hoveredRow = hoveredFaceCell / 3;
            const int hoveredCol = hoveredFaceCell % 3;
            const bool isCenter = hoveredRow == 1 && hoveredCol == 1;

            const glm::vec3 faceDirection = sortedFaces[static_cast<size_t>(hoveredFace)].localDirection;
            int normalAxis = 0;
            float maxNormalAbs = std::abs(faceDirection.x);
            if (std::abs(faceDirection.y) > maxNormalAbs) {
                normalAxis = 1;
                maxNormalAbs = std::abs(faceDirection.y);
            }
            if (std::abs(faceDirection.z) > maxNormalAbs) {
                normalAxis = 2;
            }

            if (!isCenter) {
                for (int axis = 0; axis < 3; ++axis) {
                    if (axis == normalAxis) {
                        continue;
                    }
                    const float component = hoveredFaceDirection[axis];
                    if (std::abs(component) <= 1e-4f) {
                        continue;
                    }
                    const int adjacentFace = findSortedFaceByDirection(sortedFaces, axisDirectionFromComponent(axis, component));
                    if (adjacentFace >= 0) {
                        const auto [adjacentRow, adjacentCol] =
                            cellForFaceDirection(sortedFaces[static_cast<size_t>(adjacentFace)], vertices, hoveredFaceDirection);
                        highlightedCells[static_cast<size_t>(adjacentFace)] = adjacentRow * 3 + adjacentCol;
                    }
                }
            }
        }

        for (size_t sortedIndex = 0; sortedIndex < sortedFaces.size(); ++sortedIndex) {
            const SortedViewCubeFace& sortedFace = sortedFaces[sortedIndex];
            std::array<ImVec2, 4> quad{};
            std::array<glm::vec3, 4> localQuad{};
            for (int i = 0; i < 4; ++i) {
                const size_t cornerIndex = static_cast<size_t>(sortedFace.face.corners[static_cast<size_t>(i)]);
                quad[static_cast<size_t>(i)] = vertices[cornerIndex].screen;
                localQuad[static_cast<size_t>(i)] = vertices[cornerIndex].local;
            }
            orderFaceQuadForScreen(quad, localQuad);
            drawList->AddConvexPolyFilled(quad.data(), 4, cubeFillColor);

            if (sortedFace.frontFacing) {
                const int highlightedCell = highlightedCells[sortedIndex];
                if (highlightedCell >= 0) {
                    const int row = highlightedCell / 3;
                    const int col = highlightedCell % 3;
                    const std::array<ImVec2, 4> cellQuad = {{
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col)], kFaceCuts[static_cast<size_t>(row)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col + 1)], kFaceCuts[static_cast<size_t>(row)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col + 1)], kFaceCuts[static_cast<size_t>(row + 1)]),
                        bilerp(quad, kFaceCuts[static_cast<size_t>(col)], kFaceCuts[static_cast<size_t>(row + 1)]),
                    }};
                    drawList->AddConvexPolyFilled(cellQuad.data(), 4, cubeFaceHoverColor);
                }

                for (int split = 1; split <= 2; ++split) {
                    const float t = kFaceCuts[static_cast<size_t>(split)];
                    const ImVec2 v0 = bilerp(quad, t, 0.0f);
                    const ImVec2 v1 = bilerp(quad, t, 1.0f);
                    const ImVec2 h0 = bilerp(quad, 0.0f, t);
                    const ImVec2 h1 = bilerp(quad, 1.0f, t);
                    drawList->AddLine(v0, v1, cubeCutColor, 1.4f);
                    drawList->AddLine(h0, h1, cubeCutColor, 1.4f);
                }
            }
        }

        for (int edgeIndex : sortedEdgeIndices) {
            const ViewCubeEdge& edge = edges[static_cast<size_t>(edgeIndex)];
            const bool accentEdge = isAccentEdge(edge);
            if (accentEdge) {
                const int fromVertex = edge.startVertex == 4 ? edge.startVertex : edge.endVertex;
                const int toVertex = edge.startVertex == 4 ? edge.endVertex : edge.startVertex;
                const ViewCubeVertex& a = vertices[static_cast<size_t>(fromVertex)];
                const ViewCubeVertex& b = vertices[static_cast<size_t>(toVertex)];
                const bool edgeHighlighted =
                    highlightedCells[static_cast<size_t>(edge.firstFace)] >= 0 || highlightedCells[static_cast<size_t>(edge.secondFace)] >= 0;
                const glm::vec3 localAxis = vertices[static_cast<size_t>(toVertex)].local - vertices[static_cast<size_t>(fromVertex)].local;
                const ImU32 accentColor = edgeAccentColor(localAxis, edgeHighlighted);
                drawList->AddLine(a.screen, b.screen, accentColor, 2.2f);
            }
            else {
                drawList->AddLine(vertices[static_cast<size_t>(edge.startVertex)].screen,
                                  vertices[static_cast<size_t>(edge.endVertex)].screen, cubeEdgeColor, 2.2f);
            }
        }

        if (hoveredFace >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            if (camera.getMode() == Camera::Mode::Mode2D) {
                const int hoveredRow = hoveredFaceCell / 3;
                const int hoveredCol = hoveredFaceCell % 3;
                if (hoveredRow == 1 && hoveredCol == 1) {
                    camera.snapToDirection(sortedFaces[static_cast<size_t>(hoveredFace)].localDirection);
                }
                else if (hoveredRow == 1 || hoveredCol == 1) {
                    camera.snapToDirection(
                        screenStripeDirection(sortedFaces[static_cast<size_t>(hoveredFace)].localDirection, viewRotation, hoveredRow, hoveredCol));
                }
                else {
                    camera.snapToDirection(hoveredFaceDirection);
                }
                return;
            }

            const int hoveredRow = hoveredFaceCell / 3;
            const int hoveredCol = hoveredFaceCell % 3;
            const bool isCorner = hoveredRow != 1 && hoveredCol != 1;
            const bool isCenter = hoveredRow == 1 && hoveredCol == 1;

            if (isCorner) {
                camera.snapToDirection(hoveredFaceDirection);
            }
            else if (isCenter) {
                camera.snapToDirection(sortedFaces[static_cast<size_t>(hoveredFace)].localDirection);
            }
            else {
                camera.snapToDirection(hoveredFaceDirection);
            }
        }
    }
}

SceneViewport::SceneViewport(RendererType type, CaptureController& captureController) : captureController_(&captureController), renderer_(createRenderer(type)) {}

void SceneViewport::setScreenSize(int width, int height) {
    renderer_->camera.setScreenSize(glm::vec2(static_cast<float>(width), static_cast<float>(height)));
}

void SceneViewport::resetView() { renderer_->camera.resetView(); }

void SceneViewport::syncScene(const Lattice::Simulation& simulation) { App::Viewport::syncRendererWithSimulation(*renderer_, simulation); }

void SceneViewport::renderFrame(Lattice::Simulation& simulation, Interface& appInterface, const DebugViews& debugViews) {
    PROFILE_SCOPE("SceneViewport::renderFrame");

    UiState& uiState = appInterface.state();
    uiState.simStep = simulation.world().getSimStep();

    appInterface.update();
    renderer_->camera.update(ImGui::GetIO().DeltaTime);
    if (renderer_->getRenderDataCount() > simulation.activeWorldId()) {
        const RenderData& activeRenderData = renderer_->getRenderData(simulation.activeWorldId());
        if (activeRenderData.drawVectorField || activeRenderData.drawFieldArrows || activeRenderData.drawFieldContours) {
            simulation.world().updateVectorField();
        }
    }

    if (ToolsManager::pickingSystem != nullptr) {
        App::Viewport::syncRendererWithSimulation(*renderer_, simulation, &ToolsManager::pickingSystem->getSelectedAtomIds());
    }
    else {
        App::Viewport::syncRendererWithSimulation(*renderer_, simulation);
    }

    refreshAtomDebugViews(debugViews, simulation);
    captureController_->renderFrame(*renderer_, [&]() {
        ToolsManager::overlay.draw();
        drawViewCubeOverlay(renderer_->camera);
        appInterface.draw(*renderer_);
    });
}

bool SceneViewport::setRendererType(RendererType type, const Lattice::Simulation& simulation) {
    if (renderer_ && renderer_->camera.getMode() == Camera::Mode::Mode2D) {
        cached2DCameraState_.valid = true;
        cached2DCameraState_.position = renderer_->camera.getPosition();
        cached2DCameraState_.zoom = renderer_->camera.getZoom();
        cached2DCameraState_.direction = -renderer_->camera.getForwardVector();
        cached2DCameraState_.up = renderer_->camera.getOrthographicUp();
        cached2DCameraState_.center = renderer_->camera.getOrbitCenter();
    }

    std::unique_ptr<BaseRenderer> newRenderer = createRenderer(type);
    if (!newRenderer) {
        return false;
    }

    if (renderer_) {
        newRenderer->camera.setScreenSize(renderer_->camera.getScreenSize());
    }

    App::Viewport::syncRendererWithSimulation(*newRenderer, simulation);
    if (renderer_) {
        copyRenderSettings(*newRenderer, *renderer_);
    }
    if (type == RendererType::Renderer2D && cached2DCameraState_.valid) {
        newRenderer->camera.setOrthographicView(cached2DCameraState_.direction, cached2DCameraState_.up);
        newRenderer->camera.setPosition(cached2DCameraState_.position);
        newRenderer->camera.setZoom(cached2DCameraState_.zoom);
    }
    else {
        newRenderer->camera.resetView();
    }
    renderer_ = std::move(newRenderer);
    return true;
}

std::unique_ptr<BaseRenderer> SceneViewport::createRenderer(RendererType type) {
    switch (type) {
    case RendererType::Renderer2D:
        return std::make_unique<Renderer2D>();
    case RendererType::Renderer3D:
        return std::make_unique<Renderer3D>();
    }

    return nullptr;
}

void SceneViewport::copyRenderSettings(BaseRenderer& destination, const BaseRenderer& source) {
    if (destination.getRenderDataCount() == 0 || source.getRenderDataCount() == 0) {
        return;
    }

    const size_t count = std::min(destination.getRenderDataCount(), source.getRenderDataCount());
    for (size_t i = 0; i < count; ++i) {
        RenderData& target = destination.getRenderData(i);
        const RenderData& current = source.getRenderData(i);
        target.drawAtoms = current.drawAtoms;
        target.drawGrid = current.drawGrid;
        target.drawVectorField = current.drawVectorField;
        target.drawFieldArrows = current.drawFieldArrows;
        target.drawFieldContours = current.drawFieldContours;
        target.fieldAutoScale = current.fieldAutoScale;
        target.fieldPotentialScale = current.fieldPotentialScale;
        target.fieldCellSize = current.fieldCellSize;
        target.fieldSmoothing = current.fieldSmoothing;
        target.fieldContourStep = current.fieldContourStep;
        target.drawBonds = current.drawBonds;
        target.drawBox = current.drawBox;
        target.drawMemoryOrder = current.drawMemoryOrder;
        target.speedColorMode = current.speedColorMode;
        target.speedGradientMax = current.speedGradientMax;
        target.alpha = current.alpha;
    }
}
