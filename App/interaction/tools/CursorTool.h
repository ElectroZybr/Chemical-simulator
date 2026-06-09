#pragma once

#include "App/interaction/tools/ITool.h"
#include "Lattice/Engine/physics/Atom/AtomStorage.h"

class CursorTool final : public ITool {
public:
    explicit CursorTool(ToolContext& context) noexcept;

    void onLeftPressed(glm::ivec2 mousePos) override;
    void onLeftReleased(glm::ivec2 mousePos) override;
    void onFrame(glm::ivec2 mousePos, float deltaTime) override;
    void reset() override;

private:
    static constexpr AtomStorage::AtomId InvalidAtomId = AtomStorage::InvalidAtomId;

    AtomStorage::AtomId selectedMoveAtomId_ = InvalidAtomId;
    bool atomMoveActive_ = false;
};
