#include "FontManager.h"

#include <algorithm>
#include <cmath>

#include "generated/fonts/Font_Awesome_5_Free_Solid_900.h"
#include "generated/fonts/Rubik_VariableFont_wght.h"

#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf897

bool FontManager::load(float uiScale) {
    auto* fonts = ImGui::GetIO().Fonts;
    uiScale = std::clamp(uiScale, 0.75f, 3.0f);
    fonts->Clear();

    const auto scaledSize = [uiScale](float basePx) { return std::max(12.0f, std::round(basePx * uiScale)); };

    static const ImWchar iconRanges[] = {ICON_MIN_FA, ICON_MAX_FA, 0};
    static const ImWchar ruRanges[] = {
        0x0020, 0x00FF, 0x0400, 0x04FF, 0,
    };

    ImFontConfig mainConfig{};
    mainConfig.OversampleH = 4;
    mainConfig.OversampleV = 4;
    mainConfig.FontDataOwnedByAtlas = false;
    main = fonts->AddFontFromMemoryTTF((void*)s_font_Rubik_VariableFont_wght, sizeof(s_font_Rubik_VariableFont_wght), scaledSize(37.5f),
                                       &mainConfig);
    if (!main) {
        return false;
    }

    ImFontConfig iconConfig{};
    iconConfig.MergeMode = true;
    iconConfig.PixelSnapH = true;
    iconConfig.GlyphMinAdvanceX = scaledSize(30.0f);
    iconConfig.FontDataOwnedByAtlas = false;
    if (!fonts->AddFontFromMemoryTTF((void*)s_font_Font_Awesome_5_Free_Solid_900, sizeof(s_font_Font_Awesome_5_Free_Solid_900),
                                     scaledSize(30.0f), &iconConfig, iconRanges)) {
        return false;
    }

    ImFontConfig sideIconConfig{};
    sideIconConfig.PixelSnapH = true;
    sideIconConfig.OversampleH = 4;
    sideIconConfig.OversampleV = 4;
    sideIconConfig.FontDataOwnedByAtlas = false;
    icons = fonts->AddFontFromMemoryTTF((void*)s_font_Font_Awesome_5_Free_Solid_900, sizeof(s_font_Font_Awesome_5_Free_Solid_900),
                                        scaledSize(22.5f), &sideIconConfig, iconRanges);
    if (!icons) {
        return false;
    }

    ImFontConfig dialogConfig{};
    dialogConfig.OversampleH = 4;
    dialogConfig.OversampleV = 4;
    dialogConfig.FontDataOwnedByAtlas = false;
    dialog = fonts->AddFontFromMemoryTTF((void*)s_font_Rubik_VariableFont_wght, sizeof(s_font_Rubik_VariableFont_wght), scaledSize(18.0f),
                                         &dialogConfig, ruRanges);
    if (!dialog) {
        return false;
    }

    return true;
}
