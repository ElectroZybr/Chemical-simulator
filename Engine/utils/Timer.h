#ifndef SHOOTER_TIMER_H
#define SHOOTER_TIMER_H

#include <iostream>
#include <chrono>
#include <ctime>
#include <cmath>

class Timer {
private:
    std::chrono::high_resolution_clock::time_point _startTime;
    std::chrono::high_resolution_clock::time_point _endTime;
    bool _isRunning = false;

public:
    void start();
    void stop();

    [[nodiscard]] double elapsedMilliseconds() const;
    [[nodiscard]] double elapsedSeconds() const;
};


#endif
