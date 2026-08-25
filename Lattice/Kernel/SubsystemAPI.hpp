#pragma once


struct SubsystemAPI {
    SubsystemAPI() = default;
    SubsystemAPI(const SubsystemAPI&) = delete;
    SubsystemAPI& operator=(const SubsystemAPI&) = delete;
    SubsystemAPI& operator=(SubsystemAPI&&) = delete;
};