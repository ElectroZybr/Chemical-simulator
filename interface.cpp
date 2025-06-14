#include <SFML/Graphics.hpp>

#include "Interface.h"

#include <iostream>


#define WIDHT   800
#define HEIGHT  600

#define ICON_MIN_FA 0xf000
#define ICON_MAX_FA 0xf897

#define ICON_FA_FLASK u8"\uf0c3"
#define ICON_FA_COG u8"\uf013"
#define ICON_FA_PAUSE u8"\uf04c"
#define ICON_FA_PLAY u8"\uf04b"
#define ICON_FA_FORWARD u8"\uf04e"
#define ICON_FA_BACKWARD u8"\uf04a"
#define ICON_FA_FAST_FORWARD u8"\uf050"
#define ICON_FA_FAST_BACKWARD u8"\uf049"


sf::RenderWindow* Interface::window = nullptr;
ImGuiStyle* Interface::style = nullptr;
ImGuiStyle Interface::baseStyle;
ImFont* Interface::Rubik_VariableFont_wght = nullptr;
ImFont* Interface::Font_Awesome = nullptr;
sf::Clock Interface::clock;
float Interface::current_ui_scale;
int Interface::selectedAtom = -1;
bool Interface::pause;
bool Interface::cursorHovered = false;



void Interface::custom_style() {
    Interface::style = &ImGui::GetStyle();
    ImVec4* colors = style->Colors;

    style->WindowPadding = ImVec2(7.5, 7.5);
    style->WindowRounding = 7.5;
    style->FramePadding = ImVec2(2.5, 2.5);
    style->ItemSpacing = ImVec2(6, 4);
    style->ItemInnerSpacing = ImVec2(4, 3);
    style->IndentSpacing = 12.5;
    style->ScrollbarSize = 7.5;
    style->ScrollbarRounding = 7.5;
    style->GrabMinSize = 7.5;
    style->GrabRounding = 3.5;
    style->FrameRounding = 3;

    colors[ImGuiCol_Text] = ImVec4(0.95, 0.96, 0.98, 1.00);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.36, 0.42, 0.47, 1.00);
    colors[ImGuiCol_WindowBg] = ImVec4(0.11, 0.15, 0.17, 1.00);
    colors[ImGuiCol_ChildBg] = ImVec4(0.15, 0.18, 0.22, 1.00);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08, 0.08, 0.08, 0.94);
    colors[ImGuiCol_Border] = ImVec4(0.43, 0.43, 0.50, 0.50);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00, 0.00, 0.00, 0.00);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12, 0.20, 0.28, 1.00);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.09, 0.12, 0.14, 1.00);
    colors[ImGuiCol_TitleBg] = ImVec4(0.09, 0.12, 0.14, 0.65);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.00, 0.00, 0.00, 0.51);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08, 0.10, 0.12, 1.00);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15, 0.18, 0.22, 1.00);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02, 0.02, 0.02, 0.39);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18, 0.22, 0.25, 1.00);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09, 0.21, 0.31, 1.00);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37, 0.61, 1.00, 1.00);
    colors[ImGuiCol_Button] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.18, 0.23, 0.25, 1.00);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06, 0.53, 0.98, 1.00);
    colors[ImGuiCol_Header] = ImVec4(0.20, 0.25, 0.29, 0.55);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26, 0.59, 0.98, 0.80);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26, 0.59, 0.98, 1.00);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26, 0.59, 0.98, 0.25);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26, 0.59, 0.98, 0.67);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06, 0.05, 0.07, 1.00);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61, 0.61, 0.61, 1.00);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00, 0.43, 0.35, 1.00);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90, 0.70, 0.00, 1.00);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00, 0.60, 0.00, 1.00);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25, 1.00, 0.00, 0.43);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00, 0.98, 0.95, 0.73);
    baseStyle = *style;
    current_ui_scale = 1;
}


int Interface::init(sf::RenderWindow& w) {
    window = &w;

    if (!ImGui::SFML::Init(*window)) return EXIT_FAILURE;

    custom_style();

    // style->ScaleAllSizes(0.5);
    ImGui::GetIO().FontGlobalScale = 0.75;

    // Загружаем шрифты
    Interface::Rubik_VariableFont_wght = ImGui::GetIO().Fonts->AddFontFromFileTTF("Engine/gui/fonts/Rubik-VariableFont_wght.ttf", 50.0f);

    // Загружаем иконки
    ImFontConfig config;
    config.MergeMode = true; // Важно!
    config.GlyphMinAdvanceX = 40.0f;
    static const ImWchar icon_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
    Interface::Font_Awesome = ImGui::GetIO().Fonts->AddFontFromFileTTF("Engine/gui/fonts/Font Awesome 5 Free-Solid-900.otf", 40.0f, &config, icon_ranges);

    if (!ImGui::SFML::UpdateFontTexture()) return EXIT_FAILURE;
}

