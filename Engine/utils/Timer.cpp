#include "Timer.h"

using namespace std::chrono;

void Timer::start() {
    _startTime = high_resolution_clock::now();
    _isRunning = true;
}

void Timer::stop() {
    _endTime = high_resolution_clock::now();
    _isRunning = false;
}

double Timer::elapsedMilliseconds() const {
    return elapsedSeconds()*1000;
}

double Timer::elapsedSeconds() const {
    high_resolution_clock::time_point endTime;

    if(_isRunning) {
        endTime = high_resolution_clock::now();
    } else {
        endTime = _endTime;
    }

    return duration<double>(endTime - _startTime).count();
}
