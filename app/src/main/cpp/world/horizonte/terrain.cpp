// Portado do Horizonte; fórmulas originais preservadas.
#include "terrain_grid.h"
namespace zz::horizonte {
using namespace terrain_internal;
int Terrain::groundHeight(int x, int z) const {
  double h = noise(seed_, x / 29.0, z / 29.0) * 13 +
             noise(seed_, x / 12.0, z / 12.0) * 4;
  int top = 14 + int(h);
  double river =
             64 + 16 * std::sin(z * .046) + 8 * (noise(seed_, z * .04, 2) - .5),
         bank = std::abs(x - river);
  if (bank < 7)
    top = std::min(top, 12 + int(bank * .8));
  return top;
}
int Terrain::biome(int x, int z) const {
  double v = noise(uint64_t(seed_) + 417, x / 180.0, z / 180.0);
  return v < .3 ? 1 : v > .7 ? 2 : 0;
}
} // namespace zz::horizonte
