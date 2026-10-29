// Portado do Horizonte; fórmulas originais preservadas.
#include "terrain_grid.h"
namespace zz::horizonte {
using namespace terrain_internal;
std::array<std::array<int, 3>, 4> Village::homes() const {
  return {{{x - 19, z - 16, 0},
           {x + 12, z - 16, 1},
           {x - 19, z + 12, 2},
           {x + 12, z + 12, 0}}};
}
std::vector<Village> Terrain::villagesNear(int x, int z, int radius) const {
  std::vector<Village> out;
  if (std::abs(x - 64) <= radius + 45 && std::abs(z - 176) <= radius + 45)
    out.push_back({64, 176, 0});
  for (int cx = fd(x - radius - 45, 544); cx <= fd(x + radius + 45, 544); cx++)
    for (int cz = fd(z - radius - 45, 544); cz <= fd(z + radius + 45, 544);
         cz++) {
      Random r(uint64_t(seed_) + uint64_t(int64_t(cx)) * 341873128712ULL +
               uint64_t(int64_t(cz)) * 132897987541ULL + 10387312);
      int vx = cx * 544 + r.integer(26) * 16 + 8,
          vz = cz * 544 + r.integer(26) * 16 + 8;
      if (vx >= -45 && vx <= 172 && vz >= -45 && vz <= 172)
        continue;
      if (std::hypot(vx - 64, vz - 176) < 90 || groundHeight(vx, vz) <= 18)
        continue;
      if (std::abs(x - vx) <= radius + 45 && std::abs(z - vz) <= radius + 45)
        out.push_back({vx, vz, biome(vx, vz)});
    }
  return out;
}
namespace terrain_internal {
void applyVillages(Grid &w, const Terrain &terrain, int cx, int cz) {
  auto set = [&](int x, int y, int z, int t) { w.set(x, y, z, t); };
  for (auto v : terrain.villagesNear(cx * 16 + 8, cz * 16 + 8, 12)) {
    int sy = terrain.groundHeight(v.x, v.z);
    for (int x = cx * 16; x < cx * 16 + 16; x++)
      for (int z = cz * 16; z < cz * 16 + 16; z++) {
        bool road = (std::abs(x - v.x) <= 1 && std::abs(z - v.z) <= 28) ||
                    (std::abs(z - v.z) <= 1 && std::abs(x - v.x) <= 28);
        for (auto h : v.homes()) {
          int dx = h[0] + 3, dz = h[1] + 7;
          if (std::abs(x - dx) <= 1 && z >= std::min(dz, v.z) &&
              z <= std::max(dz, v.z))
            road = true;
        }
        if (road) {
          int y = terrain.groundHeight(x, z);
          set(x, y, z, blockId("dirt_path"));
          for (int h = y + 1; h < y + 5; h++)
            set(x, h, z, AIR);
        }
        if (std::abs(x - v.x) <= 3 && std::abs(z - v.z) <= 3) {
          for (int y = terrain.groundHeight(x, z); y <= sy; y++)
            set(x, y, z, BRICK);
          for (int h = sy + 1; h < sy + 6; h++)
            set(x, h, z, AIR);
        }
      }
    for (auto home : v.homes()) {
      int x = home[0], z = home[1], job = home[2],
          y = terrain.groundHeight(x + 3, z + 3) + 1;
      if (x + 8 < cx * 16 || x - 1 > cx * 16 + 15 || z + 8 < cz * 16 ||
          z - 1 > cz * 16 + 15)
        continue;
      int wall = v.biome == 1 ? blockId("sandstone") : PLANK;
      for (int a = 0; a < 7; a++)
        for (int b = 0; b < 7; b++) {
          for (int h = terrain.groundHeight(x + a, z + b); h < y; h++)
            set(x + a, h, z + b, BRICK);
          set(x + a, y - 1, z + b, PLANK);
          for (int h = 0; h < 5; h++) {
            int t = a == 0 || a == 6 || b == 0 || b == 6 ? wall : AIR;
            if ((a == 0 || a == 6) && (b == 0 || b == 6))
              t = WOOD;
            if (b == 6 && a == 3 && h < 3)
              t = AIR;
            if ((a == 0 || a == 6) && b >= 2 && b <= 3 && h == 2)
              t = GLASS;
            set(x + a, y + h, z + b, t);
          }
        }
      for (int h = 0; h < 4; h++)
        for (int a = -1 + h; a <= 7 - h; a++)
          for (int b = -1; b <= 7; b++)
            set(x + a, y + 5 + h, z + b, v.biome == 2 ? SNOW : ROOF);
      set(x + 2, y + 1, z + 7, LANTERN);
      set(x + 1, y, z + 1,
          blockId(job == 1   ? "smithing_table"
                  : job == 2 ? "bookshelf"
                             : "composter"));
      set(x + 5, y, z + 1, blockId("chest"));
    }
    int y = sy + 1;
    for (int dx = -1; dx <= 1; dx++)
      for (int dz = -1; dz <= 1; dz++) {
        set(v.x + dx, y, v.z + dz, dx == 0 && dz == 0 ? WATER : BRICK);
        set(v.x + dx, y - 1, v.z + dz, BRICK);
      }
    for (int dx : {-2, 2})
      for (int dz : {-2, 2}) {
        for (int h = 0; h < 4; h++)
          set(v.x + dx, y + h, v.z + dz, WOOD);
        set(v.x + dx, y + 4, v.z + dz, LANTERN);
      }
    for (int x = v.x + 7; x < v.x + 14; x++)
      for (int z = v.z - 5; z < v.z + 3; z++) {
        int fy = terrain.groundHeight(x, z);
        set(x, fy, z, z == v.z - 1 ? WATER : blockId("farmland"));
        set(x, fy + 1, z, z == v.z - 1 ? AIR : blockId("wheat"));
      }
  }
}
} // namespace terrain_internal
} // namespace zz::horizonte