void Interface::CheckEvent(const sf::Event& event) {
    if (event.type == sf::Event::Resized) {
        // Пересчитываем масштаб ImGui
        // window->setView(sf::View(sf::FloatRect(0, 0, event.size.width, event.size.height)));
        // ImGui::GetIO().DisplaySize = ImVec2(event.size.width, event.size.height);
        float scale_W = static_cast<float>(event.size.width) / WIDHT;
        float scale_H = static_cast<float>(event.size.height) / HEIGHT;
        current_ui_scale = scale_H < scale_W ? scale_H : scale_W;
        // current_ui_scale = std::clamp(current_ui_scale, 1.0f, 1.5f);
        ImGui::GetStyle() = baseStyle;
        style->ScaleAllSizes(current_ui_scale);
        
        ImGui::GetIO().FontGlobalScale = current_ui_scale*(1./4.*3);
        std::cout << current_ui_scale << ' ' << ImGui::GetIO().FontGlobalScale << std::endl;
    }
}

bool Interface::getPause() {
    return pause;
}

int Interface::getSelectedAtom() {
    static int decode[] = { 1, -1, -1, -1, -1, -1, -1,  2, 
                            3,  4,  5,  6,  7,  8,  9, 10,
                           11, 12, 13, 14, 15, 16, 17, 18};
    if (selectedAtom != -1)
        return decode[selectedAtom];
    return -1;
}

