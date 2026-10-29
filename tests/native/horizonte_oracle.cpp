// Regenera fixtures diretamente do Horizonte não modificado. Compilar com
// seus quatro módulos, -ffp-contract=off e -I<projeto-horizonte>/app/src/main/cpp/horizonte.
#include "terrain.h"
#include "terrain_noise.h"
#include <cstdio>
#include <cstdint>
#include <string>
namespace hz {
uint16_t blockId(std::string name) {
    const char* names[] = {"diamond_ore", "sandstone", "dirt_path", "smithing_table",
        "bookshelf", "composter", "chest", "farmland", "wheat"};
    for (uint16_t i = 0; i < 9; ++i) if (name == names[i]) return 20 + i;
    return 0;
}
}
int main() {
    for (int64_t seed : {0LL, 1LL, 42LL, 424242LL, 4294967295LL}) {
        hz::Terrain t(seed);
        for (auto p : {std::pair<int,int>{0,0}, {3,4}, {7,7}, {-1,-1}, {-9,8}, {4,11}, {191,192}, {383,383}}) {
            auto blocks = t.chunk(p.first,p.second);
            uint64_t h = 14695981039346656037ULL;
            for (uint16_t b : blocks) { h ^= b & 255; h *= 1099511628211ULL; h ^= b >> 8; h *= 1099511628211ULL; }
            std::printf("{%lld, %d, %d, %lluULL, %d, %d},\n", (long long)seed, p.first,p.second,
                (unsigned long long)h, t.groundHeight(p.first*16,p.second*16),t.biome(p.first*16,p.second*16));
        }
    }
}
