// Estado do autopiloto de medição de desempenho (game_perf.cpp).
#pragma once

#include <cstddef>
#include <vector>

#include "../core/math.h"

namespace zz::perf {

struct AutopilotState {
    bool active = false;
    int mode = 0;                   // 1 sem inimigos, 2 com inimigos
    int frame = 0, frames = 7200;   // 2 min simulados a 60 fps
    size_t leg = 0;
    std::vector<Vec2> route;        // pontos (x, z)
    Vec3 lastPos;
    int starved = 0;                // quadros em que algum chunk do 3x3 em volta do jogador não existia
    char label[16] = "auto";
};

}  // namespace zz::perf
