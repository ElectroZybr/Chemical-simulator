// #pragma once

// #include <string_view>

// #include <GLFW/glfw3.h>

// namespace GPU {
// class  GPUAPI {
// public:
//     static constexpr std::string_view apiName = "GPUAPI";
//     virtual ~GPUAPI() = default;

//     virtual void init(GLFWwindow* window, uint32_t width, uint32_t height) = 0;
//     virtual void initHeadless(uint32_t width, uint32_t height) = 0;

//     virtual void resize(uint32_t width, uint32_t height) = 0;

//     virtual void processEvents() = 0;
//     virtual void waitIdle() = 0;

//     virtual void shutdown() = 0;
// };
// }