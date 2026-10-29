// Portado do Horizonte; fórmulas originais preservadas.
#include "terrain_noise.h"
namespace zz::horizonte::terrain_internal {
int fd(int a, int b) {
  int q = a / b, r = a % b;
  return q - (r < 0);
}
int mod(uint64_t h, int b) {
  int64_t s;
  if (h >> 63) {
    uint64_t magnitude = (~h) + 1;
    int r = int(magnitude % b);
    return r ? b - r : 0;
  }
  s = int64_t(h);
  return int(s % b);
}
uint64_t hash(uint64_t seed, int x, int y, int z) {
  uint64_t n = seed + uint64_t(int64_t(x)) * 374761393ULL +
               uint64_t(int64_t(z)) * 668265263ULL +
               uint64_t(int64_t(y)) * 1442695041ULL;
  n = (n ^ (n >> 13)) * 1274126177ULL;
  return n ^ (n >> 16);
}
double noise(uint64_t seed, double x, double z) {
  int ix = int(std::floor(x)), iz = int(std::floor(z));
  double fx = x - ix, fz = z - iz;
  fx = fx * fx * (3 - 2 * fx);
  fz = fz * fz * (3 - 2 * fz);
  auto v = [&](int a, int b) {
    return (hash(seed, a, 0, b) & 0xffffff) / double(0xffffff);
  };
  double a = v(ix, iz) * (1 - fx) + v(ix + 1, iz) * fx,
         b = v(ix, iz + 1) * (1 - fx) + v(ix + 1, iz + 1) * fx;
  return a * (1 - fz) + b * fz;
}
bool cave(uint64_t seed, int x, int y, int z, int top) {
  if (y < 3 || y > top - 4)
    return false;
  double t = 9 + 4 * std::sin(x * .075) + 3 * std::cos(z * .08);
  return std::abs(y - t) < 1.6 && noise(seed + 907, x * .055, z * .055) > .40;
}
} // namespace zz::horizonte::terrain_internal
