#include "AddAtomTool.h"

#include <cstdlib>

#include "Lattice/Engine/Simulation.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"
#include "GUI/interface/UiState.h"
#include "GUI/interface/panels/periodic/PeriodicPanel.h"
#include "Rendering/BaseRenderer.h"

namespace {
    glm::vec3 randomVelocity(float scale) {
        auto randomComponent = []() {
            return 2.0f * static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX) - 1.0f;
        };
        return glm::vec3(randomComponent(), randomComponent(), randomComponent()) * scale;
    }
}

AddAtomTool::AddAtomTool(ToolContext& context) noexcept : ITool(context) {}

void AddAtomTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (!ctx.isValid()) {
        return;
    }

    if (ctx.uiState == nullptr) {
        return;
    }

    AtomStorage& atoms = ctx.simulation->atoms();
    const World& box = ctx.simulation->world();
    const AtomData::Type atomType = static_cast<AtomData::Type>(PeriodicPanel::decodeAtom(ctx.uiState->selectedAtom));
    glm::vec3 spawnPos = screenToLocalWorld(mousePos);
    const bool is2D = ctx.activeRenderer() != nullptr && ctx.activeRenderer()->camera.getMode() == Camera::Mode::Mode2D;

    if (!(1 <= spawnPos.x && spawnPos.x <= box.getWorldSize().x - 1 && 1 <= spawnPos.y && spawnPos.y <= box.getWorldSize().y - 1 &&
          1 <= spawnPos.z && spawnPos.z <= box.getWorldSize().z - 1)) {
        return;
    }

    const float atomRadius = AtomData::getProps(atomType).radius;
    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        const glm::vec3 atomPos = atoms.pos(atomIndex);
        const float radius = AtomData::getProps(atoms.type(atomIndex)).radius;
        if (glm::length(atomPos - spawnPos) <= 2.f * (radius + atomRadius)) {
            return;
        }
    }

    glm::vec3 velocity = randomVelocity(5.0f);
    if (is2D) {
        const glm::vec3 planeNormal = -ctx.activeRenderer()->camera.getForwardVector();
        velocity -= planeNormal * glm::dot(velocity, planeNormal);
    }
    ctx.simulation->createAtom(spawnPos, velocity, atomType, false);
}
