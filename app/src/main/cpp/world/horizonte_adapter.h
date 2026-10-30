#pragma once
#include "horizonte/terrain.h"
#include "world.h"

namespace zz::horizonte {
constexpr int kHeight = 128;
constexpr int kSea = 16;

double riverDistance(uint32_t seed, int x, int z);
bool clearFootprint(const Terrain& terrain, uint32_t seed, int x, int z, float radius);
void generate(const Terrain& terrain, Chunk& chunk, int height);
uint8_t surface(const Terrain& terrain, int x, int z);
}
