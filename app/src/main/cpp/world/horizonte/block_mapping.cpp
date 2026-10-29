#include "block_mapping.h"
#include "terrain.h"
#include "../blocks.h"
#include <array>
#include <string>

namespace zz::horizonte {
namespace {
// Os primeiros vinte IDs pertencem ao gerador original. Os demais são os
// nomes consultados por chunk_generator/village_generator, sem importar o catálogo.
constexpr std::array<uint8_t, 29> blocks = {
    B_AIR, B_GRASS, B_DIRT, B_STONE, B_LOG_OAK, B_LEAVES_OAK,
    B_PLANKS, B_BRICK, B_WATER, B_SAND, B_SNOW, B_OBSIDIAN,
    B_IRON_ORE, B_LANTERN, B_ROOF_RED, B_WOOL_RED, B_GLASS,
    B_WATER, B_WATER, B_MAGMA, B_IRON_ORE, B_SANDSTONE, B_PATH,
    B_STONE_BRICK, B_BOOKSHELF, B_CRATE, B_CRATE, B_DIRT, B_TALL_GRASS
};
}

uint16_t blockId(std::string name) {
    static const char* const names[] = {"diamond_ore", "sandstone", "dirt_path",
        "smithing_table", "bookshelf", "composter", "chest", "farmland", "wheat"};
    for (uint16_t i = 0; i < 9; ++i)
        if (name == names[i]) return 20 + i;
    return 0;
}

uint8_t toBlock(uint16_t id) {
    return id < blocks.size() ? blocks[id] : B_AIR;
}
}  // namespace zz::horizonte