int Interface::Update() {
    ImGui::SFML::Update(*window, clock.restart());
    cursorHovered = false;
    

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(122*current_ui_scale, 65*current_ui_scale));

    ImGui::Begin("Tools", nullptr, 
        ImGuiWindowFlags_NoMove |           // Запретить перемещение
        ImGuiWindowFlags_NoResize |         // Запретить изменение размера
        ImGuiWindowFlags_NoCollapse |       // Убрать кнопку сворачивания
        ImGuiWindowFlags_NoTitleBar |       // Скрыть заголовок
        ImGuiWindowFlags_NoScrollbar
    );
    // std::cout << ImGui::GetContentRegionAvail().x << std::endl;
    
    ImGui::PushFont(Rubik_VariableFont_wght);

    // ImGui::BeginChild("dsd", ImVec2(100*current_ui_scale, 50*current_ui_scale), true);

    if (ImGui::Button(ICON_FA_COG, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
        ImGui::OpenPopup("my_popup");
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_FLASK, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
        ImGui::OpenPopup("my_popup");
    }

    // Само выпадающее меню
    if (ImGui::BeginPopup("my_popup")) {
        if (ImGui::MenuItem("save")) { /* действие */ }
        if (ImGui::MenuItem("open")) { /* действие */ }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("exit")) { /* действие */ }
        ImGui::EndPopup();
    }
    
    // // Проверка на вхождение курсора в область
    // if (ImGui::IsItemHovered())
    //     cursorHovered = true;

    ImGui::PopFont();
    ImGui::End();



    ImGui::SetNextWindowPos(ImVec2(window->getSize().x / 2 - (387*current_ui_scale/2), 0));
    ImGui::SetNextWindowSize(ImVec2(387*current_ui_scale, 142*current_ui_scale));
    ImGui::Begin("Top panel", nullptr, 
        ImGuiWindowFlags_NoMove |           // Запретить перемещение
        ImGuiWindowFlags_NoResize |         // Запретить изменение размера
        ImGuiWindowFlags_NoCollapse |       // Убрать кнопку сворачивания
        ImGuiWindowFlags_NoTitleBar |       // Скрыть заголовок
        ImGuiWindowFlags_NoScrollbar
    );

    static const char* keys[] = {"H",  " ",  " ",  " ",  " ", " ", " ",  "He", 
                                 "Li", "Be", "B",  "C",  "N", "O", "F",  "Ne",
                                 "Na", "Mg", "Al", "Si", "P", "S", "Cl", "Ar"};

    
    bool flag = false;

    ImGui::PushFont(Rubik_VariableFont_wght);
    for (int i = 0; i < IM_ARRAYSIZE(keys); i++) {
        // Меняем стиль для выбранной кнопки
        if (i == selectedAtom) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06, 0.53, 0.98, 1.00));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06, 0.53, 0.98, 1.00));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06, 0.53, 0.98, 1.00));
        }
        
        if (ImGui::Button(keys[i], ImVec2(40*current_ui_scale, 40*current_ui_scale))) {
            flag = true;
        }
        
        if (i == selectedAtom) {
            ImGui::PopStyleColor(3);
        }
        
        if (flag) {
            if (selectedAtom != i)
                selectedAtom = i;
            else 
                selectedAtom = -1;
            flag = false;
        }
        

        // Располагаем кнопки в ряд с отступами
        if ((i + 1) % 8 != 0) ImGui::SameLine(0.0f, 7.5f*current_ui_scale);
    }

    // // Проверка на вхождение курсора в область
    // if (ImGui::IsItemHovered())
    //     cursorHovered = true;

    ImGui::PopFont();
    ImGui::End();


    
    ImGui::SetNextWindowPos(ImVec2(window->getSize().x - (122*current_ui_scale), 0));
    ImGui::SetNextWindowSize(ImVec2(122*current_ui_scale, 65*current_ui_scale));

    ImGui::Begin("Poops", nullptr, 
        ImGuiWindowFlags_NoMove |           // Запретить перемещение
        ImGuiWindowFlags_NoResize |         // Запретить изменение размера
        ImGuiWindowFlags_NoCollapse |       // Убрать кнопку сворачивания
        ImGuiWindowFlags_NoTitleBar |       // Скрыть заголовок
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::PushFont(Rubik_VariableFont_wght);

    static int fast = 0;    // 0 - обычная скорость, 1 - x2, 2 - x3
    if (fast == 0) {
        if (ImGui::Button(ICON_FA_FORWARD, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
            fast++;
        }
    } else if (fast == 1) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06, 0.53, 0.98, 1.00));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06, 0.53, 0.98, 1.00));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06, 0.53, 0.98, 1.00));
        if (ImGui::Button(ICON_FA_FORWARD, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
            fast++;
        }
        ImGui::PopStyleColor(3);
    } else if (fast == 2) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.06, 0.53, 0.98, 1.00));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06, 0.53, 0.98, 1.00));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.06, 0.53, 0.98, 1.00));
        if (ImGui::Button(ICON_FA_FAST_FORWARD, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
            fast = 0;
        }
        ImGui::PopStyleColor(3);
    }
    ImGui::SameLine();

    if (pause) {
        if (ImGui::Button(ICON_FA_PAUSE, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
            pause = false;
        }
    } else {
        if (ImGui::Button(ICON_FA_PLAY, ImVec2(50*current_ui_scale, 50*current_ui_scale))) {
            pause = true;
        }
    }
    // if (ImGui::IsItemHovered()) {
    //     ImGui::SetTooltip("podscazka");
    // }

    // Само выпадающее меню
    if (ImGui::BeginPopup("my_popup")) {
        if (ImGui::MenuItem("save")) { /* действие */ }
        if (ImGui::MenuItem("open")) { /* действие */ }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("exit")) { /* действие */ }
        ImGui::EndPopup();
    }

    // // Проверка на вхождение курсора в область
    // if (ImGui::IsItemHovered())
    //     cursorHovered = true;

    ImGui::PopFont();
    ImGui::End();



    ImGui::SetNextWindowPos(ImVec2(window->getSize().x - (150*current_ui_scale), window->getSize().y - (50*current_ui_scale)));
    ImGui::SetNextWindowSize(ImVec2(window->getSize().x, window->getSize().y));
    ImGui::Begin("Stats", nullptr, 
        ImGuiWindowFlags_NoMove |           // Запретить перемещение
        ImGuiWindowFlags_NoResize |         // Запретить изменение размера
        ImGuiWindowFlags_NoCollapse |       // Убрать кнопку сворачивания
        ImGuiWindowFlags_NoTitleBar |       // Скрыть заголовок
        ImGuiWindowFlags_NoScrollbar
    );
    ImGui::PushFont(Rubik_VariableFont_wght);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::PopFont();
    ImGui::End();
    
    cursorHovered = ImGui::IsAnyItemHovered() || ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup);
}