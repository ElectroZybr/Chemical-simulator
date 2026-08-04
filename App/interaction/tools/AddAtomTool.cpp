#include "AddAtomTool.h"

#include "Lattice/Engine/Simulation.h"
#include "GUI/interface/UiState.h"
#include "GUI/interface/panels/periodic/PeriodicPanel.h"
#include "Rendering/BaseRenderer.h"

AddAtomTool::AddAtomTool(ToolContext& context) noexcept : ITool(context) {}

void AddAtomTool::onLeftPressed(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (!ctx.isValid()) {
        return;
    }

    if (ctx.uiState == nullptr) {
        return;
    }

    const AtomData::Type atomType = static_cast<AtomData::Type>(PeriodicPanel::decodeAtom(ctx.uiState->selectedAtom));
    const bool spawnMolecule = ctx.simulation->findMoleculeTemplate(ctx.uiState->spawnSpecies) != nullptr;
    glm::vec3 spawnPos = screenToLocalWorld(mousePos);
    const std::optional<glm::mat3> moleculeRotation =
        spawnMolecule ? std::optional<glm::mat3>(Lattice::Simulation::randomRotationMatrix()) : std::nullopt;

    if (!ctx.simulation->canSpawnMolecule(ctx.uiState->spawnSpecies, spawnPos, moleculeRotation)) {
        return;
    }

    if (spawnMolecule) {
        (void)ctx.simulation->spawnMoleculeChecked(ctx.uiState->spawnSpecies, spawnPos, moleculeRotation, false);
        return;
    }

    launchOrigin_ = spawnPos;
    launchedAtomId_ = ctx.simulation->appendAtomFast(launchOrigin_, glm::vec3(0.0f), atomType, false);
    ctx.simulation->finishAtomBatch();
}

void AddAtomTool::onLeftReleased(glm::ivec2 mousePos) {
    ToolContext& ctx = context();
    if (!ctx.isValid() || launchedAtomId_ == InvalidAtomId) {
        return;
    }

    AtomStorage& atoms = ctx.simulation->atoms();
    const size_t atomIndex = atoms.indexOf(launchedAtomId_);
    if (atomIndex >= atoms.size()) {
        launchedAtomId_ = InvalidAtomId;
        return;
    }

    const glm::vec3 releaseWorld = screenToLocalWorld(mousePos);
    constexpr float kLaunchVelocityScale = 1.2f;
    const glm::vec3 launchVelocity = (releaseWorld - launchOrigin_) * kLaunchVelocityScale;
    atoms.setVel(atomIndex, launchVelocity);
    launchedAtomId_ = InvalidAtomId;
}

void AddAtomTool::reset() {
    launchedAtomId_ = InvalidAtomId;
    launchOrigin_ = glm::vec3(0.0f);
}
