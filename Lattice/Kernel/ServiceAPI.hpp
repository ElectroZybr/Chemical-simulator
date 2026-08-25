#pragma once

#include <thread>
#include <atomic>

enum class ServiceLaunch {
    Worker,
    Host
};

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

    void enter() {
        if (running_.exchange(true))
            return;
        host_ = true;
        run();
        running_ = false;
    }

    void stop() {
        requestStop();
        if (thread_.joinable())
            thread_.join();
        running_ = false;
    }

    bool running() const { return running_.load(); }
    bool host() const { return host_; }

    virtual ~ServiceAPI() {
        stop();
    }

protected:
    virtual void run() = 0;

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
    bool host_ = false;
};