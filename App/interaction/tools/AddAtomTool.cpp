#include "AddAtomTool.h"

#include "Engine/Simulation.h"
#include "Engine/physics/AtomStorage.h"
#include "GUI/interface/UiState.h"
#include "GUI/interface/panels/periodic/PeriodicPanel.h"
#include "Rendering/BaseRenderer.h"

AddAtomTool::AddAtomTool(ToolContext& context) noexcept : ITool(context) {}

void AddAtomTool::onLeftPressed(Vec2i mousePos) {
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
    Vec3f spawnPos = screenToLocalWorld(mousePos);
    const bool is2D = ctx.activeRenderer() != nullptr && ctx.activeRenderer()->camera.getMode() == Camera::Mode::Mode2D;
    if (is2D) {
        spawnPos.z = box.getWorldSize().z * 0.5f;
    }

    if (!(1 <= spawnPos.x && spawnPos.x <= box.getWorldSize().x - 1 && 1 <= spawnPos.y && spawnPos.y <= box.getWorldSize().y - 1 &&
          1 <= spawnPos.z && spawnPos.z <= box.getWorldSize().z - 1)) {
        return;
    }

    const float atomRadius = AtomData::getProps(atomType).radius;
    for (size_t atomIndex = 0; atomIndex < atoms.size(); ++atomIndex) {
        const Vec3f atomPos = atoms.pos(atomIndex);
        const float radius = AtomData::getProps(atoms.type(atomIndex)).radius;
        if ((atomPos - spawnPos).abs() <= 2.f * (radius + atomRadius)) {
            return;
        }
    }

    Vec3f velocity = Vec3f::Random() * 5.f;
    if (is2D) {
        velocity.z = 0.0f;
    }
    ctx.simulation->createAtom(spawnPos, velocity, atomType, false);
}
