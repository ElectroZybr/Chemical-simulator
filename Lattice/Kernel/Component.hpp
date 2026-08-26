#pragma once

class Components;

class Component {
public:
    virtual ~Component() = default;
    virtual void configure(Components&) {}
};