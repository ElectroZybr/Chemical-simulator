#include "ProfilerTreeView.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "Lattice/Engine/metrics/Profiler.h"

namespace {
    struct ProfileNameAlias {
        std::string_view fullName;
        std::string_view shortName;
    };

    constexpr std::array<ProfileNameAlias, 21> kProfileNameAliases{{
        {"Application::RenderFrame", "Render"},
        {"Capture::readback", "capture"},
        {"Capture::encodeFrame", "encode"},
        {"Simulation::update", "Physics"},
        {"RendererGL::drawShot", "drawShot"},
        {"RendererGL::drawAtoms", "drawAtoms"},
        {"RendererGL::drawBox", "drawBox"},
        {"RendererGL::drawBondsGL", "drawBonds"},
        {"RendererGL::drawGridGL", "drawGrid"},
        {"ForceField::compute", "compute"},
        {"ForceField::ComputeForces(NL)", "forces NL"},
        {"StepOps::computeForces", "step forces"},
        {"VerletScheme::pipeline", "verlet"},
        {"VerletScheme::correct", "verlet correct"},
        {"KDKScheme::pipeline", "kdk"},
        {"KDKScheme::halfKick", "kick"},
        {"KDKScheme::drift", "drift"},
        {"NeighborList::build", "nl build"},
        {"SpatialGrid::rebuild", "sg rebuild"},
        {"ForceField::Coulomb", "coulomb"},
    }};

    std::string shortenProfileName(std::string_view name) {
        for (const ProfileNameAlias& alias : kProfileNameAliases) {
            if (name == alias.fullName) {
                return std::string(alias.shortName);
            }
        }

        const size_t lastScope = name.rfind("::");
        if (lastScope != std::string_view::npos) {
            return std::string(name.substr(lastScope + 2));
        }
        return std::string(name);
    }

    std::string formatNodeLabel(std::string_view label, double ms, double percent) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%.*s  %.3f ms | %.1f%%", static_cast<int>(label.size()), label.data(), ms, percent);
        return buffer;
    }

    bool isVisibleNode(const ProfileTreeEntry& entry) { return entry.smoothedMs > 0.0001; }

    void drawTreeNode(const std::vector<ProfileTreeEntry>& entries, size_t index, double totalTrackedMs) {
        if (index >= entries.size()) {
            return;
        }

        const ProfileTreeEntry& entry = entries[index];
        if (!isVisibleNode(entry)) {
            return;
        }

        const double percent = totalTrackedMs > 0.0 ? (entry.smoothedMs * 100.0 / totalTrackedMs) : 0.0;

        std::vector<size_t> visibleChildren;
        visibleChildren.reserve(entry.childIndices.size());
        for (const size_t childIndex : entry.childIndices) {
            if (childIndex < entries.size() && isVisibleNode(entries[childIndex])) {
                visibleChildren.emplace_back(childIndex);
            }
        }

        std::sort(visibleChildren.begin(), visibleChildren.end(),
                  [&](size_t lhs, size_t rhs) { return entries[lhs].smoothedMs > entries[rhs].smoothedMs; });

        const std::string shortName = shortenProfileName(entry.name);
        const std::string visibleLabel = formatNodeLabel(shortName, entry.smoothedMs, percent);
        const std::string nodeLabel = visibleLabel + "###tree_" + std::to_string(index);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
        if (entry.parentIndex == ProfileTreeEntry::kNoParent) {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }
        if (visibleChildren.empty()) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::TreeNodeEx(nodeLabel.data(), flags);
            return;
        }

        if (ImGui::TreeNodeEx(nodeLabel.data(), flags)) {
            for (const size_t childIndex : visibleChildren) {
                drawTreeNode(entries, childIndex, totalTrackedMs);
            }
            ImGui::TreePop();
        }
    }
}

void drawProfilerTreeView(float uiScale) {
    (void)uiScale;

    const Profiler& profiler = Profiler::instance();
    const std::vector<ProfileTreeEntry>& treeEntries = profiler.treeEntries();

    std::vector<size_t> rootIndices;
    rootIndices.reserve(treeEntries.size());
    double accountedPercent = 0.0;
    for (size_t i = 0; i < treeEntries.size(); ++i) {
        const ProfileTreeEntry& entry = treeEntries[i];
        if (entry.parentIndex == ProfileTreeEntry::kNoParent && isVisibleNode(entry)) {
            rootIndices.emplace_back(i);
            accountedPercent += entry.smoothedPercentOfFrame;
        }
    }

    std::sort(rootIndices.begin(), rootIndices.end(),
              [&](size_t lhs, size_t rhs) { return treeEntries[lhs].smoothedMs > treeEntries[rhs].smoothedMs; });

    accountedPercent = std::clamp(accountedPercent, 0.0, 100.0);
    double totalTrackedMs = 0.0;
    for (const size_t rootIndex : rootIndices) {
        totalTrackedMs += treeEntries[rootIndex].smoothedMs;
    }

    ImGui::TextDisabled("Загрузка CPU");
    ImGui::SameLine();
    ImGui::Text("%.1f%%", accountedPercent);
    ImGui::ProgressBar(static_cast<float>(accountedPercent / 100.0), ImVec2(-1.0f, 0.0f));
    ImGui::Separator();

    for (const size_t rootIndex : rootIndices) {
        drawTreeNode(treeEntries, rootIndex, totalTrackedMs);
    }
}
