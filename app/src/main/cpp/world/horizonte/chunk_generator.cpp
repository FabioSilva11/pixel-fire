// Grounded Horizonte terrain: caves, vegetation and villages; no airborne land masses.
#include "terrain_grid.h"
namespace zz::horizonte {
using namespace terrain_internal;
std::vector<uint16_t> Terrain::chunk(int cx, int cz) const {
  Grid w(16, cx * 16, cz * 16);
  auto set = [&](int x, int y, int z, int t) { w.set(x, y, z, t); };
  for (int lx = 0; lx < 16; lx++)
    for (int lz = 0; lz < 16; lz++) {
      int x = cx * 16 + lx, z = cz * 16 + lz, top = groundHeight(x, z),
          b = biome(x, z);
      for (int y = 0; y <= std::max(top, 16); y++) {
        int t = y > top       ? WATER
                : y == top    ? (top <= 17 || b == 1 ? SAND
                                 : b == 2            ? SNOW
                                                     : GRASS)
                : y > top - 4 ? (b == 1 ? SAND : DIRT)
                              : STONE;
        if (y > 1 && y < top - 4) {
          auto h = hash(seed_, x, y, z);
          if (cave(seed_, x, y, z, top))
            t = y < 6 ? LAVA : AIR;
          else if (mod(h, 113) == 0)
            t = IRON;
          else if (y < 10 && mod(h, 313) == 0)
            t = blockId("diamond_ore");
        }
        set(x, y, z, t);
      }

    }
  for (int ax = fd(cx * 16 - 3, 8) * 8; ax < cx * 16 + 19; ax += 8)
    for (int az = fd(cz * 16 - 3, 8) * 8; az < cz * 16 + 19; az += 8) {
      int top = groundHeight(ax, az);
      if (top <= 17 || biome(ax, az) == 1 ||
          mod(hash(seed_, ax, 0, az), 5) != 0)
        continue;
      for (int y = top + 1; y < top + 6; y++)
        set(ax, y, az, WOOD);
      for (int dx = -2; dx <= 2; dx++)
        for (int dz = -2; dz <= 2; dz++)
          for (int dy = 4; dy <= 6; dy++)
            if (std::abs(dx) + std::abs(dz) < 4)
              set(ax + dx, top + dy, az + dz,
                  biome(ax, az) == 2 && dy == 6 ? SNOW : LEAVES);
    }
  applyVillages(w, *this, cx, cz);
  return std::move(w.b);
}
} // namespace zz::horizonte
