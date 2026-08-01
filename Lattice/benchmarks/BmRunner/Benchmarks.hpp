#pragma once
#include <chrono>

class Benchmarks {
private:
    using Clock = std::chrono::high_resolution_clock;

    void run();
};