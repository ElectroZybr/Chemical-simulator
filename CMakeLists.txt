cmake_minimum_required(VERSION 3.17)
project(Chemical-simulator)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Настройка SFML
set(SFML_DIR "C:/Users/user/Documents/Coding/Chemical-simulator/Libraries/sfml/lib/cmake/SFML")
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Добавление ImGui и ImGui-SFML
set(IMGUI_DIR "C:/Users/user/Documents/Coding/Chemical-simulator/Libraries/imgui")
add_subdirectory(Libraries/imgui)
add_subdirectory(Libraries/imgui-sfml)

# Исходные файлы
add_executable(${CMAKE_PROJECT_NAME}
        # custom files:
        main.cpp
        interface.h
        interface.cpp
        # engine:
        engine/Consts.h
        engine/math/Vec2D.h
        engine/math/Vec2D.cpp
        engine/physics/Atom.h
        engine/physics/Atom.cpp
        engine/physics/SpatialGrid.h
        engine/physics/SpatialGrid.cpp
        engine/Camera.h
        engine/Camera.cpp
        )

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE sfml-system sfml-network sfml-graphics sfml-window sfml-audio)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ImGui-SFML::ImGui-SFML)