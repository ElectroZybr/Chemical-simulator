cmake_minimum_required(VERSION 3.10)

project(imgui)

# Основные исходные файлы ImGui
set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_demo.cpp
)

# Создаём библиотеку
add_library(imgui STATIC ${IMGUI_SOURCES})

# Устанавливаем корректные свойства для MSVC (если нужно)
if(MSVC)
    target_compile_definitions(imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Подключаем стандартные include-директории
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})