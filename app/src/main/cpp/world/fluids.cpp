// Algoritmo de água portado de Horizonte/fluid_physics.cpp.
#include "fluids.h"
#include "fluid_catalogue.h"
#include <algorithm>
#include <cmath>
namespace zz::fluids {
static const int sides[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
void FluidSimulation::enqueue(int x, int y, int z) {
  Pos p{x, y, z};
  if (y >= 0 && y < 128 && queue.size() < 16384 && queued.insert(p).second)
    queue.push_back(p);
}
void FluidSimulation::changed(int x, int y, int z) {
  enqueue(x, y, z);
  enqueue(x, y - 1, z);
  enqueue(x, y + 1, z);
  for (auto &d : sides)
    enqueue(x + d[0], y, z + d[1]);
}
void FluidSimulation::update(const WorldRead &get, const WorldWrite &write,
                             float dt, float px, float py, float pz) {
  auto set = [&](int x, int y, int z, uint16_t id) {
    if (get(x, y, z) != id) {
      write(x, y, z, id);
      changed(x, y, z);
    }
  };
  auto flow = [](int kind, int level) {
    for (int id = 0; id < blockCount(); id++) {
      auto &b = block(id);
      if (b.fluidKind == kind && b.fluidLevel == level)
        return uint16_t(id);
    }
    return blockId("air");
  };
  auto replace = [](const Block &b) {
    return b.name == "air" || (b.flags & 128) != 0;
  };
  scanClock -= dt;
  if (scanClock <= 0) {
    scanClock = 2;
    for (int x = int(px) - 10; x <= int(px) + 10; x++)
      for (int z = int(pz) - 10; z <= int(pz) + 10; z++)
        for (int y = std::max(1, int(py) - 6); y <= std::min(126, int(py) + 6);
             y++) {
          auto &b = block(get(x, y, z));
          if (b.fluidKind || (b.flags & 8))
            enqueue(x, y, z);
        }
  }
  clock += dt;
  if (clock < .25f)
    return;
  clock -= .25f;
  tick++;
  int count = std::min(256, int(queue.size()));
  for (int i = 0; i < count && !queue.empty(); i++) {
    auto p = queue.front();
    queue.pop_front();
    queued.erase(p);
    int x, y, z;
    std::tie(x, y, z) = p;
    if (std::abs(x - px) > activeRadius || std::abs(z - pz) > activeRadius)
      continue;
    uint16_t id = get(x, y, z);
    Block b = block(id);
    if (b.flags & 8) {
      if (y > 0 && !block(get(x, y - 1, z)).solid) {
        set(x, y, z, blockId("air"));
        set(x, y - 1, z, id);
      }
      continue;
    }
    if (!b.fluidKind)
      continue;
    if (b.fluidKind == 2 && tick % 6) {
      enqueue(x, y, z);
      continue;
    }
    bool stop = false;
    for (auto &d : sides) {
      auto other = block(get(x + d[0], y, z + d[1]));
      if (other.fluidKind && other.fluidKind != b.fluidKind) {
        if (b.fluidKind == 2) {
          set(x, y, z, blockId(b.fluidLevel == 0 ? "obsidian" : "cobblestone"));
          stop = true;
          break;
        }
        set(x + d[0], y, z + d[1],
            blockId(other.fluidLevel == 0 ? "obsidian" : "cobblestone"));
      }
    }
    if (stop)
      continue;
    if (b.fluidLevel) {
      int desired = 9, sources = 0;
      for (auto &d : sides)
        if (get(x + d[0], y, z + d[1]) == blockId("water"))
          sources++;
      if (b.fluidKind == 1 && sources >= 2 &&
          (block(get(x, y - 1, z)).solid ||
           get(x, y - 1, z) == blockId("water"))) {
        set(x, y, z, blockId("water"));
        b = block(blockId("water"));
        desired = 0;
      } else if (block(get(x, y + 1, z)).fluidKind == b.fluidKind)
        desired = 8;
      else
        for (auto &d : sides) {
          auto adjacent = block(get(x + d[0], y, z + d[1]));
          if (adjacent.fluidKind == b.fluidKind)
            desired = std::min(
                desired, (adjacent.fluidLevel == 8 ? 0 : adjacent.fluidLevel) +
                             (b.fluidKind == 1 ? 1 : 2));
        }
      if (desired > 8 ||
          (desired == 8 && block(get(x, y + 1, z)).fluidKind != b.fluidKind)) {
        set(x, y, z, blockId("air"));
        continue;
      }
      if (desired != b.fluidLevel) {
        set(x, y, z, flow(b.fluidKind, desired));
        b = block(get(x, y, z));
      }
    }
    if (y > 0) {
      auto below = block(get(x, y - 1, z));
      if (below.fluidKind && below.fluidKind != b.fluidKind) {
        set(x, y - 1, z,
            blockId(b.fluidKind == 2        ? "stone"
                    : below.fluidLevel == 0 ? "obsidian"
                                            : "cobblestone"));
        continue;
      }
      if (replace(below)) {
        set(x, y - 1, z, flow(b.fluidKind, 8));
        continue;
      }
      if (below.fluidKind == b.fluidKind)
        continue;
    }
    int level =
        (b.fluidLevel == 8 ? 0 : b.fluidLevel) + (b.fluidKind == 1 ? 1 : 2);
    if (level > 7)
      continue;
    for (auto &d : sides) {
      auto adjacent = block(get(x + d[0], y, z + d[1]));
      if (replace(adjacent) ||
          (adjacent.fluidKind == b.fluidKind && adjacent.fluidLevel > level &&
           adjacent.fluidLevel != 8))
        set(x + d[0], y, z + d[1], flow(b.fluidKind, level));
    }
  }
}
} // namespace hz
