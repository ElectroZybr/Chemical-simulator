#include <cstdint>
#include <glm/vec3.hpp>

namespace Morton3D {
    inline uint64_t expandBits(uint32_t v) {
        uint64_t x = v & 0x1fffff;
        x = (x | (x << 32)) & 0x1f00000000ffff;
        x = (x | (x << 16)) & 0x1f0000ff0000ff;
        x = (x | (x << 8))  & 0x100f00f00f00f00f;
        x = (x | (x << 4))  & 0x10c30c30c30c30c3;
        x = (x | (x << 2))  & 0x1249249249249249;
        return x;
    }

    inline uint64_t encode(uint32_t x, uint32_t y, uint32_t z) {
        return expandBits(x) | (expandBits(y) << 1) | (expandBits(z) << 2);
    }

    inline uint32_t compactBits(uint64_t x) {
        x &= 0x1249249249249249;
        x = (x ^ (x >> 2)) & 0x10c30c30c30c30c3;
        x = (x ^ (x >> 4)) & 0x100f00f00f00f00f;
        x = (x ^ (x >> 8)) & 0x1f0000ff0000ff;
        x = (x ^ (x >> 16)) & 0x1f00000000ffff;
        x = (x ^ (x >> 32)) & 0x1fffff;
        return static_cast<uint32_t>(x);
    }
}