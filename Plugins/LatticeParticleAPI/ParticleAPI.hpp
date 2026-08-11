#pragma once

struct IntegratorAPI {
    static constexpr std::string_view apiName = "IntegratorAPI";
    
    void* instance = nullptr;
    void (*step)(void* instance, float dt) = nullptr;
};
