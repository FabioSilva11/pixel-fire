// Portado do Horizonte; fórmulas originais preservadas.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace zz::horizonte {
uint16_t blockId(std::string name);
struct Village {
  int x, z, biome;
  uint64_t id() const { return (uint64_t(uint32_t(x)) << 32) | uint32_t(z); }
  std::array<std::array<int, 3>, 4> homes() const;
};
class Terrain {
  int64_t seed_;

public:
  explicit Terrain(int64_t seed) : seed_(seed) {}
  std::vector<uint16_t> chunk(int cx, int cz) const;
  int groundHeight(int x, int z) const;
  int biome(int x, int z) const;
  std::vector<Village> villagesNear(int x, int z, int radius) const;
};
} // namespace zz::horizonte
