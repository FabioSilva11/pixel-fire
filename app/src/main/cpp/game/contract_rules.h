// Regras puras dos contratos de expedição (sem dependência do motor).
// Fica em header para rodar no autoteste de depuração (selfTest) sem compilador C++ de host.
#pragma once

#include <algorithm>
#include <cmath>

namespace zz::contractrules {

struct ReloadResult {
    int ammo;        // balas no pente depois da recarga
    int reservePct;  // reserva restante, em centésimos de pente (< 0 = ilimitada)
};

// reservePct < 0 = reserva ilimitada (comportamento antigo do jogo).
inline bool canReload(int current, int magazine, int reservePct) { return current < magazine && reservePct != 0; }

inline ReloadResult reload(int current, int magazine, int reservePct) {
    if (reservePct < 0) return {magazine, reservePct};
    const int missing = std::max(0, magazine - current);
    if (missing == 0 || reservePct == 0 || magazine <= 0) return {current, reservePct};
    const int needPct = (missing * 100 + magazine - 1) / magazine;  // arredonda para cima
    if (reservePct >= needPct) return {magazine, reservePct - needPct};
    const int added = reservePct * magazine / 100;
    if (added <= 0) return {current, 0};  // "poeira" (p.ex. 3% de 5 balas): zera a reserva
    return {current + added, 0};
}

// Balas de reserva para a arma em mãos (arredonda para cima); -1 = ilimitada.
inline int reserveRounds(int reservePct, int magazine) { return reservePct < 0 ? -1 : (reservePct * magazine + 99) / 100; }

// Guardas de um ponto de interesse depois do multiplicador do contrato (nunca menos de 1).
inline int scaledGuards(int guards, float mul) { return std::max(1, static_cast<int>(std::lround(guards * mul))); }

// ---------------------------------------------------------------------------------------------- Caixa perdida (4D)
// Números do contrato. O Kotlin (Contracts.CARGO_*) repete estes valores nos textos; um teste de paridade lê este arquivo.
constexpr float kCargoMinStartM = 450.0f;    // distância mínima da largada
constexpr float kCargoMaxStartM = 1400.0f;   // distância máxima da largada
constexpr float kCargoNestClearM = 250.0f;   // folga mínima de ninhos e do portal
constexpr float kCargoPickupM = 1.8f;        // raio de coleta por proximidade
// Plano B (ilha em que nenhum ponto cabe na janela principal): janela mais larga, sempre determinística.
constexpr float kCargoRelaxMinStartM = 250.0f;
constexpr float kCargoRelaxMaxStartM = 2200.0f;
constexpr float kCargoRelaxNestClearM = 120.0f;

// tier 0 = janela principal, 1 = plano B.
inline bool cargoSiteOk(float distStart, float distObjective, int tier) {
    const float lo = tier == 0 ? kCargoMinStartM : kCargoRelaxMinStartM;
    const float hi = tier == 0 ? kCargoMaxStartM : kCargoRelaxMaxStartM;
    const float clear = tier == 0 ? kCargoNestClearM : kCargoRelaxNestClearM;
    return distStart >= lo && distStart <= hi && distObjective > clear;
}

// Mistura de bits (mesma semente => mesma caixa). Não usa o Rng do jogo: o resto da partida não muda.
inline unsigned cargoHash(unsigned seed) {
    unsigned x = seed ^ 0xCA46E5u;
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Índice inicial entre `count` candidatos (a busca segue em círculo até achar um com baú).
inline int cargoStartIndex(unsigned seed, int count) { return count <= 0 ? 0 : static_cast<int>(cargoHash(seed) % static_cast<unsigned>(count)); }

// Dica de distância enquanto a carga está oculta: arredonda para 100 m.
inline int cargoHintMeters(float dist) { return static_cast<int>(std::lround(dist / 100.0f)) * 100; }

// Autoteste (depuração): retorna quantas verificações falharam; `firstFailLine` recebe a linha da primeira falha.
inline int selfTest(int* firstFailLine) {
    int failed = 0;
    int firstLine = 0;
#define CT_CHECK(cond)                                \
    do {                                              \
        if (!(cond)) {                                \
            ++failed;                                 \
            if (firstLine == 0) firstLine = __LINE__; \
        }                                             \
    } while (0)

    CT_CHECK(reload(0, 30, -1).ammo == 30 && reload(0, 30, -1).reservePct == -1);      // ilimitada
    CT_CHECK(reload(0, 30, 320).ammo == 30 && reload(0, 30, 320).reservePct == 220);   // recarga completa custa 100
    CT_CHECK(reload(29, 30, 100).ammo == 30 && reload(29, 30, 100).reservePct == 96);  // 1 bala custa 4% (arredonda para cima)
    CT_CHECK(reload(10, 30, 50).ammo == 25 && reload(10, 30, 50).reservePct == 0);     // reserva parcial
    CT_CHECK(reload(0, 5, 3).ammo == 0 && reload(0, 5, 3).reservePct == 0);            // poeira
    CT_CHECK(reload(30, 30, 100).ammo == 30 && reload(30, 30, 100).reservePct == 100); // pente cheio não gasta
    CT_CHECK(!canReload(0, 30, 0) && !canReload(30, 30, 100) && canReload(0, 30, -1) && canReload(5, 30, 20));
    CT_CHECK(reserveRounds(220, 30) == 66 && reserveRounds(-1, 30) == -1 && reserveRounds(0, 30) == 0);
    CT_CHECK(scaledGuards(4, 0.5f) == 2 && scaledGuards(2, 0.5f) == 1 && scaledGuards(6, 1.0f) == 6 && scaledGuards(1, 0.5f) == 1);
    // Sequência de 3,2 pentes com arma de 30: 3 recargas completas, a quarta é parcial (6 balas) e zera.
    {
        int ammo = 0, reserve = 320, full = 0;
        for (int i = 0; i < 4; ++i) {
            const ReloadResult r = reload(ammo, 30, reserve);
            if (r.ammo == 30) ++full;
            ammo = 0;
            reserve = r.reservePct;
            if (i == 3) CT_CHECK(r.ammo == 6);
        }
        CT_CHECK(full == 3 && reserve == 0);
    }

    // Caixa perdida: janelas, determinismo e dica.
    CT_CHECK(cargoSiteOk(450.0f, 251.0f, 0) && cargoSiteOk(1400.0f, 400.0f, 0));
    CT_CHECK(!cargoSiteOk(449.0f, 900.0f, 0) && !cargoSiteOk(1401.0f, 900.0f, 0) && !cargoSiteOk(900.0f, 250.0f, 0));
    CT_CHECK(!cargoSiteOk(300.0f, 900.0f, 0) && cargoSiteOk(300.0f, 900.0f, 1) && cargoSiteOk(2000.0f, 130.0f, 1) && !cargoSiteOk(2300.0f, 900.0f, 1));
    {
        bool inRange = true, deterministic = true, varies = false;
        for (unsigned s = 1; s < 400; ++s) {
            const int a = cargoStartIndex(s, 17);
            inRange = inRange && a >= 0 && a < 17;
            deterministic = deterministic && a == cargoStartIndex(s, 17);
            if (a != cargoStartIndex(s + 1, 17)) varies = true;
        }
        CT_CHECK(inRange && deterministic && varies && cargoStartIndex(5, 0) == 0 && cargoStartIndex(5, 1) == 0);
    }
    // Vetores dourados (calculados em Python com o mesmo algoritmo): a mesma semente tem de dar a mesma caixa em qualquer ABI.
    CT_CHECK(cargoHash(1u) == 1917426218u && cargoHash(12345u) == 1970030932u && cargoHash(15999999u) == 2306428781u);
    CT_CHECK(cargoStartIndex(1u, 23) == 7 && cargoStartIndex(12345u, 23) == 18 && cargoStartIndex(15999999u, 23) == 5 && cargoStartIndex(777u, 23) == 1);
    CT_CHECK(cargoHintMeters(640.0f) == 600 && cargoHintMeters(650.0f) == 700 && cargoHintMeters(40.0f) == 0);

#undef CT_CHECK
    if (firstFailLine) *firstFailLine = firstLine;
    return failed;
}

}  // namespace zz::contractrules
