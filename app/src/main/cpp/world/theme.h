// Biomas/ambientações: cada zona de 5 fases tem paleta, árvores e céu próprios.
#pragma once

#include "../core/math.h"
#include "blocks.h"

namespace zz {

enum class TreeKind { Oak, Spruce, Cactus, Dead, Birch, Cherry, Maple, Ipe, Jacaranda, Palm };

struct SkyPreset {
    Vec3 zenith;
    Vec3 horizon;
    Vec3 fog;
    Vec3 sunColor;
    Vec3 ambient;
    Vec3 sunDirection;  // direção PARA o sol
    float sunIntensity;
    float fogStart;
    float fogEnd;
    float night;  // 0 = dia, 1 = noite (estrelas, lanterna do jogador)
};

struct Theme {
    int id;
    const char* name;
    Block surface;
    Block filler;
    Block beach;
    Block deep;
    TreeKind tree;
    float treeDensity;
    float plantDensity;
    Block plantA;
    Block plantB;
    Vec3 grassTint;   // multiplicador da grama/folhas (texturas procedurais)
    Vec3 waterTint;
    float hilliness;  // amplitude do relevo
    SkyPreset sky;
};

inline Theme themeForStage(int stage) {
    int zone = ((stage < 1 ? 1 : stage) - 1) / 5;
    switch (zone % 4) {
        case 0:
            return Theme{0, "Floresta", B_GRASS, B_DIRT, B_SAND, B_STONE, TreeKind::Oak, 0.020f, 0.10f,
                         B_FLOWER_RED, B_TALL_GRASS, {0.55f, 0.85f, 0.35f}, {0.25f, 0.45f, 0.85f}, 1.0f,
                         SkyPreset{{0.32f, 0.55f, 0.95f}, {0.72f, 0.85f, 1.0f}, {0.74f, 0.85f, 0.98f},
                                   {1.0f, 0.96f, 0.86f}, {0.52f, 0.56f, 0.64f}, normalize(Vec3(0.45f, 0.8f, 0.3f)),
                                   0.85f, 60.0f, 150.0f, 0.0f}};
        case 1:
            return Theme{1, "Deserto", B_SAND, B_SANDSTONE, B_SAND, B_SANDSTONE, TreeKind::Cactus, 0.006f, 0.03f,
                         B_DEAD_BUSH, B_DEAD_BUSH, {0.75f, 0.78f, 0.4f}, {0.2f, 0.55f, 0.75f}, 0.7f,
                         SkyPreset{{0.36f, 0.42f, 0.78f}, {1.0f, 0.68f, 0.42f}, {0.98f, 0.72f, 0.52f},
                                   {1.0f, 0.78f, 0.5f}, {0.56f, 0.46f, 0.44f}, normalize(Vec3(-0.6f, 0.32f, 0.4f)),
                                   0.85f, 55.0f, 140.0f, 0.0f}};
        case 2:
            return Theme{2, "Neve", B_SNOW_GRASS, B_DIRT, B_GRAVEL, B_STONE, TreeKind::Spruce, 0.016f, 0.02f,
                         B_TALL_GRASS, B_TALL_GRASS, {0.45f, 0.7f, 0.55f}, {0.3f, 0.5f, 0.8f}, 1.25f,
                         SkyPreset{{0.42f, 0.52f, 0.7f}, {0.78f, 0.84f, 0.92f}, {0.8f, 0.85f, 0.92f},
                                   {0.95f, 0.95f, 1.0f}, {0.56f, 0.6f, 0.68f}, normalize(Vec3(0.3f, 0.6f, -0.5f)),
                                   0.7f, 40.0f, 115.0f, 0.15f}};
        default:
            return Theme{3, "Terra Devastada", B_ASH, B_NETHERRACK, B_GRAVEL, B_NETHERRACK, TreeKind::Dead, 0.010f,
                         0.04f, B_MUSHROOM_RED, B_DEAD_BUSH, {0.55f, 0.5f, 0.35f}, {0.45f, 0.2f, 0.25f}, 1.1f,
                         SkyPreset{{0.05f, 0.04f, 0.12f}, {0.32f, 0.12f, 0.16f}, {0.2f, 0.08f, 0.1f},
                                   {0.85f, 0.55f, 0.6f}, {0.3f, 0.24f, 0.32f}, normalize(Vec3(0.2f, 0.5f, 0.6f)),
                                   0.35f, 30.0f, 95.0f, 1.0f}};
    }
}

// Cenário do lobby: clareira de floresta ao entardecer.
inline Theme lobbyTheme() {
    Theme t = themeForStage(1);
    t.sky.zenith = {0.24f, 0.32f, 0.62f};
    t.sky.horizon = {0.98f, 0.62f, 0.46f};
    t.sky.fog = {0.82f, 0.6f, 0.55f};
    t.sky.sunColor = {1.0f, 0.82f, 0.62f};
    t.sky.sunDirection = normalize(Vec3(-0.7f, 0.28f, -0.5f));
    t.sky.fogStart = 30.0f;
    t.sky.fogEnd = 80.0f;
    return t;
}

}  // namespace zz
