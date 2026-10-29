// Bancada headless de streaming: roda o World + IslandGenerator reais, sem GL, com semente e rota fixas, e compara a
// política antiga (limite por contagem) com o orçamento por tempo. Também executa autotestes do diagnóstico.
// Disparada por NativeBridge.perfBench (somente builds de depuração a chamam). Logs na tag ZZPerf.
#pragma once

#include <cstdint>

namespace zz::perf {

struct BenchConfig {
    uint32_t seed = 1234567;   // < 16 777 216 (limite do FloatArray do Kotlin)
    int stage = 2;             // 2 = Floresta; 16 = Terra Devastada (névoa curta)
    int radius = 8;            // raio de malha em chunks
    int meters = 900;
    float speedMps = 6.5f;
    int policy = 2;            // 0 legado, 1 orçamento, 2 os dois em sequência
    float assumedOtherMs = 8.0f;   // trabalho "do resto do quadro" assumido no orçamento adaptativo
    bool selfTest = true;
};

// 0 = ok; 1 = algum autoteste ou verificação falhou.
int runBench(const BenchConfig& cfg);

}  // namespace zz::perf
