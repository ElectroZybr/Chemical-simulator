#include <SFML/Graphics.hpp>
#include <cmath>
#include "engine/Camera.h"
#include <iostream>

#include <SFML/Graphics.hpp>
#include <functional>
#include <memory>

#include <windows.h>
#include <dwmapi.h>

#include "imgui.h"
#include "imgui-SFML.h"

#define WIGHT   800
#define HEIGHT  600

// #pragma comment(lib, "dwmapi.lib")

// // Установить цвет заголовка окна
// void SetTitleBarColor(HWND hwnd, COLORREF color) {
//     BOOL darkMode = TRUE;
//     DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

//     DWMCOLORIZATIONPARAMS params;
//     DwmGetColorizationParameters(&params);
//     params.clrColor = color;  // Новый цвет (в формате 0x00BBGGRR)
//     DwmSetColorizationParameters(&params, 0);
// }

void apply_custom_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // imgui.SwitchContext()
    // local colors = style.Colors
    // local clr = imgui.Col
    // local ImVec4 = imgui.ImVec4
    // local ImVec2 = imgui.ImVec2

    style.WindowPadding = ImVec2(15, 15);
    style.WindowRounding = 15.0;
    style.FramePadding = ImVec2(5, 5);
    style.ItemSpacing = ImVec2(12, 8);
    style.ItemInnerSpacing = ImVec2(8, 6);
    style.IndentSpacing = 25.0;
    style.ScrollbarSize = 15.0;
    style.ScrollbarRounding = 15.0;
    style.GrabMinSize = 15.0;
    style.GrabRounding = 7.0;
    // style.ChildWindowRounding = 8.0;
    style.FrameRounding = 6.0;
  

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
    // colors[clr.ComboBg] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_CheckMark] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37, 0.61, 1.00, 1.00);
    colors[ImGuiCol_Button] = ImVec4(0.20, 0.25, 0.29, 1.00);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28, 0.56, 1.00, 1.00);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.06, 0.53, 0.98, 1.00);
    colors[ImGuiCol_Header] = ImVec4(0.20, 0.25, 0.29, 0.55);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.26, 0.59, 0.98, 0.80);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.26, 0.59, 0.98, 1.00);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.26, 0.59, 0.98, 0.25);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26, 0.59, 0.98, 0.67);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06, 0.05, 0.07, 1.00);
    // colors[clr.CloseButton] = ImVec4(0.40, 0.39, 0.38, 0.16);
    // colors[clr.CloseButtonHovered] = ImVec4(0.40, 0.39, 0.38, 0.39);
    // colors[clr.CloseButtonActive] = ImVec4(0.40, 0.39, 0.38, 1.00);
    colors[ImGuiCol_PlotLines] = ImVec4(0.61, 0.61, 0.61, 1.00);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00, 0.43, 0.35, 1.00);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.90, 0.70, 0.00, 1.00);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00, 0.60, 0.00, 1.00);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25, 1.00, 0.00, 0.43);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00, 0.98, 0.95, 0.73);
    // apply_custom_style();
}

