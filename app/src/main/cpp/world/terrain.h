// Gerador do cenário finito do lobby (as fases usam a ilha em streaming, island.h).
#pragma once

#include <cstdint>
#include <vector>

#include "../core/math.h"
#include "structures.h"
#include "theme.h"
#include "world.h"

namespace zz {

struct WorldLayout {
    Vec3 start;
    float startYaw = 0;
    std::vector<Vec3> lights;       // lanternas e fogueiras (luz pontual)
    std::vector<Vec3> campfires;    // partículas de fogo
    int seaLevel = 13;
};

WorldLayout generateLobbyWorld(World& world, const Theme& theme);

// Variação de céu/horário a partir da semente (manhã dourada, nublado, noite...).
Theme applySkyVariant(const Theme& base, uint32_t seed, bool allowNight);

}  // namespace zz
