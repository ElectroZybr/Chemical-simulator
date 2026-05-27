# Исправление бага: раньше CTest имел ноль зарегистрированных тестов. Этот
# executable собирает точечные regression-проверки для багов physics/load/package.
add_executable(latticelab_tests
    RegressionTests.cpp
)

target_link_libraries(latticelab_tests PRIVATE latticelab_lib)

if(COMMAND target_copy_webgpu_binaries)
    # Исправление бага: тесты линкуют тот же backend-код, что и приложение,
    # поэтому WebGPU runtime DLL тоже копируется рядом с executable тестов.
    target_copy_webgpu_binaries(latticelab_tests)
endif()

add_test(NAME latticelab.regression COMMAND latticelab_tests)
