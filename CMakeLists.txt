cmake_minimum_required(VERSION 3.17)
project(Chemical_simulator)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(${CMAKE_PROJECT_NAME}
        # custom files:
        main.cpp
        # 3d engine:
        engine/Consts.h
        engine/math/Vec2D.h
        engine/math/Vec2D.cpp
        engine/Camera.h
        engine/Camera.cpp
        )


set(SFML_DIR "C:/Libraries/SFML/lib/cmake/SFML")
set(SFML_STATIC_LIBRARIES TRUE)

find_package(SFML 2.5.1 COMPONENTS graphics audio REQUIRED)

if (SFML_FOUND)
    include_directories(${SFML_INCLUDE_DIR})
endif()

target_link_libraries(${CMAKE_PROJECT_NAME} sfml-audio sfml-network sfml-graphics sfml-window sfml-system)
