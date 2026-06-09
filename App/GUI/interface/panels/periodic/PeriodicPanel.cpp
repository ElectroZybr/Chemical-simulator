#include "PeriodicPanel.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <string_view>

#include "Lattice/Engine/physics/Atom/AtomData.h"

namespace {
struct PeriodicElementSlot {
    AtomData::Type type;
    int column;
    int row;
};

constexpr std::array<PeriodicElementSlot, static_cast<size_t>(AtomData::Type::COUNT)> kPeriodicSlots{{
    {AtomData::Type::Z, 1, 0},

    {AtomData::Type::H, 0, 0},
    {AtomData::Type::He, 17, 0},

    {AtomData::Type::Li, 0, 1}, {AtomData::Type::Be, 1, 1}, {AtomData::Type::B, 12, 1}, {AtomData::Type::C, 13, 1},
    {AtomData::Type::N, 14, 1}, {AtomData::Type::O, 15, 1}, {AtomData::Type::F, 16, 1}, {AtomData::Type::Ne, 17, 1},

    {AtomData::Type::Na, 0, 2}, {AtomData::Type::Mg, 1, 2}, {AtomData::Type::Al, 12, 2}, {AtomData::Type::Si, 13, 2},
    {AtomData::Type::P, 14, 2}, {AtomData::Type::S, 15, 2}, {AtomData::Type::Cl, 16, 2}, {AtomData::Type::Ar, 17, 2},

    {AtomData::Type::K, 0, 3},  {AtomData::Type::Ca, 1, 3}, {AtomData::Type::Sc, 2, 3},  {AtomData::Type::Ti, 3, 3},
    {AtomData::Type::V, 4, 3},  {AtomData::Type::Cr, 5, 3}, {AtomData::Type::Mn, 6, 3},  {AtomData::Type::Fe, 7, 3},
    {AtomData::Type::Co, 8, 3}, {AtomData::Type::Ni, 9, 3}, {AtomData::Type::Cu, 10, 3}, {AtomData::Type::Zn, 11, 3},
    {AtomData::Type::Ga, 12, 3}, {AtomData::Type::Ge, 13, 3}, {AtomData::Type::As, 14, 3}, {AtomData::Type::Se, 15, 3},
    {AtomData::Type::Br, 16, 3}, {AtomData::Type::Kr, 17, 3},

    {AtomData::Type::Rb, 0, 4},  {AtomData::Type::Sr, 1, 4}, {AtomData::Type::Y, 2, 4},  {AtomData::Type::Zr, 3, 4},
    {AtomData::Type::Nb, 4, 4},  {AtomData::Type::Mo, 5, 4}, {AtomData::Type::Tc, 6, 4}, {AtomData::Type::Ru, 7, 4},
    {AtomData::Type::Rh, 8, 4},  {AtomData::Type::Pd, 9, 4}, {AtomData::Type::Ag, 10, 4}, {AtomData::Type::Cd, 11, 4},
    {AtomData::Type::In, 12, 4}, {AtomData::Type::Sn, 13, 4}, {AtomData::Type::Sb, 14, 4}, {AtomData::Type::Te, 15, 4},
    {AtomData::Type::I, 16, 4},  {AtomData::Type::Xe, 17, 4},

    {AtomData::Type::Cs, 0, 5},  {AtomData::Type::Ba, 1, 5}, {AtomData::Type::La, 2, 7}, {AtomData::Type::Ce, 3, 7},
    {AtomData::Type::Pr, 4, 7},  {AtomData::Type::Nd, 5, 7}, {AtomData::Type::Pm, 6, 7}, {AtomData::Type::Sm, 7, 7},
    {AtomData::Type::Eu, 8, 7},  {AtomData::Type::Gd, 9, 7}, {AtomData::Type::Tb, 10, 7}, {AtomData::Type::Dy, 11, 7},
    {AtomData::Type::Ho, 12, 7}, {AtomData::Type::Er, 13, 7}, {AtomData::Type::Tm, 14, 7}, {AtomData::Type::Yb, 15, 7},
    {AtomData::Type::Lu, 16, 7}, {AtomData::Type::Hf, 3, 5}, {AtomData::Type::Ta, 4, 5}, {AtomData::Type::W, 5, 5},
    {AtomData::Type::Re, 6, 5},  {AtomData::Type::Os, 7, 5}, {AtomData::Type::Ir, 8, 5}, {AtomData::Type::Pt, 9, 5},
    {AtomData::Type::Au, 10, 5}, {AtomData::Type::Hg, 11, 5}, {AtomData::Type::Tl, 12, 5}, {AtomData::Type::Pb, 13, 5},
    {AtomData::Type::Bi, 14, 5}, {AtomData::Type::Po, 15, 5}, {AtomData::Type::At, 16, 5}, {AtomData::Type::Rn, 17, 5},

    {AtomData::Type::Fr, 0, 6},  {AtomData::Type::Ra, 1, 6}, {AtomData::Type::Ac, 2, 8}, {AtomData::Type::Th, 3, 8},
    {AtomData::Type::Pa, 4, 8},  {AtomData::Type::U, 5, 8},  {AtomData::Type::Np, 6, 8}, {AtomData::Type::Pu, 7, 8},
    {AtomData::Type::Am, 8, 8},  {AtomData::Type::Cm, 9, 8}, {AtomData::Type::Bk, 10, 8}, {AtomData::Type::Cf, 11, 8},
    {AtomData::Type::Es, 12, 8}, {AtomData::Type::Fm, 13, 8}, {AtomData::Type::Md, 14, 8}, {AtomData::Type::No, 15, 8},
    {AtomData::Type::Lr, 16, 8}, {AtomData::Type::Rf, 3, 6}, {AtomData::Type::Db, 4, 6}, {AtomData::Type::Sg, 5, 6},
    {AtomData::Type::Bh, 6, 6},  {AtomData::Type::Hs, 7, 6}, {AtomData::Type::Mt, 8, 6}, {AtomData::Type::Ds, 9, 6},
    {AtomData::Type::Rg, 10, 6}, {AtomData::Type::Cn, 11, 6}, {AtomData::Type::Nh, 12, 6}, {AtomData::Type::Fl, 13, 6},
    {AtomData::Type::Mc, 14, 6}, {AtomData::Type::Lv, 15, 6}, {AtomData::Type::Ts, 16, 6}, {AtomData::Type::Og, 17, 6},
}};

constexpr int kColumnCount = 18;
constexpr int kMainRowCount = 7;
constexpr int kRowCount = 9;
constexpr const char* kZeriumLabel = "Ze";

std::string_view atomLabel(AtomData::Type type) {
    if (type == AtomData::Type::Z) {
        return kZeriumLabel;
    }
    return AtomData::symbol(type);
}

const PeriodicElementSlot& slotForType(AtomData::Type type) {
    return kPeriodicSlots[static_cast<size_t>(type)];
}

const ImVec4 kEmptyCellColor = ImVec4(0.15f, 0.19f, 0.24f, 0.82f);
const ImVec4 kTextColor = ImVec4(0.93f, 0.95f, 0.98f, 1.00f);
ImVec4 baseColorForCategory(AtomData::Category category) {
    switch (category) {
    case AtomData::Category::Custom:
        return ImVec4(0.24f, 0.36f, 0.54f, 1.00f);
    case AtomData::Category::AlkaliMetal:
        return ImVec4(0.58f, 0.28f, 0.28f, 1.00f);
    case AtomData::Category::AlkalineEarthMetal:
        return ImVec4(0.55f, 0.41f, 0.24f, 1.00f);
    case AtomData::Category::TransitionMetal:
        return ImVec4(0.35f, 0.40f, 0.56f, 1.00f);
    case AtomData::Category::PostTransitionMetal:
        return ImVec4(0.36f, 0.48f, 0.46f, 1.00f);
    case AtomData::Category::Metalloid:
        return ImVec4(0.40f, 0.52f, 0.32f, 1.00f);
    case AtomData::Category::ReactiveNonmetal:
        return ImVec4(0.28f, 0.50f, 0.42f, 1.00f);
    case AtomData::Category::Halogen:
        return ImVec4(0.25f, 0.56f, 0.60f, 1.00f);
    case AtomData::Category::NobleGas:
        return ImVec4(0.40f, 0.34f, 0.58f, 1.00f);
    case AtomData::Category::Lanthanide:
        return ImVec4(0.55f, 0.34f, 0.50f, 1.00f);
    case AtomData::Category::Actinide:
        return ImVec4(0.56f, 0.28f, 0.40f, 1.00f);
    case AtomData::Category::Unknown:
        return ImVec4(0.30f, 0.36f, 0.42f, 1.00f);
    }
    return ImVec4(0.30f, 0.36f, 0.42f, 1.00f);
}

ImVec4 tintColor(const ImVec4& color, float lift) {
    return ImVec4(
        std::min(color.x + lift, 1.0f),
        std::min(color.y + lift, 1.0f),
        std::min(color.z + lift, 1.0f),
        color.w
    );
}

ImVec4 mixColor(const ImVec4& base, const ImVec4& overlay, float amount) {
    return ImVec4(
        base.x + (overlay.x - base.x) * amount,
        base.y + (overlay.y - base.y) * amount,
        base.z + (overlay.z - base.z) * amount,
        base.w + (overlay.w - base.w) * amount
    );
}

void drawCellLabel(const ImVec2& min, const ImVec2& max, std::string_view label, float fontSize) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label.data(), label.data() + label.size());
    const ImVec2 textPos(
        min.x + (max.x - min.x - textSize.x) * 0.5f,
        min.y + (max.y - min.y - textSize.y) * 0.5f
    );
    drawList->AddText(font, fontSize, textPos, ImGui::GetColorU32(kTextColor), label.data(), label.data() + label.size());
}
}

