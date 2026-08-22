#pragma once

#include <thread>
#include <atomic>

struct ServiceAPI {
    ServiceAPI() = default;
    ServiceAPI(const ServiceAPI&) = delete;
    ServiceAPI& operator=(const ServiceAPI&) = delete;
    ServiceAPI& operator=(ServiceAPI&&) = delete;

    void start() {
        if (running_.exchange(true))
            return;

        thread_ = std::thread([this] {
            run();
            running_ = false;
        });
    }

    void stop() {
        if (!running_ && !thread_.joinable())
            return;

        requestStop();
        if (thread_.joinable())
            thread_.join();
        running_ = false;
    }

    bool running() const { return running_.load(); }

    virtual ~ServiceAPI() {
        stop();
    }

protected:
    // сервис реализует свой цикл
    virtual void run() = 0;

    // прерывание блокирующих wait
    virtual void requestStop() {
        stopRequested_ = true;
    }

    bool stopRequested() const {
        return stopRequested_.load();
    }

private:
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
};