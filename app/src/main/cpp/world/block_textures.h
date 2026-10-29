#pragma once

#include <cstdint>
#include <vector>

#include "theme.h"

namespace zz {

constexpr int kTexSize = 16;

// Gera T_COUNT camadas RGBA 16x16 em pixel art procedural, coloridas pelo bioma.
std::vector<uint8_t> generateBlockTextures(const Theme& theme);

// Cor média de uma camada (para partículas de impacto em blocos).
Vec3 averageLayerColor(const std::vector<uint8_t>& layers, int layer);

}  // namespace zz