int PeriodicPanel::decodeAtom(int index) {
    if (index < 0 || index >= static_cast<int>(AtomData::Type::COUNT)) {
        return static_cast<int>(AtomData::Type::Z);
    }
    return index;
}

void PeriodicPanel::draw(float scale, glm::ivec2 windowSize, int& selectedAtom) {
    const float cellSize = 31.0f * scale;
    const float cellGap = 3.0f * scale;
    const float leftPadding = 10.0f * scale;
    const float panelPeek = 9.0f * scale;
    const float topPadding = 8.0f * scale;
    const float bottomPadding = 10.0f * scale;
    const float rowGapBonus = 10.0f * scale;

    const float gridWidth = kColumnCount * cellSize + (kColumnCount - 1) * cellGap;
    const float gridHeight = kRowCount * cellSize + (kRowCount - 1) * cellGap + rowGapBonus;
    const float panelW = gridWidth + leftPadding * 2.0f;
    const float panelH = gridHeight + topPadding + bottomPadding;
    const float panelX = windowSize.x * 0.5f - panelW * 0.5f;

    const ImVec2 mouse = ImGui::GetMousePos();
    const float hiddenY = panelPeek - panelH;
    const float currentY = hiddenY + animProgress * panelH;
    const float hoverTop = 0.0f;
    const float topActivationHeight = 28.0f * scale;
    const float hoverBottom = std::max(currentY + panelH, topActivationHeight);
    const bool overVisiblePanel = mouse.x >= panelX && mouse.x <= panelX + panelW &&
                                  mouse.y >= hoverTop && mouse.y <= hoverBottom;

    const float target = overVisiblePanel ? 1.0f : 0.0f;
    const float step = std::min(ImGui::GetIO().DeltaTime * 12.0f, 1.0f);
    animProgress += (target - animProgress) * step;

    const float animY = hiddenY + animProgress * panelH;

    ImGui::SetNextWindowPos(ImVec2(panelX, animY));
    ImGui::SetNextWindowSize(ImVec2(panelW, panelH));
    ImGui::Begin("Periodic", nullptr, PANEL_FLAGS);

    const float startX = leftPadding;
    const float startY = topPadding;
    const float fontSize = std::max(12.0f, cellSize * 0.60f);

    std::array<std::array<bool, kColumnCount>, kRowCount> occupied{};
    for (const PeriodicElementSlot& slot : kPeriodicSlots) {
        occupied[static_cast<size_t>(slot.row)][static_cast<size_t>(slot.column)] = true;
    }

    for (int row = 0; row < kRowCount; ++row) {
        for (int column = 0; column < kColumnCount; ++column) {
            if (occupied[static_cast<size_t>(row)][static_cast<size_t>(column)]) {
                continue;
            }

            const float extraRowOffset = row >= kMainRowCount ? rowGapBonus : 0.0f;
            const ImVec2 min(
                startX + column * (cellSize + cellGap),
                startY + row * (cellSize + cellGap) + extraRowOffset
            );
            const ImVec2 max(min.x + cellSize, min.y + cellSize);
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(kEmptyCellColor), 5.0f * scale);
        }
    }

    for (int i = 0; i < static_cast<int>(AtomData::Type::COUNT); ++i) {
        const AtomData::Type type = static_cast<AtomData::Type>(i);
        const PeriodicElementSlot& slot = slotForType(type);
        const float extraRowOffset = slot.row >= kMainRowCount ? rowGapBonus : 0.0f;
        const float x = startX + slot.column * (cellSize + cellGap);
        const float y = startY + slot.row * (cellSize + cellGap) + extraRowOffset;

        ImGui::SetCursorPos(ImVec2(x, y));
        const std::string_view label = atomLabel(type);
        ImGui::InvisibleButton(label.data(), ImVec2(cellSize, cellSize));

        const bool hovered = ImGui::IsItemHovered();
        const bool selected = selectedAtom == i;
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const AtomData::Category category = AtomData::category(type);
        const ImVec4 baseColor = baseColorForCategory(category);
        const ImVec4 hoverColor = tintColor(baseColor, 0.08f);
        const ImVec4 selectedColor = mixColor(baseColor, kTextColor, 0.18f);
        const ImVec4 fillColor = selected ? selectedColor : (hovered ? hoverColor : baseColor);
        ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(fillColor), 5.0f * scale);
        if (selected) {
            const ImVec4 outlineColor = mixColor(baseColor, kTextColor, 0.45f);
            ImGui::GetWindowDrawList()->AddRect(min, max, ImGui::GetColorU32(outlineColor), 5.0f * scale, 0, 1.5f * scale);
        }

        drawCellLabel(min, max, label, fontSize);

        if (hovered) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f * scale, 6.0f * scale));
            ImGui::BeginTooltip();
            ImGui::SetWindowFontScale(0.5f);
            ImGui::Text("%s (%s) - %s", AtomData::name(type).data(), AtomData::symbol(type).data(), AtomData::categoryName(category).data());
            ImGui::EndTooltip();
            ImGui::PopStyleVar();
        }

        if (ImGui::IsItemClicked()) {
            selectedAtom = (selectedAtom == i) ? static_cast<int>(AtomData::Type::Z) : i;
        }
    }

    ImGui::End();
}
