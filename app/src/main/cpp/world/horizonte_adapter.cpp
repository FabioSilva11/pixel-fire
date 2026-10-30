#include "horizonte_adapter.h"
#include "horizonte/terrain_noise.h"
#include "horizonte/block_mapping.h"
#include <algorithm>
#include <cmath>

namespace zz::horizonte {
double riverDistance(uint32_t seed, int x, int z) {
    double river = 64 + 16 * std::sin(z * .046) +
                   8 * (terrain_internal::noise(seed, z * .04, 2) - .5);
    return std::abs(x - river);
}

bool clearFootprint(const Terrain& terrain, uint32_t seed, int x, int z, float radius) {
    // Aldeias têm casas, campos e caminhos até 45 blocos do centro.
    if (!terrain.villagesNear(x, z, int(std::ceil(radius))).empty()) return false;
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
