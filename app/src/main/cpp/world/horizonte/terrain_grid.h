// Portado do Horizonte; fórmulas originais preservadas.
#pragma once
#include "terrain_noise.h"
namespace zz::horizonte::terrain_internal {
struct Grid {
  std::vector<uint16_t> b;
  int size, ox, oz;
  Grid(int s, int x = 0, int z = 0) : b(s * s * 128), size(s), ox(x), oz(z) {}
  void set(int x, int y, int z, int t) {
    x -= ox;
    z -= oz;
    if (x >= 0 && z >= 0 && x < size && z < size && y >= 0 && y < 128)
      b[(y * size + z) * size + x] = uint16_t(t);
  }
  int get(int x, int y, int z) const {
    if (x < 0 || z < 0 || x >= size || z >= size || y < 0 || y >= 128)
      return 0;
    return b[(y * size + z) * size + x];
  }
  int surface(int x, int z) {
    for (int y = 127; y >= 0; y--) {
      int t = get(x, y, z);
      if (t && t != WATER && t != LAVA && t != 17 && t != 18)
        return y;
    }
    return -1;
  }
  void box(int x, int y, int z, int sx, int sy, int sz, int t) {
    for (int a = x; a < x + sx; a++)
      for (int c = z; c < z + sz; c++)
        for (int h = y; h < y + sy; h++)
          set(a, h, c, t);
  }
  void tree(int x, int y, int z, bool snow) {
    if (y < 1)
      return;
    box(x, y, z, 1, 5, 1, WOOD);
    box(x - 2, y + 3, z - 2, 5, 2, 5, LEAVES);
    box(x - 1, y + 5, z - 1, 3, 2, 3, snow ? SNOW : LEAVES);
  }
};
void applyVillages(Grid &, const Terrain &, int, int);
} // namespace zz::horizonte::terrain_internal
