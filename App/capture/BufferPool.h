#pragma once

#include <cstddef>
#include <deque>

#include <webgpu/webgpu-raii.hpp>

// Все методы вызываются строго из render thread — синхронизация не нужна.
class BufferPool {
public:
    BufferPool() = default;

    // Вызвать один раз перед первым захватом.
    void init(size_t bufferSize, size_t initialCount);

    // Взять буфер. Если пул пуст — создаёт новый
    wgpu::Buffer acquire();

    // Вернуть буфер после Unmap(). Должен быть размаппен.
    void release(wgpu::Buffer buffer);

    bool isInitialized() const { return bufferSize > 0; }
    size_t freeCount() const { return free.size(); }

private:
    wgpu::Buffer createBuffer() const;

    size_t bufferSize = 0;
    std::deque<wgpu::Buffer> free;
};