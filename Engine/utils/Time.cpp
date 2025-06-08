//
// Created by Иван Ильин on 11.01.2021.
//

#include "Time.h"
#include "Log.h"
#include "../Consts.h"

using namespace std::chrono;

Time *Time::_instance = nullptr;

void Time::init() {
    delete _instance;
    _instance = new Time();

    Log::log("Time::init(): time was initialized");
}

double Time::time() {
    if (_instance == nullptr) {
        return 0;
    }

    return _instance->_time;
}

double Time::deltaTime() {
    if (_instance == nullptr) {
        return 0;
    }

    return _instance->_deltaTime;
}

void Time::update() {
    if (_instance == nullptr) {
        return;
    }

    high_resolution_clock::time_point t = high_resolution_clock::now();

    _instance->_deltaTime = duration<double>(t - _instance->_last).count();
    _instance->_time = duration<double>(t - _instance->_start).count();
    // in case when fps < 10 it is useful to decrease _deltaTime (to avoid collision problems)
    if (_instance->_deltaTime > Consts::LARGEST_TIME_STEP) {
        _instance->_deltaTime = Consts::LARGEST_TIME_STEP;
    }

    _instance->_last = t;

    if (_instance->_deltaTime > 10) {
        return;
    }

    _instance->_fpsCounter++;
    if (t - _instance->_fpsStart > _instance->_fpsCountTime) {
        _instance->_lastFps = _instance->_fpsCounter / duration<double>(t - _instance->_fpsStart).count();
        _instance->_fpsCounter = 0;
        _instance->_fpsStart = t;
    }
}

int Time::fps() {
    if (_instance == nullptr) {
        return 0;
    }
    // Cast is faster than floor and has the same behavior for positive numbers
    return static_cast<int>(_instance->_lastFps);
}

void Time::startTimer(const std::string &timerName) {
    if (_instance == nullptr) {
        return;
    }
    _instance->_timers.insert({timerName, Timer()});
    _instance->_timers[timerName].start();
}

void Time::stopTimer(const std::string &timerName) {
    if (_instance == nullptr) {
        return;
    }
    if(_instance->_timers.count(timerName) > 0) {
        _instance->_timers[timerName].stop();
    }
}

double Time::elapsedTimerMilliseconds(const std::string &timerName) {
    if (_instance == nullptr) {
        return 0;
    }
    if(_instance->_timers.count(timerName) > 0) {
        return _instance->_timers[timerName].elapsedMilliseconds();
    }
    return 0;
}

double Time::elapsedTimerSeconds(const std::string &timerName) {
    if (_instance == nullptr) {
        return 0;
    }
    if(_instance->_timers.count(timerName) > 0) {
        return _instance->_timers[timerName].elapsedSeconds();
    }
    return 0;
}

std::map<std::string, Timer> const &Time::timers() {
    if(_instance != nullptr)
        return _instance->_timers;
    else
        return std::map<std::string, Timer>();
}

void Time::free() {
    delete _instance;
    _instance = nullptr;

    Log::log("Time::free(): pointer to 'Time' was freed");
}
