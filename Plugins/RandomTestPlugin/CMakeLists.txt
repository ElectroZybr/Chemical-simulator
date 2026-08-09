project(RandomTestPlugin)
message(STATUS "настройка плагина ${PROJECT_NAME}")

add_library(${PROJECT_NAME} SHARED 
    PluginInit.cpp
    src/hello.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Lattice::Kernel
)

message(STATUS "настройка плагина ${PROJECT_NAME} завершена")
