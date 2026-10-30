// Personagens de blocos estilo Minecraft gerados em código: corpo de caixas (cabeça, tronco, braços e
// pernas), pele 64x64 em pixel art pintada a partir da aparência salva e animações
// (parado, andar, correr, morrer, gestos).
#pragma once

#include <cstdint>

namespace zz {

struct CharacterAsset;

// Aparência codificada em 32 bits (espelha Look.kt).
struct Look {
    int skin = 1;        // 0..7 tom de pele
    int hairStyle = 1;   // 0 careca, 1 curto, 2 longo, 3 moicano, 4 rabo, 5 black power, 6 franja, 7 coque
    int hairColor = 0;   // 0..7
    int eyes = 0;        // 0..3
    int face = 0;        // 0 nada, 1 barba, 2 bigode, 3 cicatriz
    int top = 0;         // 0 camiseta, 1 regata, 2 jaqueta, 3 moletom, 4 xadrez, 5 colete tático, 6 armadura, 7 jaleco
    int topColor = 6;    // 0..15
    int pantsColor = 0;  // 0..7 (7 = camuflado)
    int shoes = 0;       // 0..3
    int hat = 0;         // 0 nenhum, 1 boné, 2 capacete, 3 gorro, 4 faixa, 5 óculos, 6 fone, 7 coroa
    bool shorts = false;

    static Look decode(uint32_t code);
    uint32_t encode() const;
};


// Escala do pixel do personagem em unidades do modelo (altura total = 32 pixels).
constexpr float kBlockyPixel = 0.085f;

// Monta modelo, malhas, animações e textura do personagem.
void buildBlockyCharacter(uint32_t code, CharacterAsset& out);


}  // namespace zz
