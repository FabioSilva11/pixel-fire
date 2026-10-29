// Portado do Horizonte; fórmulas originais preservadas.
#pragma once
#include "terrain.h"
#include <algorithm>
#include <cmath>
namespace zz::horizonte::terrain_internal {
enum {
  AIR,
  GRASS,
  DIRT,
  STONE,
  WOOD,
  LEAVES,
  PLANK,
  BRICK,
  WATER,
  SAND,
  SNOW,
  CRYSTAL,
  IRON,
  LANTERN,
  ROOF,
  BLUE,
  GLASS,
  LAVA = 19
};
struct Random {
  uint64_t s;
  explicit Random(uint64_t seed)
      : s((seed ^ 0x5deece66dULL) & ((1ULL << 48) - 1)) {}
  uint32_t next(int n) {
    s = (s * 0x5deece66dULL + 11) & ((1ULL << 48) - 1);
    return uint32_t(s >> (48 - n));
  }
  float fl() { return next(24) / float(1 << 24); }
  int integer(int b) {
    if ((b & -b) == b)
      return int((uint64_t(b) * next(31)) >> 31);
    uint32_t bits, v;
    do {
      bits = next(31);
      v = bits % b;
    } while (bits - v + uint32_t(b - 1) >= 0x80000000U);
    return int(v);
  }
};
int fd(int, int);
int mod(uint64_t, int);
uint64_t hash(uint64_t, int, int, int);
double noise(uint64_t, double, double);
bool cave(uint64_t, int, int, int, int);
} // namespace zz::horizonte::terrain_internal
