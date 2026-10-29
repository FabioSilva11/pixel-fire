#pragma once
#include <cstdint>
namespace zz {
// 13 bits para cada coordenada horizontal e 7 para y (6144 x 128 x 6144).
inline uint64_t packPosition(int x, int y, int z) {
    return (uint64_t(x) << 20) | (uint64_t(z) << 7) | uint64_t(y);
}
inline int positionX(uint64_t p) { return int(p >> 20); }
inline int positionY(uint64_t p) { return int(p & 127u); }
inline int positionZ(uint64_t p) { return int((p >> 7) & 8191u); }
}
