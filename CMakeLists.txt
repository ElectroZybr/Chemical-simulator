cmake_minimum_required(VERSION 3.17)
project(Chemical-simulator)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Настройка SFML
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/sfml/lib/cmake/SFML")
set(SFML_STATIC_LIBRARIES ON)
find_package(SFML 2.5 COMPONENTS graphics window system REQUIRED)

# Добавление ImGui и ImGui-SFML
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Libraries/imgui")
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
        engine/math/Vec3D.h
        engine/math/Vec3D.cpp
        engine/physics/Atom.h
        engine/physics/Atom.cpp
        engine/physics/Bond.h
        engine/physics/Bond.cpp
        engine/physics/BondTable.h
        engine/physics/BondTable.cpp
        engine/physics/SpatialGrid.h
        engine/physics/SpatialGrid.cpp
        engine/Camera.h
        engine/Camera.cpp
        engine/Renderer.h
        engine/Renderer.cpp
        engine/Simulation.h
        engine/Simulation.cpp
        engine/Tools.h
        engine/Tools.cpp
        )

set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.rc")
target_sources(Chemical-simulator PRIVATE ${APP_ICON_RESOURCE})

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE sfml-system sfml-network sfml-graphics sfml-window sfml-audio)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ImGui-SFML::ImGui-SFML)