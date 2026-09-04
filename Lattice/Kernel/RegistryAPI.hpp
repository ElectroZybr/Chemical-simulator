#pragma once


class RegistryAPI {
public:
    virtual ~RegistryAPI() = default;

    virtual void reg() = 0;
};