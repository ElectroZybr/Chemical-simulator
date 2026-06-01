add_executable(rendering_module_tests
    CameraTests.cpp
)

target_link_libraries(rendering_module_tests PRIVATE
    LatticeLab::Rendering
)

target_copy_webgpu_binaries(rendering_module_tests)

add_test(NAME rendering.module COMMAND rendering_module_tests)
