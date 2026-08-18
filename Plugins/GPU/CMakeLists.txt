project(GPU)
message(STATUS "настройка плагина ${PROJECT_NAME}")

file(GLOB_RECURSE PLUGIN_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
    src/*.hpp
)

add_library(${PROJECT_NAME} SHARED 
    PluginInit.cpp
    ${PLUGIN_SOURCES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${webgpu_cpp_SOURCE_DIR}/wgpu-native
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Lattice::Kernel
    webgpu
    glfw
)

message(STATUS "настройка плагина ${PROJECT_NAME} завершена") 