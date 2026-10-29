// Tipos de bloco do mundo estilo Minecraft e camadas de textura procedural.
#pragma once

#include <cstdint>

namespace zz {

enum TexLayer : uint8_t {
    T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, T_STONE, T_COBBLE, T_SAND, T_SANDSTONE_SIDE, T_SANDSTONE_TOP,
    T_SNOW, T_SNOW_SIDE, T_ICE, T_WATER, T_LOG_OAK, T_LOG_TOP, T_LEAVES_OAK, T_LOG_SPRUCE,
    T_LEAVES_SPRUCE, T_LOG_DEAD, T_PLANKS, T_CRATE, T_CACTUS_SIDE, T_CACTUS_TOP, T_GRAVEL, T_PATH_TOP,
    T_PATH_SIDE, T_OBSIDIAN, T_PORTAL, T_NETHERRACK, T_ASH_TOP, T_ASH_SIDE, T_BEDROCK, T_LANTERN,
    T_MAGMA, T_TERRACOTTA, T_HAY_SIDE, T_HAY_TOP, T_COAL_ORE, T_IRON_ORE, T_CAMPFIRE_TOP, T_CAMPFIRE_SIDE,
    T_FLOWER_RED, T_FLOWER_YELLOW, T_TALL_GRASS, T_DEAD_BUSH, T_MUSHROOM_RED, T_DIAMOND, T_GOLD,
    // Árvores coloridas
    T_LOG_BIRCH, T_LOG_CHERRY, T_LOG_PALM, T_LOG_PALM_TOP, T_LEAVES_BIRCH, T_LEAVES_CHERRY, T_LEAVES_MAPLE,
    T_LEAVES_GOLD, T_LEAVES_PURPLE, T_LEAVES_PALM,
    // Construções
    T_BRICK, T_PLASTER, T_ROOF_RED, T_ROOF_SLATE, T_GLASS, T_CONCRETE, T_ASPHALT, T_STONE_BRICK, T_WOOL_RED,
    T_BOOKSHELF, T_PUMP,
    T_WATER_FLOW,
    T_COUNT
};

enum Block : uint8_t {
    B_AIR = 0, B_GRASS, B_DIRT, B_STONE, B_COBBLE, B_SAND, B_SANDSTONE, B_SNOW_GRASS, B_SNOW, B_ICE,
    B_WATER, B_LOG_OAK, B_LEAVES_OAK, B_LOG_SPRUCE, B_LEAVES_SPRUCE, B_LOG_DEAD, B_PLANKS, B_CRATE,
    B_CACTUS, B_GRAVEL, B_PATH, B_OBSIDIAN, B_PORTAL, B_NETHERRACK, B_ASH, B_BEDROCK, B_LANTERN,
    B_MAGMA, B_TERRACOTTA, B_HAY, B_COAL_ORE, B_IRON_ORE, B_CAMPFIRE, B_FLOWER_RED, B_FLOWER_YELLOW,
    B_TALL_GRASS, B_DEAD_BUSH, B_MUSHROOM_RED,
    // Árvores coloridas (bétula, cerejeira, bordo, ipê, jacarandá, coqueiro)
    B_LOG_BIRCH, B_LOG_CHERRY, B_LOG_PALM, B_LEAVES_BIRCH, B_LEAVES_CHERRY, B_LEAVES_MAPLE, B_LEAVES_GOLD,
    B_LEAVES_PURPLE, B_LEAVES_PALM,
    // Construções (casas, igreja, posto, prédios)
    B_BRICK, B_PLASTER, B_ROOF_RED, B_ROOF_SLATE, B_GLASS, B_CONCRETE, B_ASPHALT, B_STONE_BRICK, B_WOOL_RED,
    B_BOOKSHELF, B_PUMP,
    B_WATER_FLOW_1, B_WATER_FLOW_2, B_WATER_FLOW_3, B_WATER_FLOW_4,
    B_WATER_FLOW_5, B_WATER_FLOW_6, B_WATER_FLOW_7, B_WATER_FALLING,
    B_COUNT
};

enum class Shape : uint8_t { None, Cube, Cross, Liquid };

struct BlockInfo {
    Shape shape;
    bool solid;     // participa da colisão
    bool opaque;    // esconde faces vizinhas
    bool emissive;  // ignora a luz do ambiente (lanterna, magma, portal)
    uint8_t top, side, bottom;
    uint32_t mapColor;  // cor 0xRRGGBB do minimapa
};

inline const BlockInfo& blockInfo(uint8_t id) {
    static const BlockInfo kBlocks[B_COUNT] = {
        {Shape::None, false, false, false, 0, 0, 0, 0x000000},                                      // AIR
        {Shape::Cube, true, true, false, T_GRASS_TOP, T_GRASS_SIDE, T_DIRT, 0x5fa83a},              // GRASS
        {Shape::Cube, true, true, false, T_DIRT, T_DIRT, T_DIRT, 0x8a5a36},                         // DIRT
        {Shape::Cube, true, true, false, T_STONE, T_STONE, T_STONE, 0x8a8a8a},                      // STONE
        {Shape::Cube, true, true, false, T_COBBLE, T_COBBLE, T_COBBLE, 0x6f6f6f},                   // COBBLE
        {Shape::Cube, true, true, false, T_SAND, T_SAND, T_SAND, 0xe3d29a},                         // SAND
        {Shape::Cube, true, true, false, T_SANDSTONE_TOP, T_SANDSTONE_SIDE, T_SANDSTONE_TOP, 0xd8c486},
        {Shape::Cube, true, true, false, T_SNOW, T_SNOW_SIDE, T_DIRT, 0xf0f6fa},                    // SNOW_GRASS
        {Shape::Cube, true, true, false, T_SNOW, T_SNOW, T_SNOW, 0xf4f8fb},                         // SNOW
        {Shape::Cube, true, true, false, T_ICE, T_ICE, T_ICE, 0x9cc4f2},                            // ICE
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},                  // WATER
        {Shape::Cube, true, true, false, T_LOG_TOP, T_LOG_OAK, T_LOG_TOP, 0x6b4e2e},                // LOG_OAK
        {Shape::Cube, true, true, false, T_LEAVES_OAK, T_LEAVES_OAK, T_LEAVES_OAK, 0x3f8a2a},       // LEAVES_OAK
        {Shape::Cube, true, true, false, T_LOG_TOP, T_LOG_SPRUCE, T_LOG_TOP, 0x4a3420},             // LOG_SPRUCE
        {Shape::Cube, true, true, false, T_LEAVES_SPRUCE, T_LEAVES_SPRUCE, T_LEAVES_SPRUCE, 0x2f5e3a},
        {Shape::Cube, true, true, false, T_LOG_TOP, T_LOG_DEAD, T_LOG_TOP, 0x5a5048},               // LOG_DEAD
        {Shape::Cube, true, true, false, T_PLANKS, T_PLANKS, T_PLANKS, 0xb48a52},                   // PLANKS
        {Shape::Cube, true, true, false, T_CRATE, T_CRATE, T_CRATE, 0x9c7040},                      // CRATE
        {Shape::Cube, true, true, false, T_CACTUS_TOP, T_CACTUS_SIDE, T_CACTUS_TOP, 0x3e8a35},      // CACTUS
        {Shape::Cube, true, true, false, T_GRAVEL, T_GRAVEL, T_GRAVEL, 0x8c8480},                   // GRAVEL
        {Shape::Cube, true, true, false, T_PATH_TOP, T_PATH_SIDE, T_DIRT, 0xa9874f},                // PATH
        {Shape::Cube, true, true, false, T_OBSIDIAN, T_OBSIDIAN, T_OBSIDIAN, 0x241a33},             // OBSIDIAN
        {Shape::None, false, false, true, T_PORTAL, T_PORTAL, T_PORTAL, 0x9a4cff},                  // PORTAL
        {Shape::Cube, true, true, false, T_NETHERRACK, T_NETHERRACK, T_NETHERRACK, 0x7a3434},       // NETHERRACK
        {Shape::Cube, true, true, false, T_ASH_TOP, T_ASH_SIDE, T_DIRT, 0x6b6052},                  // ASH
        {Shape::Cube, true, true, false, T_BEDROCK, T_BEDROCK, T_BEDROCK, 0x333333},                // BEDROCK
        {Shape::Cube, true, true, true, T_LANTERN, T_LANTERN, T_LANTERN, 0xffd27a},                 // LANTERN
        {Shape::Cube, true, true, true, T_MAGMA, T_MAGMA, T_MAGMA, 0xd2541c},                       // MAGMA
        {Shape::Cube, true, true, false, T_TERRACOTTA, T_TERRACOTTA, T_TERRACOTTA, 0xb5643f},       // TERRACOTTA
        {Shape::Cube, true, true, false, T_HAY_TOP, T_HAY_SIDE, T_HAY_TOP, 0xd6b23c},               // HAY
        {Shape::Cube, true, true, false, T_COAL_ORE, T_COAL_ORE, T_COAL_ORE, 0x777777},             // COAL_ORE
        {Shape::Cube, true, true, false, T_IRON_ORE, T_IRON_ORE, T_IRON_ORE, 0x8d8580},             // IRON_ORE
        {Shape::Cube, true, true, true, T_CAMPFIRE_TOP, T_CAMPFIRE_SIDE, T_LOG_TOP, 0xff8a2a},      // CAMPFIRE
        {Shape::Cross, false, false, false, T_FLOWER_RED, T_FLOWER_RED, T_FLOWER_RED, 0x5fa83a},
        {Shape::Cross, false, false, false, T_FLOWER_YELLOW, T_FLOWER_YELLOW, T_FLOWER_YELLOW, 0x5fa83a},
        {Shape::Cross, false, false, false, T_TALL_GRASS, T_TALL_GRASS, T_TALL_GRASS, 0x5fa83a},
        {Shape::Cross, false, false, false, T_DEAD_BUSH, T_DEAD_BUSH, T_DEAD_BUSH, 0xd8c486},
        {Shape::Cross, false, false, false, T_MUSHROOM_RED, T_MUSHROOM_RED, T_MUSHROOM_RED, 0x6b6052},
        {Shape::Cube, true, true, false, T_LOG_TOP, T_LOG_BIRCH, T_LOG_TOP, 0xd8d4c8},              // LOG_BIRCH
        {Shape::Cube, true, true, false, T_LOG_TOP, T_LOG_CHERRY, T_LOG_TOP, 0x5a3530},             // LOG_CHERRY
        {Shape::Cube, true, true, false, T_LOG_PALM_TOP, T_LOG_PALM, T_LOG_PALM_TOP, 0x9a7a52},     // LOG_PALM
        {Shape::Cube, true, true, false, T_LEAVES_BIRCH, T_LEAVES_BIRCH, T_LEAVES_BIRCH, 0x7fb04a},  // LEAVES_BIRCH
        {Shape::Cube, true, true, false, T_LEAVES_CHERRY, T_LEAVES_CHERRY, T_LEAVES_CHERRY, 0xf2a6c8},
        {Shape::Cube, true, true, false, T_LEAVES_MAPLE, T_LEAVES_MAPLE, T_LEAVES_MAPLE, 0xd8562a},  // LEAVES_MAPLE
        {Shape::Cube, true, true, false, T_LEAVES_GOLD, T_LEAVES_GOLD, T_LEAVES_GOLD, 0xf2c830},     // LEAVES_GOLD (ipê)
        {Shape::Cube, true, true, false, T_LEAVES_PURPLE, T_LEAVES_PURPLE, T_LEAVES_PURPLE, 0x9a6ad8},
        {Shape::Cube, true, true, false, T_LEAVES_PALM, T_LEAVES_PALM, T_LEAVES_PALM, 0x4f9a36},     // LEAVES_PALM
        {Shape::Cube, true, true, false, T_BRICK, T_BRICK, T_BRICK, 0xa0523d},                      // BRICK
        {Shape::Cube, true, true, false, T_PLASTER, T_PLASTER, T_PLASTER, 0xe8e0cc},                // PLASTER
        {Shape::Cube, true, true, false, T_ROOF_RED, T_ROOF_RED, T_ROOF_RED, 0xb4442e},             // ROOF_RED
        {Shape::Cube, true, true, false, T_ROOF_SLATE, T_ROOF_SLATE, T_ROOF_SLATE, 0x4a5262},       // ROOF_SLATE
        {Shape::Cube, true, false, false, T_GLASS, T_GLASS, T_GLASS, 0xa8d8f0},                     // GLASS (recortado)
        {Shape::Cube, true, true, false, T_CONCRETE, T_CONCRETE, T_CONCRETE, 0xa6a6a2},             // CONCRETE
        {Shape::Cube, true, true, false, T_ASPHALT, T_ASPHALT, T_ASPHALT, 0x3c3c40},                // ASPHALT
        {Shape::Cube, true, true, false, T_STONE_BRICK, T_STONE_BRICK, T_STONE_BRICK, 0x7c7c80},    // STONE_BRICK
        {Shape::Cube, true, true, false, T_WOOL_RED, T_WOOL_RED, T_WOOL_RED, 0xa02a2a},             // WOOL_RED
        {Shape::Cube, true, true, false, T_PLANKS, T_BOOKSHELF, T_PLANKS, 0x8a6038},                // BOOKSHELF
        {Shape::Cube, true, true, false, T_CONCRETE, T_PUMP, T_CONCRETE, 0xd23c2c},                // PUMP (bomba de combustível)
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
        {Shape::Liquid, false, false, false, T_WATER, T_WATER_FLOW, T_WATER, 0x429be6},
    };
    return kBlocks[id < B_COUNT ? id : 0];
}

inline int waterLevel(uint8_t b) {
    return b == B_WATER ? 0 : b >= B_WATER_FLOW_1 && b <= B_WATER_FALLING ? 1 + b - B_WATER_FLOW_1 : -1;
}
inline bool isWaterBlock(uint8_t b) { return waterLevel(b) >= 0; }
inline float waterHeight(uint8_t b, bool above = false) {
    int level = waterLevel(b);
    if (level < 0) return 0;
    if (above || level == 8) return 1;
    return 1.0f - (level > 0 ? level : 1) / 8.0f;
}

inline bool isSolid(uint8_t id) { return blockInfo(id).solid; }
inline bool isLeafBlock(uint8_t b) {
    return b == B_LEAVES_OAK || b == B_LEAVES_SPRUCE || (b >= B_LEAVES_BIRCH && b <= B_LEAVES_PALM);
}
// Copas coloridas que soltam pétalas e folhas no vento.
inline bool isBlossomBlock(uint8_t b) {
    return b == B_LEAVES_CHERRY || b == B_LEAVES_MAPLE || b == B_LEAVES_GOLD || b == B_LEAVES_PURPLE;
}
inline bool isOpaque(uint8_t id) { return blockInfo(id).opaque; }

}  // namespace zz
