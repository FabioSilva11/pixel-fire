#include "horizonte_adapter.h"
#include "horizonte/terrain_noise.h"
#include "horizonte/block_mapping.h"
#include <algorithm>
#include <cmath>

namespace zz::horizonte {
namespace {
struct FloatingIsland { int x, z, radius, cap; };
FloatingIsland floatingIsland(uint32_t seed, int cx, int cz) {
    using terrain_internal::noise;
    uint64_t s = uint64_t(seed) + 1919;
    return {cx * 128 + 32 + int(noise(s, cx * 3.1, cz * 3.1) * 64),
            cz * 128 + 32 + int(noise(s, cx * 3.1 + 18, cz * 3.1) * 64),
            15 + int(noise(s, cx * 3.1, cz * 3.1 + 19) * 12),
            61 + int(noise(s, cx * 3.1 + 31, cz * 3.1 + 31) * 22)};
}
}

FloatingColumn floatingColumn(uint32_t seed, int x, int z) {
    auto i = floatingIsland(seed, terrain_internal::fd(x, 128), terrain_internal::fd(z, 128));
    double d = std::hypot(x - i.x, z - i.z) / i.radius;
    if (d >= 1) return {};
    return {i.cap - int((1 - d * .65) * i.radius), i.cap};
}

double riverDistance(uint32_t seed, int x, int z) {
    double river = 64 + 16 * std::sin(z * .046) +
                   8 * (terrain_internal::noise(seed, z * .04, 2) - .5);
    return std::abs(x - river);
}

bool clearFootprint(const Terrain& terrain, uint32_t seed, int x, int z, float radius) {
    // Aldeias têm casas, campos e caminhos até 45 blocos do centro.
    if (!terrain.villagesNear(x, z, int(std::ceil(radius))).empty()) return false;
    int r = int(std::ceil(radius));
    for (int cz = terrain_internal::fd(z-r,128); cz <= terrain_internal::fd(z+r,128); ++cz)
        for (int cx = terrain_internal::fd(x-r,128); cx <= terrain_internal::fd(x+r,128); ++cx) {
            auto i = floatingIsland(seed,cx,cz);
            if (std::hypot(x-i.x,z-i.z) < i.radius + radius) return false;
        }
    return true;
}

uint8_t surface(const Terrain& terrain, int x, int z) {
    int h = terrain.groundHeight(x,z), b = terrain.biome(x,z);
    return h <= 17 || b == 1 ? B_SAND : b == 2 ? B_SNOW : B_GRASS;
}

void generate(const Terrain& terrain, Chunk& chunk, int height) {
    auto raw = terrain.chunk(chunk.cx, chunk.cz);
    const int h = std::min(height,kHeight);
    chunk.blocks.assign(size_t(256) * height, B_AIR);
    chunk.maxY = 0;
    for (size_t i = 0; i < size_t(256) * h; ++i) {
        uint8_t block = toBlock(raw[i]);
        chunk.blocks[i] = block;
        if (block != B_AIR) chunk.maxY = int(i / 256) + 1;
    }
    for (int z=0; z<16; ++z)
        for (int x=0; x<16; ++x)
            chunk.ground[z*16+x] = int16_t(terrain.groundHeight(chunk.cx*16+x,chunk.cz*16+z)+1);
    // Origem da luz dos postes da aldeia, sem depender da ordem dos chunks.
    chunk.lights.clear();
    chunk.campfires.clear();
    for (int y=0; y<h; ++y)
        for (int z=0; z<16; ++z)
            for (int x=0; x<16; ++x)
                if (chunk.blocks[(y*16+z)*16+x] == B_LANTERN)
                    chunk.lights.push_back({chunk.cx*16+x+.5f,y+.5f,chunk.cz*16+z+.5f});
}
}  // namespace zz::horizonte