int main() {
    sf::RenderWindow window(sf::VideoMode(WIGHT, HEIGHT), "Chemical-simulator");
    window.setFramerateLimit(60);
    
    if (!ImGui::SFML::Init(window)) return -1;

    apply_custom_style();

    // ImGuiStyle& style = ImGui::GetStyle();
    // ImVec4* colors = style.Colors;
    // style.FrameRounding = 8.0f;  // Скругление углов
    // colors[ImGuiCol_Button] = ImVec4(0.12f, 0.35f, 0.58f, 1.00f);         // Основной цвет
    // colors[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.45f, 0.72f, 1.00f);  // При наведении
    // colors[ImGuiCol_ButtonActive] = ImVec4(0.08f, 0.25f, 0.42f, 1.00f);   // При нажатии

    // // Акцентные цвета
    // colors[ImGuiCol_Button]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    // colors[ImGuiCol_ButtonHovered]  = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    // colors[ImGuiCol_ButtonActive]   = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
    // ImGui::GetStyle().ScaleAllSizes(0.3);

    ImGui::GetStyle().ScaleAllSizes(0.5);
    ImGui::GetIO().FontGlobalScale = 0.5;
    





    // Создаем игровое окно и камеру
    sf::View gameView = window.getDefaultView();
    Camera camera(window, gameView);

    // Создаем окно для интерфейса
    sf::View uiView = window.getDefaultView();
    
    // Создаем тестовые объекты
    sf::CircleShape circle(50.f);
    circle.setFillColor(sf::Color::Red);
    circle.setPosition(300, 200);
    
    sf::RectangleShape rect(sf::Vector2f(100, 100));
    rect.setFillColor(sf::Color::Blue);
    rect.setPosition(500, 400);

    std::vector<sf::Vertex> gridLines;
    const int gridSize = 100;
    for (int x = -1000; x <= 1000; x += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, -1000), sf::Color(60, 60, 60)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(x, 1000), sf::Color(60, 60, 60)));
    }
    for (int y = -1000; y <= 1000; y += gridSize) {
        gridLines.push_back(sf::Vertex(sf::Vector2f(-1000, y), sf::Color(60, 60, 60)));
        gridLines.push_back(sf::Vertex(sf::Vector2f(1000, y), sf::Color(60, 60, 60)));
    }

    ImGuiIO& io = ImGui::GetIO();
    ImFont* imguiFont = io.Fonts->AddFontFromFileTTF("Engine/gui/fonts/Rubik-VariableFont_wght.ttf", 50.0f);
    if (!ImGui::SFML::UpdateFontTexture()) std::cerr << "Warning: Font texture update failed. Text may not render correctly.\n";
    
    sf::Clock clock;
    
    while (window.isOpen()) {
        float deltaTime = clock.restart().asSeconds();
        
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);

            if (event.type == sf::Event::Closed)
                window.close();

            if (event.type == sf::Event::Resized) {
                uiView.setSize(event.size.width, event.size.height);
                uiView.setCenter(event.size.width/2, event.size.height/2);

                // text.setPosition(sf::Vector2f(window.getSize()) - sf::Vector2f(400, 100));

                ImGui::GetIO().DisplaySize = ImVec2(event.size.width, event.size.height);

    
                // // Пересчитываем масштаб ImGui
                // window.setView(sf::View(sf::FloatRect(0, 0, event.size.width, event.size.height)));
                // float uiScale = WIGHT / 0.001 * static_cast<float>(event.size.width);
                // std::cout << uiScale << std::endl;
                // ImGui::GetStyle().ScaleAllSizes(uiScale);
                // ImGui::GetIO().FontGlobalScale = uiScale;

            }

            

            camera.handleEvent(event, window);
        }


        ImGui::SFML::Update(window, clock.restart());

        // ImGui::Begin("Hello, world!");
        // ImGui::Button("Look at this pretty button");
        // ImGui::Button("Look at this pretty button");
        // ImGui::End();

        
        ImGui::SetNextWindowPos(ImVec2(0, 0));  // Левый верхний угол
        ImGui::SetNextWindowSize(ImVec2(window.getSize().x, 120));  // Ширина на весь экран, высота 50px
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f)); // Темно-серый фон
        
        ImGui::PushFont(imguiFont);
        ImGui::Begin("Top Menu", nullptr, 
            ImGuiWindowFlags_NoMove |           // Запретить перемещение
            ImGuiWindowFlags_NoResize |         // Запретить изменение размера
            ImGuiWindowFlags_NoCollapse |       // Убрать кнопку сворачивания
            ImGuiWindowFlags_NoTitleBar |       // Скрыть заголовок
            ImGuiWindowFlags_NoScrollbar
        );

        // Кнопки меню
        if (ImGui::Button("File")) { /* ... */ }
        ImGui::SameLine();
        if (ImGui::Button("Edit")) { /* ... */ }

        int t;
        ImGui::SameLine();
        if (ImGui::SliderInt("Edit0", &t, 0, 100)) { /* ... */ }

        static bool toggled = false;

        // Стилизация
        ImVec4 active_color = ImVec4(0.2f, 0.7f, 0.3f, 1.0f); // Зеленый при активации
        ImVec4 inactive_color = ImVec4(0.7f, 0.2f, 0.2f, 1.0f); // Красный при деактивации

        ImGui::PushStyleColor(ImGuiCol_Button, toggled ? active_color : inactive_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, toggled ? active_color : inactive_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, toggled ? active_color : inactive_color);

        if (ImGui::Button(toggled ? "ON" : "OFF", ImVec2(100, 40))) {
            toggled = !toggled;
        }

        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();


        ImGui::End();
        ImGui::PopFont();
        ImGui::PopStyleColor(); // Не забываем убрать стиль


        static int selected_key = -1;  // -1 означает ничего не выбрано
        static const char* keys[] = {"H", "C", "O", "F", "Cu"};
        bool flag = false;

        ImGui::PushFont(imguiFont);
        ImGui::Begin("Keyboard");
        for (int i = 0; i < IM_ARRAYSIZE(keys); i++) {
            // Меняем стиль для выбранной кнопки
            if (i == selected_key) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.69f, 1.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.69f, 1.0f, 1.0f));
            }
            
            if (ImGui::Button(keys[i], ImVec2(40, 40))) {
                flag = true;
            }
            
            if (i == selected_key) {
                ImGui::PopStyleColor(3);
            }
            
            if (flag) {
                selected_key = i;  // Выбираем текущую кнопку
                flag = false;
            }
            

            // Располагаем кнопки в ряд с отступами
            if ((i + 1) % 5 != 0) ImGui::SameLine(0.0f, 2.0f);
        }
        ImGui::End();
        ImGui::PopFont();


        



        // char tx[50];
        // sf::Vector2i mpos = sf::Mouse::getPosition(window);
        // sprintf(tx, "x:%d y:%d", mpos.x, mpos.y);
        // text.setString(tx);

        // Управление камерой
        camera.handleInput(deltaTime, window);
        
        
        // Обновление камеры
        camera.update(deltaTime, window);

        // button.setPosition(sf::Vector2f(350, 275) + camera.getPosition());
        
        // Очистка
        window.clear(sf::Color(35, 35, 35, 255));
        
        // Рисуем игровые объекты
        window.setView(gameView);
        window.draw(circle);
        window.draw(rect);
        window.draw(&gridLines[0], gridLines.size(), sf::Lines);

        // Рисуем интерфейс
        window.setView(uiView);
        // window.draw(text);    
        ImGui::SFML::Render(window);
        
        // Вывод на экран
        window.display();
    }
    ImGui::SFML::Shutdown();
    
    return 0;
}