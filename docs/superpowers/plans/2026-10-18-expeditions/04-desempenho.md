# 04 — Desempenho: diagnóstico e orçamento de streaming (etapa 3 do spec)

Spec de origem: `docs/superpowers/specs/2026-10-18-zombie-zone-expeditions-design.md`, seção "Desempenho" e item 3 de "Ordem de implementação" ("Adicionar diagnósticos de desempenho e limitar streaming por frame").

Este documento é o contrato para o agente implementador. Foi escrito a partir da leitura integral de `world/world.{h,cpp}`, `world/island.{h,cpp}`, `world/terrain.{h,cpp}`, `game/game.{h,cpp}`, `game/game_mission.cpp`, `game/protocol.h`, `game/nav.{h,cpp}`, `render/renderer.{h,cpp}`, `render/gl_util.*`, `jni_bridge.cpp`, `core/*.h`, `engine/GameSurfaceView.kt`, `engine/NativeBridge.kt`, `engine/NativeProtocol.kt` e dos trechos de `MainActivity.kt`/`HudView.kt` que tocam o motor. Números de linha são do working tree atual (branch `feature/ilhas-infinitas`, com alterações não commitadas de outra origem: preserve-as).

Regras que valem para toda a implementação:

- Sem emulador. Um único Gradle por vez, sempre `--no-daemon --max-workers=1 "-Pandroid.injected.build.abi=arm64-v8a"`. O PC tem 4 GB.
- NÃO tocar em `HudView.kt` (dono: área 01-hud), nos IDs AdMob nem em `ADMOB_TEST_DEVICES` (o aparelho do usuário já é dispositivo de teste; nunca criar IDs de teste nem variante debug com IDs de teste).
- Edições com texto acentuado: use as ferramentas Edit/Write (ou um script `.py` em arquivo), nunca heredoc de bash.
- Cada fase abaixo deixa o APK jogável por si só. A Fase A não muda comportamento nenhum do jogo.

---

## 0. Resumo executivo

| # | Decisão | Onde |
|---|---|---|
| 1 | Diagnóstico leve **sempre ligado**: janela deslizante de 300 quadros (intervalo e CPU por quadro: média, p50, p95, máx), tempo por seção, contadores de chunks/uploads/draw calls, fila pendente. Log `ZZPerf` a cada 5 s + log imediato de "hitch" (limitado a 4/s). Custo estimado: ~12 leituras de relógio por quadro (< 1 µs) e um `snprintf` a cada 5 s. | §2 |
| 2 | Para o HUD de FPS existente: **nenhuma função JNI nova**. Um bloco `H_PERF = 650..659` no mesmo `FloatArray` de `copyHud`. `H_FPS` passa a vir da janela real (hoje é uma média exponencial de 1/dt). `HudView.kt` não muda. | §2.6 |
| 3 | Streaming: o limite por contagem (2 gerações + 3 malhas por quadro, dobrando com `pending > 24`) vira **orçamento por tempo adaptativo (1–5 ms)** + piso de 1+1 unidades + faixa **urgente** (chunks sob/ao redor do jogador ignoram o orçamento) + modo **bulk** sem limite para a carga inicial. O `static int pending` vira membro. | §3 |
| 4 | Uploads de malha para a GPU ganham orçamento (4 chunks / 384 KB por quadro, mais perto primeiro, piso 1; ilimitado nos 3 primeiros quadros após a carga). | §3.6 |
| 5 | **Threading: NÃO agora.** A aritmética do §4 mostra que o orçamento por tempo basta (trabalho médio de streaming < 1 ms/quadro). O `IslandGenerator` não é thread-safe (caches mutáveis em `poi()`/`stamp()`), então thread exigiria refatoração que não se paga. Gatilhos objetivos para reabrir a decisão estão no §4.3. | §4 |
| 6 | Achado de baixo risco com ganho garantido: o raio de malha (`viewRadius_` 6/8/10) pode ser maior que o raio realmente desenhado (névoa + 22 blocos). Em Terra Devastada (névoa 95) e em noites (névoa 95) ~22% dos chunks malhados/enviados nunca são desenhados. Adotar raio efetivo = zero mudança visual. | §3.5 |
| 7 | Renderer: ajustes em ordem de ganho/risco (cache de `glGetUniformLocation`, culling por esfera de personagens e caixas, lista visível compartilhada). O que decide o resto (GPU-bound vs CPU-bound) só a medição responde: §5.6 traz a tabela sintoma→ação. | §5 |
| 8 | Cenário reproduzível **sem emulador**: (a) *bancada headless* (`NativeBridge.perfBench`) roda o `World`+`IslandGenerator` reais, sem GL, com semente e rota fixas, A/B "legado vs orçamento" no mesmo aparelho; (b) *autopiloto* em jogo real (`Cfg.PERF`) com `dt` fixo de 1/60 s e jogador invulnerável. Ambos disparados por extras de Intent apenas em `BuildConfig.DEBUG`. Checksum do conteúdo gerado prova que o orçamento não altera o mundo. Não é viável rodar no PC: não há compilador C++ de host instalado (sem `g++`/`clang`/`cl`). | §6 |
| 9 | Dois custos periódicos fora do streaming merecem medição antes de qualquer correção: `nav_.compute` (~a cada 0,3 s) e `rebuildMinimap` (a cada ~5 s). | §1.5, §5.7 |

Tamanho: **grande** (muitos arquivos quentes), mas em fases independentes e pequenas.

---

## 1. Estado atual (lido do código)

### 1.1 Loop de quadro

- `GameSurfaceView.kt`: `RENDERMODE_WHEN_DIRTY`; o `Choreographer` chama `requestRender()` quando passaram ≥ 15,5 ms do último pedido (teto efetivo ~60 fps). `onDrawFrame` faz `NativeBridge.drawFrame(System.nanoTime())` e em seguida `drainEvents` (processa eventos na mesma thread GL, inclusive `Ev.MINIMAP`, que cria um `Bitmap` 160×160 — esse custo cai **fora** de `Game::frame`, no "vão" entre o fim do quadro nativo e o próximo).
- `Game::frame(now)` (`game.cpp:307-342`): calcula `dt` (clamp 0,05 s), `fps_` (EMA 5% de `1/dt`, arredondando mal quadros longos), um `static float fpsLogTimer` que a cada 5 s imprime `LOGI("FPS %.1f (modo %d, chunks %zu, zumbis %zu, partículas %zu)")` na tag `ZombieZoneNative`. Depois `updateLobby/updateMission/updatePvp`, `buildScene()`, `renderer_.render(scene_)` e `publishHud()`.
- `updateMission` (`game.cpp:385-457`): `streamWorld(2, 3)` é a **primeira** coisa (linha 392); depois câmera, jogador, arma, `nav_.compute` (linhas 410-418), spawner, POIs, ninhos, zumbis, projéteis, partículas, `rebuildMinimap` (432-433).
- Qualidade (`setGraphics`, `game.cpp:182`): `viewRadius_ = 6/8/10` (baixa/média/alta); `Kotlin` ainda aplica escala de resolução 60/80/100% via `holder.setFixedSize`. Padrão: qualidade 1 (média = 8 chunks, 80%), `cinematic = false`.
- `Game::frame` não mede **nada**: não há tempo de quadro, contagem de chunks gerados, uploads nem fila pendente.

### 1.2 Streaming hoje

```
Game::updateMission                         game.cpp:392
  streamWorld(2, 3)                         game_mission.cpp:100-106
     static int pending = 0;                <- estado estático de função
     gen  = pending > 24 ? 4 : 2
     mesh = pending > 24 ? 6 : 3
     pending = world_.stream(px, pz, viewRadius_, gen, mesh)
World::stream                               world.cpp:188-249
  spiral_ = offsets (dx,dz) com dx²+dz² <= (R+1+0.5)², ordenados por distância   (R=8 -> ~283 chunks)
  para cada offset, do mais perto ao mais longe:
     cria o Chunk se não existe
     se !generated: se generated >= maxGenerate -> (conta pending se dentro do raio de malha) e segue
                    senão  source_->generate(...) (IslandGenerator::generate) + updateMaxY
     se dentro do raio de malha (R+0.5), dirty:
                    se meshed < maxMesh e 3x3 de vizinhos gerados -> buildChunk(...)
                    senão ++pending
  descarta até 48 chunks/chamada além de (R+1)+2 do jogador
  retorna pending
Renderer::syncChunks                        renderer.cpp:852-896   (dentro de render(), SEM limite)
  para TODO chunk meshed com gpu.version != version: 3 VAO + 3 VBO + 3x glBufferData
```

Observações:

1. **O limite é de contagem, não de tempo.** O pior quadro de streaming é `2×gerar + 3×malhar` (ou `4× + 6×` com `pending > 24`), mais até 6 uploads no renderer. Cada unidade tem custo variável (veja H1-H3). Em um aparelho forte o limite parece folgado; num intermediário um único quadro pode somar 8-15 ms (16-30 ms dobrado) só de streaming.
2. A "dobra" reage tarde: `pending > 24` significa ~1,3 anel de chunks atrasados; o remédio é justamente o quadro mais caro.
3. `static int pending` (game_mission.cpp:102) sobrevive entre missões (valor velho do jogo anterior influencia os primeiros quadros do próximo) e não pode ser consultado por diagnósticos.
4. Não há prioridade por urgência. Se o jogador for teleportado (queda abaixo de y=1 devolve ao acampamento, `game.cpp:764-768`; `unstickPlayer`; reviver), os chunks sob os pés só chegam na ordem da espiral, 2 por quadro. `World::get` devolve `B_AIR` em chunk não gerado, ou seja, **o jogador cai no vazio** até o chão existir.
5. Carga inicial: `startMission` roda até 500 iterações de `world_.stream(..., 48, 48)` até `pending == 0` (game_mission.cpp:39-40) sob o `LoadingOverlay` do Kotlin (que só some no `Ev.MISSION_READY`, processado depois do primeiro `drawFrame`). Isso é correto e deve continuar sem orçamento de tempo (modo **bulk**). O primeiro `render()` depois disso faz o upload de todos os ~227 chunks malhados de uma vez (coberto pelo overlay).
6. Nenhum bloco do terreno é alterado depois de gerado (`grep world_.set` em `game/` não encontra nada). Logo cada chunk é malhado uma vez (exceto perda de contexto GL, que marca `dirty`). Isso simplifica orçamento e, no futuro, threading.

### 1.3 Onde o tempo provavelmente vai — hipóteses a validar com a Fase A

Valores por leitura de código, para aparelho intermediário (núcleos tipo A55/A76 a ~2 GHz, `-O2`). **São hipóteses, não medições.** O propósito da Fase A é trocá-las por números.

| Id | Item | Estimativa | Base no código |
|---|---|---|---|
| H1 | `IslandGenerator::generate` por chunk | 1-3 ms (pico 5-12 ms no primeiro chunk de um POI grande, por causa do molde) | 256 colunas × `column()` (≈ 26 oitavas de `valueNoise`: `naturalHeight` 21, `forest` 3, `riverMask` 2 — com `secondaryWeight` e o ruído do rio calculados **duas vezes**) + árvores (~36 células, metade chama `column()` de fora do chunk) + plantas; `stamp()` constrói um `unordered_map` de edições na primeira vez que o POI é tocado (island.cpp:537-556) |
| H2 | `World::buildChunk` por chunk | 0,6-2,5 ms | `pad_.assign(18×18×64)` + ~6,4 mil células sólidas × 6 faces + AO (12 leituras por face visível) + `push_back` sem `reserve` (~10 realocações por vetor) |
| H3 | Upload por chunk (`syncChunks`) | 0,2-0,6 ms | 3 VAO + 3 VBO + 3 `glBufferData` (30-100 KB) + 9 `glVertexAttribPointer` |
| H4 | `rebuildMinimap` | 3-12 ms a cada ~5 s ou 26 blocos | 25 600 colunas × (`columnReady` + `topBlockY` varrendo de `maxY` para baixo + `get` + cor); depois `Ev.MINIMAP` cria `Bitmap` na thread GL |
| H5 | `nav_.compute` | 2-8 ms a cada 0,3 s andando | janela 128×128 = 16 384 colunas (`standHeight` percorre blocos) + BFS 8 vizinhos até distância 60 |
| H6 | Draw calls por quadro | 350-550 sem cinematic; 800+ com | ~80 chunks visíveis × 3 passes (opaco/recorte/água) + personagens (6 a 8 partes cada, 1 draw + 1 `glUniformMatrix4fv` por parte; zumbis até ~30) + caixas (1 draw + 3 uniformes) + billboards (6 flushes) + céu/nuvens |
| H7 | `glGetUniformLocation` por string | 150-250 chamadas/quadro | `ShaderProgram::uniform` (gl_util.h:13) sem cache; `setVoxelUniforms` roda 2×/quadro; `drawCharacters` faz 2 consultas **por personagem** (renderer.cpp:1081-1082) |
| H8 | Streaming médio | < 1 ms/quadro | demanda a 6,5 m/s: cruzar uma fronteira de chunk a cada ~2,5 s expõe ~19 chunks novos (anel da espiral) ⇒ ~8 gerações/s e ~8 malhas/s; 8×2 ms + 8×1,3 ms ≈ 26 ms/s ≈ 0,4 ms/quadro. O problema é **rajada**, não média. |

### 1.4 Renderer (custo por quadro)

Ordem em `Renderer::render` (renderer.cpp:1392-1445): `syncChunks` → (cinematic: mapa de sombras 2048², re-desenha chunks a ≤ 69 blocos + personagens + caixas) → céu → chunks opacos (frente→trás) → chunks recortados (sem culling de face) → personagens → caixas → chunks de água (trás→frente, blend) → nuvens → billboards (sombras blob, decalques, partículas, feixes) → (cinematic: bloom 1/4 de resolução, raios de sol 24 amostras, composição).

- `drawChunks` (955-982) é chamado 3× (opaco/recorte/água) mais 2× nas sombras; cada chamada percorre **todos** os chunks residentes (~280-380), faz teste AABB×6 planos, ordena (`std::sort`) e emite 1 `glBindVertexArray` + 1 `glDrawElements` por chunk visível. `quads[pass] == 0` já é pulado.
- Personagens: **sem culling de frustum** (só distância `> drawDistance` em `buildScene`). Zumbis atrás da câmera dentro de ~130 blocos são desenhados por inteiro. Em cinematic são desenhados de novo no mapa de sombras.
- `GL_STATIC_DRAW` por passe; cada chunk tem 3 VAO + 3 VBO.
- `drawDistance = fogEnd + 10`, e `drawChunks` corta chunks a `dist > drawDistance + 12`. `fogEnd = min(theme.fogEnd, viewRadius*16 - 6)` (game.cpp:1630-1634). `theme.fogEnd` vale 150 (Floresta), 140 (Deserto), 115 (Neve), 95 (Terra Devastada); `applySkyVariant` põe 95 na noite e multiplica por 0,85 no nublado (terrain.cpp:360-371).

### 1.5 Achados adicionais (todos viram itens do plano)

1. **Raio de malha > raio desenhado** nos biomas/horários de névoa curta (§3.5): trabalho, CPU, VRAM e iteração de chunks desperdiçados, sem nenhum efeito visual.
2. `ShaderProgram::uniform` consulta a string toda vez (H7).
3. Primeiro `render()` pós-carga e qualquer rajada de remalha fazem upload ilimitado (§3.6).
4. Estado estático de função/arquivo no caminho do quadro: `pending` (game_mission.cpp:102), `fpsLogTimer` (game.cpp:313), `petalTimer` (1377), `cands` (1645), `items` em `drawChunks` (957), `prevTops` (game_mission.cpp:382), `near` em `publishHud` (2495). Só `pending` e `fpsLogTimer` precisam sair (o resto são buffers de reuso sem semântica).
5. Custos periódicos (H4, H5) acontecem **sem** orçamento e provavelmente produzem os picos "regulares" que o jogador sente: um a cada ~0,3 s (nav) e outro a cada ~5 s (minimapa).
6. `World::stream` percorre ~300 offsets todo quadro (< 20 µs): irrelevante, não otimizar.
7. Perda de contexto GL (`syncChunks` marca `dirty`) já funciona; o plano deve preservá-la.
8. Risco de produto, não de desempenho: `-ffp-contract` padrão do clang pode gerar floats diferentes entre `arm64-v8a` e `armeabi-v7a`; o spec quer "seed + contrato reproduzíveis" entre amigos. Isso afeta qualquer otimização do gerador: **só mudanças que preservem exatamente a saída** (memoização), validadas pelo checksum do §6.4. Registrar o risco cross-ABI no relatório para a área de contratos (não resolver aqui).

### 1.6 Lacunas de medição hoje

Sem tempo de quadro, sem p95, sem custo por seção, sem contagem de gerações/uploads/draw calls/fila. O único dado é a linha `FPS` da tag `ZombieZoneNative` a cada 5 s (útil como **baseline grosseiro agora mesmo**, sem alterar o app — veja §6.6, passo 0).

---

## 2. Diagnósticos sempre ligados

### 2.1 Arquivos novos: `core/perf.h` e `core/perf.cpp`

Sem dependência do motor (só `<chrono>`, `<cstdio>`, `<algorithm>`, `<vector>`, e headers do Android sob `#if __ANDROID__`). Adicionar `core/perf.cpp` ao `add_library` em `CMakeLists.txt`.

```cpp
// app/src/main/cpp/core/perf.h
// Diagnóstico de desempenho sempre ligado: janela deslizante de tempos de quadro, tempo por seção,
// contadores do mundo/renderer e log periódico (tag ZZPerf). Independente do motor: compila em teste de host
// (defina ZZ_PERF_HOST para trocar o log do Android por stderr).
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#if defined(__ANDROID__) && !defined(ZZ_PERF_HOST)
#include <android/log.h>
#include <android/trace.h>
#define ZZ_PERF_LOG(...) __android_log_print(ANDROID_LOG_INFO, "ZZPerf", __VA_ARGS__)
#define ZZ_TRACE_BEGIN(name) ATrace_beginSection(name)
#define ZZ_TRACE_END() ATrace_endSection()
#else
#define ZZ_PERF_LOG(...) (std::fprintf(stderr, __VA_ARGS__), std::fputc('\n', stderr))
#define ZZ_TRACE_BEGIN(name) ((void)0)
#define ZZ_TRACE_END() ((void)0)
#endif

#define ZZ_CAT2(a, b) a##b
#define ZZ_CAT(a, b) ZZ_CAT2(a, b)
// Seção visível no Perfetto/systrace; custo desprezível com o trace desligado.
#define ZZ_TRACE_SCOPE(name) ::zz::perf::TraceScope ZZ_CAT(zzTrace_, __LINE__)(name)

namespace zz::perf {

inline double nowMs() {
    using clock = std::chrono::steady_clock;  // CLOCK_MONOTONIC no Android, o mesmo de System.nanoTime()
    return std::chrono::duration<double, std::milli>(clock::now().time_since_epoch()).count();
}

struct TraceScope {
    explicit TraceScope(const char* name) { ZZ_TRACE_BEGIN(name); }
    ~TraceScope() { ZZ_TRACE_END(); }
};

// Seções medidas (tempo INCLUSIVO em ms). Update é derivado: trabalho - (Scene + Render + Hud),
// portanto inclui Stream, Nav e Minimap. Upload está contido em Render.
enum class Sec : int { Update, Stream, Nav, Minimap, Scene, Render, Upload, Hud, Count };
enum class Cnt : int { Generated, Meshed, Unloaded, Uploads, UploadBytes, ChunkDraws, CharDraws, BoxDraws, BillboardDraws, Culled, Count };
constexpr int kSecCount = static_cast<int>(Sec::Count);
constexpr int kCntCount = static_cast<int>(Cnt::Count);
inline const char* secKey(Sec s) {
    static const char* k[kSecCount] = {"upd", "str", "nav", "mini", "scene", "rend", "upl", "hud"};
    return k[static_cast<int>(s)];
}

// p em [0,1]; ordena 'v' no lugar.
inline float percentile(float* v, int n, float p) {
    if (n <= 0) return 0.0f;
    std::sort(v, v + n);
    int k = static_cast<int>(p * static_cast<float>(n) + 0.999f) - 1;
    return v[std::max(0, std::min(n - 1, k))];
}

struct Snapshot {
    int frames = 0;                      // amostras na janela
    float windowSec = 0;
    float fps = 0;                       // frames / duração da janela
    float intAvg = 0, intP50 = 0, intP95 = 0, intMax = 0;   // intervalo entre quadros (ms)
    float workAvg = 0, workP95 = 0, workMax = 0;            // CPU dentro de Game::frame (ms)
    float sec[kSecCount] = {};           // média por quadro (ms)
    float secMax[kSecCount] = {};
    int slow33 = 0, slow50 = 0;          // quadros com intervalo > 33,3 / > 50 ms
};

class FrameProfiler {
public:
    static constexpr int kRing = 300;    // ~5 s a 60 fps
    static constexpr float kHitchIntervalMs = 50.0f, kHitchWorkMs = 25.0f;
    static constexpr double kLogEveryMs = 5000.0, kSnapEveryMs = 1000.0, kGapMs = 500.0;

    void begin(double nowSec);           // início de Game::frame; nowSec = relógio do Kotlin
    void end();                          // fim (via FrameGuard, cobre retornos antecipados)

    void add(Sec s, double ms) { sec_[static_cast<int>(s)] += static_cast<float>(ms); }
    void count(Cnt c, long long n = 1) { cnt_[static_cast<int>(c)] += n; }
    // Valores instantâneos para o log/HUD (chame antes de end()).
    void gauge(int pendingGen, int pendingUrgent, int pendingUploads, int chunks, float gpuMB, int flags);
    void setContext(int mode, int quality, bool cinematic) { mode_ = mode; quality_ = quality; cinematic_ = cinematic; }
    void skipFrames(int n) { skip_ = n; }        // descarta quadros de carga/transição

    // Execução reproduzível: acumula TODOS os quadros até endRun() e loga p50/p95/p99/máx globais.
    void startRun(const char* label);
    void endRun();

    const Snapshot& snapshot() const { return snap_; }
    float otherMs() const { return otherEma_; }          // EMA de (trabalho - streaming), para adaptar o orçamento
    float genPerSecond() const { return genPs_; }
    float uploadsPerSecond() const { return upPs_; }
    bool sustainedSlow() const { return slowSeconds_ >= 10; }   // p95 > 25 ms por >= 10 snapshots seguidos

private:
    void summarize(Snapshot& out) const;
    void logWindow(double nowMs);
    void logHitch(float interval, float work);

    double t0_ = 0, lastNow_ = -1, lastLogMs_ = 0, lastSnapMs_ = 0, lastHitchMs_ = 0;
    float interval_ = 0;
    float sec_[kSecCount] = {};
    long long cnt_[kCntCount] = {};
    // Anéis (cronológicos): intervalo, trabalho, seções.
    float ringInt_[kRing] = {}, ringWork_[kRing] = {}, ringSec_[kRing][kSecCount] = {};
    int head_ = 0, n_ = 0, skip_ = 0;
    long long gaps_ = 0;
    // Totais da janela de log (zeram a cada logWindow).
    long long tot_[kCntCount] = {}, max_[kCntCount] = {};
    int pendingMax_ = 0;
    // Valores instantâneos.
    int pendGen_ = 0, pendUrgent_ = 0, pendUp_ = 0, chunks_ = 0, flags_ = 0, mode_ = 0, quality_ = 0;
    float gpuMB_ = 0, otherEma_ = 8.0f, genPs_ = 0, upPs_ = 0;
    bool cinematic_ = false;
    int slowSeconds_ = 0;
    Snapshot snap_;
    // Execução reproduzível.
    bool running_ = false;
    char runLabel_[48] = {};
    std::vector<float> runInt_, runWork_, runStream_;
    long long runTot_[kCntCount] = {};
    double runStartMs_ = 0;
};

class FrameGuard {
public:
    FrameGuard(FrameProfiler& p, double nowSec) : p_(p) { p_.begin(nowSec); }
    ~FrameGuard() { p_.end(); }
    FrameGuard(const FrameGuard&) = delete;
    FrameGuard& operator=(const FrameGuard&) = delete;
private:
    FrameProfiler& p_;
};

class Scoped {
public:
    Scoped(FrameProfiler& p, Sec s) : p_(p), s_(s), t0_(nowMs()) {}
    ~Scoped() { p_.add(s_, nowMs() - t0_); }
    Scoped(const Scoped&) = delete;
    Scoped& operator=(const Scoped&) = delete;
private:
    FrameProfiler& p_;
    Sec s_;
    double t0_;
};

}  // namespace zz::perf
```

Regras de implementação de `perf.cpp`:

- `begin`: `t0_ = nowMs()`; `interval_ = lastNow_ < 0 ? 0 : (nowSec - lastNow_) * 1000`; `lastNow_ = nowSec`; zera `sec_` e `cnt_`.
- `end`:
  1. `work = nowMs() - t0_`. Se `skip_ > 0`: decrementa e retorna (mas atualiza `otherEma_`).
  2. `sec_[Update] = max(0, work - sec_[Scene] - sec_[Render] - sec_[Hud])`.
  3. `otherEma_ += ((work - sec_[Stream]) - otherEma_) * 0.1f` (inicial 8 ms).
  4. Se `interval_ <= 0` ou `interval_ > kGapMs`: `++gaps_`, retorna sem empilhar (pausa, anúncio, app em segundo plano, quadro de carga).
  5. Empilha `interval_`, `work` e `sec_[]` nos anéis; soma `cnt_` em `tot_` e atualiza máximos por quadro em `max_`; `pendingMax_ = max(pendingMax_, pendGen_)`.
  6. Se em `running_`: `runInt_.push_back(interval_)`, `runWork_`, `runStream_`, soma em `runTot_`.
  7. Hitch: se `interval_ > kHitchIntervalMs || work > kHitchWorkMs` e `now - lastHitchMs_ >= 250` ⇒ `logHitch`.
  8. A cada `kSnapEveryMs`: `summarize(snap_)`; atualizar `genPs_`/`upPs_` (totais da janela de log divididos por segundos decorridos); atualizar `slowSeconds_` (`++` se `snap_.intP95 > 25`, senão zera).
  9. A cada `kLogEveryMs`: `logWindow`, zerar `tot_`, `max_`, `pendingMax_`, `gaps_`.
- `summarize`: copiar o anel em ordem cronológica para dois `float[kRing]` locais, `percentile` para p50/p95, média e máximo por varredura; `fps = n / (soma dos intervalos em s)`; contar `slow33/slow50`.
- `startRun(label)`: limpa vetores, `running_ = true`, `runStartMs_ = nowMs()`, `reserve(8192)`. `endRun()`: calcula p50/p95/p99/máx de `runInt_`, `runWork_`, `runStream_` e imprime a linha `ZZPerf run done` (§2.5).

### 2.2 Integração em `Game` (game.h / game.cpp / game_mission.cpp)

`game.h`:

```cpp
#include "../core/perf.h"
...
private:
    void streamWorld();                       // sem parâmetros (antes: streamWorld(int, int))
    int streamRadius() const;                 // raio efetivo em chunks (§3.5)
    perf::FrameProfiler prof_;
    StreamStats streamStats_;                 // substitui o 'static int pending'
    int uploadUnlimitedFrames_ = 3;           // quadros pós-carga sem orçamento de upload (§3.6)
```

`Game::frame` (esqueleto; o que não aparece fica igual):

```cpp
void Game::frame(double now) {
    perf::FrameGuard guard(prof_, now);          // begin() agora, end() em qualquer saída
    ZZ_TRACE_SCOPE("ZZ frame");
    float dt = lastTime_ < 0 ? 1.0f / 60.0f : static_cast<float>(now - lastTime_);
    lastTime_ = now;
    if (dt > 0) fps_ = lerpf(fps_, 1.0f / std::max(dt, 1e-3f), 0.05f);
    dt = clampf(dt, 0.0f, 0.05f);
    if (autopilot_.active) dt = 1.0f / 60.0f;    // §6.2: trajetória independente da taxa de quadros
    time_ += dt;
    // REMOVER: static float fpsLogTimer + LOGI("FPS ...") — o ZZPerf substitui.
    prof_.setContext(static_cast<int>(mode_), quality_, renderer_.cinematic());   // quality_: novo membro guardado em setGraphics

    if (mode_ == Mode::Lobby) updateLobby(dt);
    else if (mode_ == Mode::Pvp) updatePvp(dt);
    else if (mode_ == Mode::Mission) { /* igual */ }
    if (!world_.streaming()) world_.rebuildDirty(6);
    if (mode_ == Mode::None) { /* igual */ return; }
    { perf::Scoped t(prof_, perf::Sec::Scene);  buildScene(); }
    scene_.upload.unlimited = uploadUnlimitedFrames_ > 0;
    if (uploadUnlimitedFrames_ > 0) --uploadUnlimitedFrames_;
    { perf::Scoped t(prof_, perf::Sec::Render); renderer_.render(scene_); }
    const RenderStats& rs = renderer_.stats();
    prof_.add(perf::Sec::Upload, rs.uploadMs);
    prof_.count(perf::Cnt::Uploads, rs.uploads);
    prof_.count(perf::Cnt::UploadBytes, static_cast<long long>(rs.uploadBytes));
    prof_.count(perf::Cnt::ChunkDraws, rs.chunkDraws[0] + rs.chunkDraws[1] + rs.chunkDraws[2]);
    prof_.count(perf::Cnt::CharDraws, rs.partDraws);
    prof_.count(perf::Cnt::BoxDraws, rs.boxDraws);
    prof_.count(perf::Cnt::BillboardDraws, rs.billboardFlushes);
    prof_.count(perf::Cnt::Culled, rs.culledCharacters + rs.culledBoxes);
    prof_.gauge(streamStats_.pending, streamStats_.pendingUrgent, rs.uploadsPending,
                static_cast<int>(world_.chunks().size()), rs.gpuBytes / (1024.0f * 1024.0f), perfFlags());
    if (mode_ == Mode::Mission || mode_ == Mode::Pvp) { perf::Scoped t(prof_, perf::Sec::Hud); publishHud(); }
}
```

Dentro de `updateMission`, cercar com `perf::Scoped` os três blocos periódicos: `streamWorld()` (o próprio `streamWorld` já mede `Sec::Stream`), `nav_.compute(...)` (`Sec::Nav`) e `rebuildMinimap()` (`Sec::Minimap`). `startMission`, `startPvp` e `enterLobby` (quando regenera) chamam `prof_.skipFrames(3)` e `uploadUnlimitedFrames_ = 3`.

`perfFlags()`: bit0 = 1 se `world_.streaming()`; bit1 = 2 se `streamStats_.timeCapped`; bit2 = 4 se `prof_.sustainedSlow()`; bit3 = 8 se `autopilot_.active`.

### 2.3 Contadores do mundo — `StreamStats` (world.h)

```cpp
struct StreamStats {
    int pending = 0;          // chunks do raio de malha ainda sem geração/malha (mesma semântica do retorno antigo)
    int pendingUrgent = 0;    // idem, dentro da faixa urgente (deve ser 0 em regime)
    int generated = 0, meshed = 0, unloaded = 0;
    float genMs = 0, meshMs = 0, totalMs = 0;
    bool timeCapped = false;  // o orçamento de tempo adiou trabalho neste quadro
};
```

`Game::streamWorld` soma `generated/meshed/unloaded` em `prof_.count(...)`.

### 2.4 Contadores do renderer — `RenderStats` (renderer.h)

```cpp
struct RenderStats {
    int uploads = 0, uploadsPending = 0, gpuDeletes = 0;
    size_t uploadBytes = 0, gpuBytes = 0;      // gpuBytes: VBOs de chunk residentes
    float uploadMs = 0;
    int chunkDraws[3] = {0, 0, 0};             // opaco, recorte, água (passe principal)
    int shadowChunkDraws = 0;                  // mapa de sombras (cinematic)
    int characterDraws = 0, partDraws = 0;     // personagens e partes (1 draw por parte)
    int boxDraws = 0, billboardFlushes = 0;
    int culledCharacters = 0, culledBoxes = 0;
    int quads = 0;                             // quads de chunk no passe principal
};
...
const RenderStats& stats() const { return stats_; }
int pendingUploads() const { return stats_.uploadsPending; }
```

`Renderer::render` zera `stats_` no início; `drawChunks(..., int* counter)` incrementa; `drawCharacters`/`drawBoxes`/`drawBillboards` incrementam seus contadores; `syncChunks` preenche uploads/bytes/ms. `ChunkGpu` (world.h) ganha `uint32_t bytes = 0;` e o renderer mantém `size_t gpuBytes_` (soma ao subir, subtrai ao apagar).

### 2.5 Formato do log (dicionário)

Tag fixa `ZZPerf`, prioridade Info. Linhas `chave=valor` separadas por espaço, chaves com ponto, sem espaços nos valores (parseável por regex `([\w.]+)=(\S+)`). Capturar com `adb logcat -s ZZPerf:I`.

**Janela (a cada 5 s):**

```
ZZPerf win n=298 fps=59.7 int.avg=16.7 int.p50=16.6 int.p95=18.1 int.max=31.2 work.avg=9.3 work.p95=12.0 work.max=24.5 s.upd=3.1 s.str=0.8 s.nav=0.5 s.mini=0.1 s.scene=0.7 s.rend=4.2 s.upl=0.3 s.hud=0.2 sm.str=6.1 sm.nav=5.2 sm.mini=9.8 sm.upl=2.1 slow33=2 slow50=0 gaps=0 gen.ps=7.2 gen.max=2 mesh.ps=6.6 mesh.max=3 unl.ps=7.0 up.ps=6.8 up.max=4 upKB.ps=310 pend=3 pend.max=19 pendUrg=0 chunks=301 gpuMB=18.4 dc.chunks=170 dc.chars=190 dc.boxes=48 dc.bb=6 cull=41 mode=2 q=1 cin=0 flags=1
```

| Campo | Significado |
|---|---|
| `n` | quadros válidos na janela |
| `fps` | `n` ÷ soma dos intervalos |
| `int.*` | intervalo entre chamadas de `Game::frame` (ms); inclui espera de vsync/swap e o trabalho Kotlin pós-quadro |
| `work.*` | tempo de CPU dentro de `Game::frame` (ms); **não** inclui a espera do swap |
| `s.*` | média por quadro de cada seção (ms); `sm.*` = máximo na janela |
| `slow33`/`slow50` | quadros com intervalo > 33,3 / > 50 ms |
| `gaps` | intervalos > 500 ms descartados (pausa, anúncio, carga) |
| `gen.ps`/`mesh.ps`/`up.ps`/`unl.ps` | chunks gerados / malhados / enviados / descartados por segundo; `*.max` = pico em um único quadro |
| `upKB.ps` | KB de malha enviados por segundo |
| `pend` | fila pendente de geração/malha no último quadro; `pend.max` na janela; `pendUrg` urgentes |
| `chunks` | chunks residentes (`world_.chunks().size()`); `gpuMB` VBOs de chunk na GPU |
| `dc.*` | por quadro (média): draws de chunk, partes de personagem, caixas, flushes de billboard; `cull` = personagens+caixas descartados por frustum |
| `mode` | 0 nenhum, 1 lobby, 2 missão, 3 PvP; `q` qualidade; `cin` cinematic; `flags` bits do §2.6 |

**Interpretação rápida:** `gap = int.avg - work.avg`. Gap pequeno e `int.avg` ≈ 16,7 ⇒ saudável. Gap grande (≥ 5 ms) ⇒ o quadro está esperando a GPU/composição (limitado por GPU ou vsync). `work.avg` alto ⇒ limitado por CPU; olhar `s.*`.

**Hitch (imediato, máx 4/s):**

```
ZZPerf hitch int=62.1 work=48.3 top=str:31.0 str=31.0 upd=33.5 nav=0.0 mini=0.0 scene=1.2 rend=12.0 upl=8.1 gen=2 mesh=3 up=6 pend=22 chunks=301
```

`top=` é a seção com maior tempo entre `str, nav, mini, scene, rend, upl, hud` (atribui a culpa). Critério: `int > 50 ms` ou `work > 25 ms`.

**Carga (uma vez por missão, em `startMission`):**

```
ZZPerf load total=2140 tex=60 island=3 stream=1380 gen=283 mesh=227 nests=12 chars=210 bigmap=370 minimap=45 ms
```

Obter com um `lap()` de `perf::nowMs()` entre as etapas de `startMission` (game_mission.cpp:14-98): `generateBlockTextures`, construção de `IslandGenerator`, o laço bulk, `stamp` dos ninhos/portal, `loadZombieAssets`, `buildBigMap`, `rebuildMinimap`.

**Execução reproduzível (`endRun`):**

```
ZZPerf run done label=auto1 frames=7200 sec=118.4 int.p50=16.7 int.p95=18.0 int.p99=24.6 int.max=61.3 work.p50=8.8 work.p95=12.1 work.p99=16.0 work.max=33.5 str.p95=3.9 str.p99=5.2 str.max=12.4 slow33=14 slow50=2 gen=1790 mesh=1510 up=1510 upMB=96.2 pend.max=21 starved=0 hash=0x9f3a21c5d0e4b7aa
```

**Bancada:** formato no §6.3.

### 2.6 Interface para o Kotlin / HUD de FPS existente (sem tocar `HudView.kt`)

Nenhuma função JNI nova para o HUD. O `FloatArray` de `NativeBridge.copyHud(out)` já tem 660 posições; `H_WAVE_FLAGS = 643` está só declarado (nenhum código escreve nele). Reservar `650..659`:

`protocol.h` (dentro de `enum HudIndex`, antes de `H_SIZE`):

```cpp
    H_WAVE_FLAGS = 643,
    // Desempenho (somente leitura pelo HUD; 644-649 livres)
    H_PERF_FPS = 650,        // quadros/s na janela deslizante (~5 s)
    H_PERF_AVG_MS = 651,     // intervalo médio entre quadros
    H_PERF_P95_MS = 652,     // p95 do intervalo
    H_PERF_MAX_MS = 653,     // pior intervalo da janela
    H_PERF_WORK_MS = 654,    // CPU média dentro de Game::frame
    H_PERF_STREAM_MS = 655,  // média de tempo de streaming de mundo por quadro
    H_PERF_PENDING = 656,    // chunks pendentes de geração/malha
    H_PERF_GEN_PS = 657,     // chunks gerados por segundo
    H_PERF_UP_PS = 658,      // uploads de malha por segundo
    H_PERF_FLAGS = 659,      // bit0 streaming ativo, bit1 orçamento esgotado, bit2 degradação sustentada (p95>25 ms por 10 s), bit3 autopiloto
    H_SIZE = 660
```

Os valores são **explícitos** (`= 650`...) para que o teste de espelho (§8) consiga ler o header com regex.

`NativeProtocol.kt` (`object Hud`):

```kotlin
    const val PERF_FPS = 650
    const val PERF_AVG_MS = 651
    const val PERF_P95_MS = 652
    const val PERF_MAX_MS = 653
    const val PERF_WORK_MS = 654
    const val PERF_STREAM_MS = 655
    const val PERF_PENDING = 656
    const val PERF_GEN_PS = 657
    const val PERF_UP_PS = 658
    const val PERF_FLAGS = 659
    const val SIZE = 660   // inalterado
```

`publishHud` (game.cpp ~2439): trocar `h[H_FPS] = fps_` por

```cpp
const perf::Snapshot& ps = prof_.snapshot();
h[H_FPS] = ps.frames >= 30 ? ps.fps : fps_;            // o texto "NN FPS" do HUD passa a refletir a janela real
h[H_PERF_FPS] = ps.fps;       h[H_PERF_AVG_MS] = ps.intAvg;   h[H_PERF_P95_MS] = ps.intP95;
h[H_PERF_MAX_MS] = ps.intMax; h[H_PERF_WORK_MS] = ps.workAvg;
h[H_PERF_STREAM_MS] = ps.sec[static_cast<int>(perf::Sec::Stream)];
h[H_PERF_PENDING] = static_cast<float>(streamStats_.pending);
h[H_PERF_GEN_PS] = prof_.genPerSecond();  h[H_PERF_UP_PS] = prof_.uploadsPerSecond();
h[H_PERF_FLAGS] = static_cast<float>(perfFlags());
```

Contrato com a área 01-hud: nada é obrigatório. Se quiser, o HUD pode mostrar `"${Hud.PERF_FPS} FPS · p95 ${Hud.PERF_P95_MS} ms"` e sinalizar `PERF_FLAGS and 4`. `publishHud` só roda em missão e PvP; no lobby o HUD está oculto.

Configuração da execução reproduzível (§6.2) usa dois índices novos no `FloatArray` de `startMission` (não no HUD):

```cpp
// protocol.h, enum ConfigIndex
    C_EXPLOSIVES = 27,
    C_PERF = 28,         // 0 desligado, 1 autopiloto sem inimigos, 2 autopiloto com inimigos
    C_PERF_FRAMES = 29,  // duração em quadros simulados (0 = 7200)
    C_COUNT = 30
```

e `Cfg.PERF = 28`, `Cfg.PERF_FRAMES = 29`, `Cfg.COUNT = 30` em `NativeProtocol.kt`. `jni_bridge.cpp` lê os dois em `MissionConfig::perfMode/perfFrames`.

### 2.7 Traços Perfetto (opcional, quase grátis)

`ZZ_TRACE_SCOPE` (acima) em: `Game::frame` ("ZZ frame"), `streamWorld` ("ZZ streamWorld"), `World::stream` ("ZZ stream"), `IslandGenerator::generate` via `World::stream` ("ZZ genChunk"), `World::buildChunk` ("ZZ meshChunk"), `Renderer::render` ("ZZ render"), `syncChunks` ("ZZ upload"), `drawChunks` ("ZZ drawChunks"), `NavField::compute` ("ZZ nav"), `rebuildMinimap` ("ZZ minimap"). `ATrace_beginSection` existe desde a API 23 e o `minSdk` é 24; a lib `android` já está linkada. Captura: §6.7.

### 2.8 Custo da própria instrumentação

~12 chamadas a `steady_clock` por quadro (~40 ns cada em arm64 via vDSO), cópia de ~25 floats para os anéis, um `snprintf` + `__android_log_write` a cada 5 s e no máximo 4 hitches/s. Meta de aceitação: `work.avg` não pode subir mais que 0,1 ms em relação ao build anterior (§9, A7).

---

## 3. Orçamento de streaming

### 3.1 O modelo novo em uma frase

Cada chamada a `World::stream` tem um **orçamento de tempo adaptativo** (1-5 ms) para trabalho "normal", um **piso** (1 geração + 1 malha, para nunca parar), uma **faixa urgente** (chunks em volta do jogador, que ignoram o orçamento até um teto de segurança de 12 ms) e um **teto absoluto** por contagem (4+6) contra explosões de custo; a carga inicial usa o modo **bulk** (sem tempo).

Prioridade dentro da chamada, na ordem da espiral (perto→longe):

1. **Urgente**: geração até o anel `urgentRadius+1` (5×5 = 25 chunks) e malha até `urgentRadius` (3×3), `urgentRadius = 1`. Ignora `maxMs`/`maxGenerate`/`maxMesh`; limitado por `urgentMaxMs = 12`. Garante chão sob o jogador (física lê `World::get`) e vizinhos para a malha dos 9 chunks centrais.
2. **Piso**: se ainda há pendência, pelo menos `minGenerate = 1` e `minMesh = 1` por chamada, qualquer que seja o custo (garante progresso mesmo num aparelho já estourado).
3. **Orçamento**: continua enquanto `gasto + previsão(tipo) <= maxMs`, com `previsão` = média móvel exponencial (α = 0,2) do custo observado de gerar/malhar (inicial 2,0 e 1,2 ms). A previsão evita estourar o orçamento com mais uma unidade que provavelmente não cabe.
4. **Teto**: `maxGenerate = 4`, `maxMesh = 6` por chamada (proteção contra previsão otimista).

`maxMs` é recalculado por quadro em `Game::streamWorld`:

```
maxMs = clamp(16,7 - otherMs - 3,0, 1,0 .. 5,0)     // otherMs = EMA de (trabalho do quadro - streaming)
```

Ex.: `otherMs = 8` ⇒ 5,0 ms (teto); `otherMs = 12` ⇒ 1,7 ms; `otherMs >= 13` ⇒ 1,0 ms (só piso). A meta de 16,7 ms é o teto de ~60 fps do `GameSurfaceView`; a folga de 3 ms cobre o swap e o trabalho Kotlin pós-quadro.

Por que isso estabiliza: a demanda média é ~0,4 ms/quadro (H8); o orçamento só importa nas rajadas de cruzamento de fronteira (~19 gerações em ~1 s). Com 3-5 ms/quadro essa rajada vira ~13-21 quadros de 3-5 ms em vez de ~10 quadros de 8-15 ms; e como os chunks novos nascem no anel mais distante (atrás da névoa), a latência extra é invisível.

### 3.2 `world.h`: tipos novos e assinatura

```cpp
struct StreamBudget {
    float maxMs = 3.0f;          // tempo (geração + malha) permitido por chamada, fora urgente/piso
    int minGenerate = 1;         // piso por chamada, se houver pendência
    int minMesh = 1;
    int maxGenerate = 4;         // teto absoluto por chamada
    int maxMesh = 6;
    int urgentRadius = 1;        // chunks; < 0 desliga a faixa urgente
    float urgentMaxMs = 12.0f;   // teto de segurança do trabalho urgente num quadro
    bool timed = true;           // false = só contagem (comportamento antigo)

    static StreamBudget legacy(int gen, int mesh) {   // reproduz exatamente o limite antigo
        StreamBudget b;
        b.timed = false; b.minGenerate = b.minMesh = 0;
        b.maxGenerate = gen; b.maxMesh = mesh; b.urgentRadius = -1;
        return b;
    }
    static StreamBudget bulk() { return legacy(48, 48); }   // carga inicial
};

// Registro opcional de amostras por chunk (somente a bancada usa).
struct StreamTrace { std::vector<float> genMs, meshMs; };

class World {
public:
    ...
    StreamStats stream(float x, float z, int radius, const StreamBudget& budget);
    // Compatibilidade: delega para o modo legado e devolve só 'pending'.
    int stream(float x, float z, int radius, int maxGenerate, int maxMesh) {
        return stream(x, z, radius, StreamBudget::legacy(maxGenerate, maxMesh)).pending;
    }
    // Diagnóstico/bancada
    void setTrace(StreamTrace* t) { trace_ = t; }
    void enableContentHash(bool on) { hashContent_ = on; contentHash_ = 0; }
    uint64_t residentHash(float x, float z, int radius) const;   // soma ordem-independente de hashChunk() dos chunks gerados no raio
private:
    float genEmaMs_ = 2.0f, meshEmaMs_ = 1.2f;
    StreamTrace* trace_ = nullptr;
    bool hashContent_ = false;
    uint64_t contentHash_ = 0;
```

`resetStreaming` e `clear` redefinem `genEmaMs_/meshEmaMs_` e `contentHash_`.

### 3.3 `World::stream` — corpo novo (substitui world.cpp:188-249)

Mantém intactos: construção de `spiral_`, `pcx/pcz`, `meshLim`, criação de chunk, preenchimento de `blocks/ground/lights/campfires` antes de `generate`, `updateMaxY`, e o descarte final. Muda o controle de fluxo e a coleta de estatísticas:

```cpp
StreamStats World::stream(float fx, float fz, int radius, const StreamBudget& b) {
    StreamStats st;
    if (!source_) return st;
    ZZ_TRACE_SCOPE("ZZ stream");
    const double t0 = perf::nowMs();
    const int genRadius = radius + 1;
    // ... (reconstrução de spiral_ idêntica à atual) ...
    const int pcx = static_cast<int>(std::floor(fx)) >> 4, pcz = static_cast<int>(std::floor(fz)) >> 4;
    const float meshLim = (radius + 0.5f) * (radius + 0.5f);
    // Faixa urgente em distância de Chebyshev (quadrado): geração até urgentRadius+1 (5x5), malha até urgentRadius (3x3).
    // -1 desliga (nenhum 'cheb' >= 0 é <= -1), que é o caso do modo legado/bulk.
    const int urgentMeshCheb = b.urgentRadius >= 0 ? b.urgentRadius : -1;
    const int urgentGenCheb = b.urgentRadius >= 0 ? b.urgentRadius + 1 : -1;

    // Pode executar mais uma unidade? 'done' = unidades já feitas nesta chamada deste tipo.
    auto allowed = [&](bool urgent, int done, int floorUnits, int capUnits, float predictedMs) {
        const float spent = static_cast<float>(perf::nowMs() - t0);
        if (urgent && b.timed) return spent < b.urgentMaxMs;       // urgente: ignora contagem e maxMs
        if (done >= capUnits) return false;
        if (!b.timed) return true;                                 // modo legado: só contagem
        if (done < floorUnits) return true;                        // piso: sempre progride
        return spent + predictedMs <= b.maxMs;                     // cabe no orçamento?
    };

    for (const auto& off : spiral_) {
        const int cx = pcx + off.first, cz = pcz + off.second;
        if (cx < 0 || cz < 0 || cx >= chunksX_ || cz >= chunksZ_) continue;
        const float d2 = static_cast<float>(off.first * off.first + off.second * off.second);
        const int cheb = std::max(std::abs(off.first), std::abs(off.second));
        const bool inMesh = d2 <= meshLim;
        Chunk* c = chunkAt(cx, cz);
        if (!c) c = createChunk(cx, cz);
        if (!c->generated) {
            if (!allowed(cheb <= urgentGenCheb, st.generated, b.minGenerate, b.maxGenerate, genEmaMs_)) {
                st.timeCapped = true;
                if (inMesh) { ++st.pending; if (cheb <= urgentMeshCheb) ++st.pendingUrgent; }
                continue;
            }
            const double g0 = perf::nowMs();
            c->blocks.assign(static_cast<size_t>(kChunkSize) * kChunkSize * height_, B_AIR);
            std::fill(std::begin(c->ground), std::end(c->ground), static_cast<int16_t>(0));
            c->lights.clear();
            c->campfires.clear();
            { ZZ_TRACE_SCOPE("ZZ genChunk"); source_->generate(*c, height_); }
            updateMaxY(*c);
            c->generated = true;
            c->dirty = true;
            const float ms = static_cast<float>(perf::nowMs() - g0);
            genEmaMs_ += (ms - genEmaMs_) * 0.2f;
            st.genMs += ms;
            ++st.generated;
            if (trace_) trace_->genMs.push_back(ms);
            if (hashContent_) contentHash_ += hashChunk(*c);
        }
        if (inMesh && c->dirty) {
            if (neighborsReady(cx, cz) && allowed(cheb <= urgentMeshCheb, st.meshed, b.minMesh, b.maxMesh, meshEmaMs_)) {
                const double m0 = perf::nowMs();
                { ZZ_TRACE_SCOPE("ZZ meshChunk"); buildChunk(*c); }
                c->dirty = false;
                c->meshed = true;
                ++c->version;
                const float ms = static_cast<float>(perf::nowMs() - m0);
                meshEmaMs_ += (ms - meshEmaMs_) * 0.2f;
                st.meshMs += ms;
                ++st.meshed;
                if (trace_) trace_->meshMs.push_back(ms);
            } else {
                ++st.pending;
                if (cheb <= urgentMeshCheb) ++st.pendingUrgent;
                if (neighborsReady(cx, cz)) st.timeCapped = true;   // adiado pelo orçamento, não por vizinho faltando
            }
        }
    }
    // Descarte: igual ao atual, contando st.unloaded
    ...
    st.totalMs = static_cast<float>(perf::nowMs() - t0);
    return st;
}
```

`hashChunk` (função estática em world.cpp): FNV-1a de 64 bits sobre `chunk.blocks` semeado com `(cx, cz)`; a soma de todos os chunks é ordem-independente.

Equivalência com o legado (importante para o A/B): com `StreamBudget::legacy(g, m)`, `allowed()` cai em `done >= capUnits` ⇒ falso (pula) e senão verdadeiro ⇒ idêntico ao `generated >= maxGenerate` / `meshed < maxMesh` de hoje; `urgentRadius = -1` torna os limites de Chebyshev negativos (nenhum `cheb >= 0` é ≤ -1), então nada é urgente. Incluir `../core/perf.h` em `world.h`/`world.cpp` (e `<cstdlib>` para `std::abs`).

### 3.4 `Game::streamWorld` — adaptação por quadro (game_mission.cpp:100)

```cpp
namespace {
constexpr float kTargetFrameMs = 16.7f;   // teto de ~60 fps do GameSurfaceView
constexpr float kStreamSafetyMs = 3.0f;   // swap + trabalho Kotlin pós-quadro
constexpr float kStreamMinMs = 1.0f, kStreamMaxMs = 5.0f;
}

void Game::streamWorld() {
    if (!world_.streaming()) return;
    ZZ_TRACE_SCOPE("ZZ streamWorld");
    perf::Scoped timer(prof_, perf::Sec::Stream);
    StreamBudget b;
#ifdef ZZ_STREAM_LEGACY   // chave de emergência: -DZZ_STREAM_LEGACY volta ao limite antigo
    const bool behind = streamStats_.pending > 24;
    b = StreamBudget::legacy(behind ? 4 : 2, behind ? 6 : 3);
#else
    b.maxMs = clampf(kTargetFrameMs - prof_.otherMs() - kStreamSafetyMs, kStreamMinMs, kStreamMaxMs);
    if (renderer_.pendingUploads() > 16) b.maxMesh = 1;   // contrapressão: GPU atrasada, não acumule malhas na CPU
#endif
    streamStats_ = world_.stream(player_.position.x, player_.position.z, streamRadius(), b);
    prof_.count(perf::Cnt::Generated, streamStats_.generated);
    prof_.count(perf::Cnt::Meshed, streamStats_.meshed);
    prof_.count(perf::Cnt::Unloaded, streamStats_.unloaded);
}
```

Chamada em `updateMission` (game.cpp:392): `streamWorld();`. Em `startMission`, zerar `streamStats_ = StreamStats{}` e trocar o laço bulk por:

```cpp
for (int i = 0; i < 500; ++i)
    if (world_.stream(layout_.start.x, layout_.start.z, streamRadius(), StreamBudget::bulk()).pending == 0) break;
```

Tuning futuro (opcional, só com dados): se `pendingUrgent == 0` e o jogador está parado (< 0,5 m/s) com `pending > 12`, dobrar `maxMs` (recupera rápido depois de teleporte/retorno de anúncio sem custo percebido).

### 3.5 Raio efetivo de streaming (zero mudança visual)

`drawChunks` só desenha chunk com distância do centro ≤ `drawDistance + 12 = fogEnd + 22`. Chunks malhados além disso nunca aparecem. Substituir `viewRadius_` por:

```cpp
int Game::streamRadius() const {
    // Nenhum chunk além de (névoa + 10 + 12) é desenhado (Renderer::drawChunks): não vale gerar nem malhar.
    const float fogEnd = std::min(theme_.sky.fogEnd, viewRadius_ * 16.0f - 6.0f);   // igual a buildScene (game.cpp:1630)
    const int r = static_cast<int>(std::ceil((fogEnd + 22.0f) / 16.0f - 0.5f));
    return std::max(4, std::min(viewRadius_, r));
}
```

Efeito: `viewRadius_ = 8`: Floresta/Deserto/Neve (névoa ≥ 115) ⇒ 8 (sem mudança); Terra Devastada ou noite (névoa 95) ⇒ 7 (−22% de chunks na malha: π·7,5² vs π·8,5²). Usar `streamRadius()` em `streamWorld`, no laço bulk e (para consistência) em qualquer ponto que hoje use `viewRadius_` como raio de mundo. `buildScene` continua usando `viewRadius_` para a névoa (inalterado). Se a área de contratos reduzir a visão ("noite vermelha"), esta função faz o streaming acompanhar automaticamente: avisar a área 05/contratos que o raio de mundo deriva de `theme_.sky.fogEnd`.

### 3.6 Orçamento de upload para a GPU (renderer)

`FrameScene` ganha `UploadBudget upload;`:

```cpp
struct UploadBudget { int maxChunks = 4; size_t maxBytes = 384 * 1024; bool unlimited = false; };
```

`Game` seta `scene_.upload.unlimited = uploadUnlimitedFrames_ > 0` (3 quadros após `startMission/startPvp/enterLobby`, cobertos pelo overlay de carregamento).

`Renderer::syncChunks(World&, const FrameScene&)` (renderer.cpp:852) passa a:

```cpp
void Renderer::syncChunks(World& world, const FrameScene& scene) {
    ZZ_TRACE_SCOPE("ZZ upload");
    const double t0 = perf::nowMs();
    world.drainGpuGarbage(garbage_);
    for (ChunkGpu& g : garbage_) {
        if (g.contextId != contextId_) continue;
        glDeleteVertexArrays(3, g.vao);
        glDeleteBuffers(3, g.vbo);
        gpuBytes_ -= std::min<size_t>(gpuBytes_, g.bytes);
        ++stats_.gpuDeletes;
    }
    garbage_.clear();

    uploadQueue_.clear();                       // std::vector<Chunk*> membro (reuso)
    for (Chunk* c : world.chunks()) {
        if (!c->meshed) continue;
        ChunkGpu& gpu = c->gpu;
        if (gpu.contextId == contextId_ && gpu.version == c->version) continue;
        if (gpu.contextId >= 0 && gpu.contextId != contextId_) {   // contexto perdido: igual ao código atual
            gpu = ChunkGpu{};
            c->dirty = true;
            continue;
        }
        uploadQueue_.push_back(c);
    }
    const Vec3 f = scene.focus;
    auto d2 = [&](const Chunk* c) { const float dx = c->cx * 16.0f + 8.0f - f.x, dz = c->cz * 16.0f + 8.0f - f.z; return dx * dx + dz * dz; };
    std::sort(uploadQueue_.begin(), uploadQueue_.end(), [&](const Chunk* a, const Chunk* b) { return d2(a) < d2(b); });

    const UploadBudget& ub = scene.upload;
    size_t bytes = 0;
    int n = 0;
    for (Chunk* c : uploadQueue_) {
        const size_t cb = (c->mesh.opaque.size() + c->mesh.cutout.size() + c->mesh.water.size()) * sizeof(VoxelVertex);
        if (!ub.unlimited && n >= 1 && (n >= ub.maxChunks || bytes + cb > ub.maxBytes)) break;   // piso: sempre sobe o mais próximo
        uploadChunk(*c, cb);          // corpo atual do laço (gen/bind/glBufferData/atributos), extraído SEM alterar comportamento
        bytes += cb;
        ++n;
    }
    stats_.uploads = n;
    stats_.uploadBytes = bytes;
    stats_.uploadsPending = static_cast<int>(uploadQueue_.size()) - n;
    stats_.gpuBytes = gpuBytes_;
    stats_.uploadMs = static_cast<float>(perf::nowMs() - t0);
}
```

`uploadChunk(Chunk&, size_t bytes)`: o miolo atual (renderer.cpp:870-894: `glGenVertexArrays/glGenBuffers` se for novo, laço dos 3 passes, `gpu.version = c->version`, `c->mesh = ChunkMesh()`), mais `gpuBytes_ += bytes - gpu.bytes; gpu.bytes = bytes;`.

Consequências: chunk malhado e ainda não enviado simplesmente não é desenhado (`drawChunks` já exige `gpu.contextId == contextId_`); a ordem por distância garante que o buraco, se existir, fica no anel mais distante (atrás da névoa). A malha permanece na CPU até subir (≤ 16 chunks × ~70 KB graças à contrapressão em `streamWorld`).

### 3.7 Pré-aquecer moldes de POI (corta o pico de H1)

O primeiro chunk de cada POI constrói o molde (`IslandGenerator::stamp`, island.cpp:537) dentro de `generate()`; vilarejos/prédios podem custar vários ms e o orçamento não consegue fatiar uma unidade. Como o molde é função pura de (POI, seed), construí-lo antes não muda o conteúdo.

- `island.h`: `bool hasStamp(const IslandPoi& p) const { return stamps_.count(p.id) != 0; }`.
- `Game::updatePois` (game_mission.cpp:158, roda a cada 0,5 s): depois do laço atual, `if (prewarmTimer_ <= 0)`: `island_->poisNear(player_.position, 230.0f, prewarmScratch_)`; para o primeiro `p` com `!island_->hasStamp(*p)`, chamar `island_->stamp(*p)` e `break`; `prewarmTimer_ = 0.25f`. Um molde por 0,25 s, a ≥ 100 blocos de distância do anel desenhado (o POI entra no raio de geração, ~144 blocos, depois de ser pré-aquecido na maioria dos casos).
- Membros novos: `float prewarmTimer_ = 0; std::vector<const IslandPoi*> prewarmScratch_;` (reiniciar em `startMission`).

### 3.8 O que NÃO fazer agora

- Não fatiar uma geração de chunk em vários quadros (complexidade de estado parcial) antes de a medição mostrar `gen.ms p95` alto *depois* do pré-aquecimento (§4.3, gatilho G4).
- Não trocar o formato de malha (3 VBO/VAO por chunk) nem fazer greedy meshing: muda AO/UV e o ganho não está medido.

---

## 4. Migrar geração para fila de CPU (thread)? Recomendação: **NÃO agora**

### 4.1 Argumentos de custo

- Média de streaming ~0,4 ms/quadro (H8); a rajada de ~19 gerações + ~19 malhas ≈ 60-70 ms de trabalho a cada ~2,5 s a 6,5 m/s (≈ 1,9 s com adrenalina). Espalhada por 13-21 quadros de 3-5 ms, cabe no orçamento sem tocar a latência visível. Com `viewRadius_ = 10` a rajada cresce ~25%, ainda dentro do mesmo regime.
- Thread só vale quando uma **unidade** individual (um chunk) já estoura o quadro ou quando a demanda média supera ~30% de um núcleo. Nenhum dos dois é esperado; a Fase A medirá.

### 4.2 Argumentos de segurança: o `World` e o `IslandGenerator` não são thread-safe hoje

| Estado compartilhado | Quem escreve | Quem lê | Perigo | Correção mínima se algum dia houver thread |
|---|---|---|---|---|
| `IslandGenerator::cache_`/`cached_` (`poi()`, island.cpp:187-209) | `generate()`, `updatePois`, `startMission`, `mapColor` | os mesmos | `cached_[id]=1` pode ficar visível antes do `IslandPoi` (ARM é fracamente ordenado); `poi()` devolve referência | pré-popular as 48×48 = 2 304 células no construtor (~2-5 ms) e torná-lo imutável |
| `IslandGenerator::stamps_` (`stamp()`, 537-556) | primeiro chunk do POI | `startMission`, `updatePois` | inserção concorrente em `unordered_map` | `std::mutex` em `stamp()` (referências a nós não invalidam) ou pré-aquecer |
| `World::grid_`/`list_` (`createChunk`/`destroyChunk`) | thread GL | GL, renderer, nav, jogo | mutação estrutural durante leitura | o worker só preenche um buffer privado; a thread GL insere no `grid_` |
| `Chunk::generated/dirty/meshed/version` | GL | GL | OK se só a GL escreve | publicar o resultado por fila (mutex) |
| `World::pad_` (scratch de `buildChunk`) | `buildChunk` | idem | compartilhado entre threads | scratch por thread |
| `Chunk::blocks` dos 8 vizinhos durante a malha | geração (uma vez) | malha | `destroyChunk` na GL enquanto o worker lê | `shared_ptr<const Chunk>` ou adiar destruição enquanto houver job |
| `blockInfo()` | constante | todos | nenhum (constante) | — |
| GL (upload, VAO/VBO) | thread GL | — | contexto é da thread GL | upload permanece na thread GL |
| `World::generation_` | `resetStreaming` | — | resultado de job de uma ilha antiga | comparar geração no retorno do job e descartar |
| `island_` substituído em `startMission` | GL | worker | uso após liberar | esvaziar/cancelar a fila antes de trocar |

Também é preciso: cancelamento por distância, ordem estável (determinismo do §6), medição de energia/térmica em big.LITTLE e testes de corrida sem ferramenta de TSAN no ambiente (sem emulador).

### 4.3 Gatilhos objetivos para reabrir a decisão

Medidos pela bancada/autopiloto *depois* da Fase C, no aparelho de referência, qualidade Média e Alta:

- **G1**: `gen.ms` p95 > 8 ms de forma persistente (excluindo o primeiro chunk de cada POI, que o §3.7 trata).
- **G2**: `pend` > 24 por mais de 2 s consecutivos na rota a 6,5 m/s com o orçamento no teto (o piso 1+1 não dá conta).
- **G3**: `str.p99` > 5 ms apesar de `maxMs` = 5 (unidades atômicas grandes) — nesse caso fatiar a unidade (G4) antes de usar thread.
- **G4**: `gen.ms` p95 > 6 ms depois do pré-aquecimento: fatiar `generate()` em etapas (colunas em 4 faixas de 4 linhas, depois árvores, plantas, POI) com `Chunk::genStage`, mantendo tudo na thread GL.
- **G5**: mudança de produto: raio ≥ 12 chunks, geração claramente mais pesada (cavernas, estruturas maiores) ou mapa mais denso.

Pré-requisitos baratos e úteis por si só (podem entrar na Fase E sem thread): pré-popular o cache de POIs, `scratch` de malha passado por parâmetro, `World::stream` separado em planejar/executar.

---

## 5. Renderer: custo por quadro e ajustes de baixo risco

### 5.1 Contagem de draw calls (estimativa por leitura; confirmar com `dc.*`)

| Grupo | Fórmula | Típico (média, sem cinematic) |
|---|---|---|
| Chunks | `visíveis × passes não vazios` (opaco, recorte, água) | ~80 × 2-3 ≈ 170 |
| Personagens | `(zumbis em ≤ drawDistance + jogador) × 6-8 partes` | 20-30 × 7 ≈ 140-210 (1 draw + 1 `glUniformMatrix4fv` por parte) |
| Caixas | arma/itens/zumbis/ninhos/baús/projéteis | 30-80 (1 draw + 3 uniformes) |
| Billboards | flushes | ~6 |
| Céu, nuvens | | 2 |
| **Total** | | **350-550** |
| Cinematic | +sombras: chunks ≤ 69 blocos ×2 passes + personagens + caixas; +pós (bloom, raios, composição) | **800+** e ~6 passes de tela cheia |

### 5.2 Ajustes, ordenados por ganho/risco (cada um é um commit separado, medido com a bancada/autopiloto)

**D1 — Cache de locais de uniforme** (risco muito baixo; ganho pequeno, ~0,1-0,3 ms): `ShaderProgram` (gl_util.h:11) guarda um cache linear por ponteiro de literal. Todas as chamadas atuais usam literais (`p.uniform("u_...")`).

```cpp
struct ShaderProgram {
    GLuint id = 0;
    GLint uniform(const char* name) const {
        if (cachedId_ != id) { count_ = 0; cachedId_ = id; }          // programa recriado (onSurfaceCreated)
        for (int i = 0; i < count_; ++i) if (keys_[i] == name) return locs_[i];
        const GLint loc = glGetUniformLocation(id, name);
        if (count_ < kMax) { keys_[count_] = name; locs_[count_] = loc; ++count_; }
        return loc;
    }
private:
    static constexpr int kMax = 64;
    mutable const char* keys_[kMax] = {};
    mutable GLint locs_[kMax] = {};
    mutable int count_ = 0;
    mutable GLuint cachedId_ = 0;
};
```

Atenção: a chave é o **endereço** do literal; nunca passar buffer dinâmico a `uniform()`. Se um literal duplicado em outro ponto do arquivo tiver endereço diferente, apenas ocupa outra entrada (correto, só menos eficiente).

**D2 — Frustum culling de personagens e caixas** (risco baixo; ganho 1-3 ms com horda ao redor; hoje só há corte por distância):

- Normalizar os planos em `extractPlanes` (renderer.cpp:599) — o teste AABB de `drawChunks` continua válido com planos normalizados.
- `CharacterDraw` ganha `Vec3 boundCenter; float boundRadius = 0;` (0 = nunca cortar; o AK-47 importado fica em 0). `Game::addCharacter` (game.cpp:1441) preenche: `boundCenter = pos + Vec3(0, 0.95f * scale, 0); boundRadius = 2.0f * scale * max(scaleXYZ)` (folga para animação de morte deitado e braços).
- Caixas: centro = coluna de translação de `box.world` (`m[12..14]`) e raio = metade da diagonal: `0.5f * sqrt(sx² + sy² + sz²)` com `sx = |col0|`, `sy = |col1|`, `sz = |col2|`. Sem mudar `BoxDraw`.
- `drawCharacters(scene, program, depthOnly)`: planos = `depthOnly ? shadowPlanes_ : frustum_`; `if (draw.boundRadius > 0 && !sphereVisible(planes, draw.boundCenter, draw.boundRadius)) { ++stats_.culledCharacters; continue; }`. Idem `drawBoxes` (precisa dos planos como parâmetro).

```cpp
static bool sphereVisible(const Vec4* planes, const Vec3& c, float r) {
    for (int i = 0; i < 6; ++i)
        if (planes[i].x * c.x + planes[i].y * c.y + planes[i].z * c.z + planes[i].w < -r) return false;
    return true;
}
```

Cuidado no mapa de sombras: um zumbi atrás da câmera pode projetar sombra para dentro da vista; por isso, no passe de sombras o teste usa `shadowPlanes_` (volume da luz), não os planos da câmera.

**D3 — Raio de streaming efetivo** (§3.5): reduz malha, uploads, VRAM e iteração de chunks em Terra Devastada e à noite, sem mudança visual.

**D4 — Reuso de memória na malha** (risco baixo; ganho 10-25% de `buildChunk`): `buildChunk` escreve num `ChunkMesh scratch_` membro (limpo, capacidade mantida) e no fim copia com tamanho exato para `chunk.mesh.{opaque,cutout,water}` (`assign(begin,end)` — uma alocação por vetor em vez de ~10 realocações). Resultado idêntico; validar pelo checksum de vértices da bancada (`verts=`).

**D5 — Lista visível compartilhada** (ganho marginal, ~0,1-0,2 ms; só se o `ZZ_TRACE` mostrar `drawChunks` relevante): calcular uma vez por vista (câmera e luz) a lista de chunks visíveis com os bits "passe não vazio", e reutilizá-la nos 3 passes (água na ordem inversa).

**D6 — Billboards** (só se `s.rend` apontar): trocar 6 vértices×9 floats por quad por 4 vértices + índices (−33% de dados), `reserve` do vetor.

### 5.3 Otimizações exatas do gerador (opcionais, Fase E; só com checksum idêntico)

- Calcular `secondaryWeight` e o ruído do rio **uma vez** por coluna e passá-los a `naturalHeight` (hoje `column()` chama `secondaryWeight` e depois `naturalHeight` chama de novo; o ruído do rio é calculado em `riverMask` e de novo em `naturalHeight`): ~5 de ~26 oitavas por coluna (~19%). Mesma função, mesmos argumentos ⇒ mesma saída. Assinaturas internas novas `naturalHeightWith(float x, float z, float secondaryWeight, float riverNoise)`.
- `chooseLayout`/`poi()` pré-populados (útil também para threading).

Qualquer mudança que **altere** o terreno (interpolar ruído em grade grossa, por exemplo) é proibida sem decisão explícita: afeta seeds compartilháveis do spec.

### 5.4 Contingência de qualidade

Mantida como está (escala de resolução 60/80/100% + `viewRadius_` 6/8/10 + cinematic). O diagnóstico expõe `flags` bit2 (degradação sustentada: p95 > 25 ms por ≥ 10 s) para que a tela de Ajustes/HUD possa *sugerir* reduzir a qualidade no futuro. Não há troca automática de qualidade neste plano.

### 5.5 GPU-bound: como saber sem ferramentas de GPU

`gap = int.avg - work.avg` ≥ 5 ms de forma estável ⇒ o quadro espera o swap/GPU. Confirmações baratas: rodar o mesmo cenário com qualidade Baixa (60% da resolução): se `int.avg` cai quase proporcionalmente à área (0,6² = 36%), é fill-rate. Opcional (Fase E, E4): `GL_EXT_disjoint_timer_query` (anel de 3 queries, lê o resultado do quadro anterior, log `gpu.ms`), só se a extensão existir.

### 5.6 Tabela sintoma → ação

| Sintoma no log | Causa provável | Ação |
|---|---|---|
| `int.p95` ≈ 16,7, `work.avg` < 10, `slow33` ≈ 0 | saudável | nada |
| `int.avg` > 17,5 e `work.avg` < 10 (gap ≥ 5 ms) | GPU/fill-rate/composição | reduzir escala (Q0), desligar cinematic, verificar `dc.*`; não mexer em streaming |
| `work.avg` > 14 | CPU | olhar `s.*`: `rend` alto ⇒ draw calls (`dc.*`), D1/D2; `upd` alto ⇒ zumbis/nav/minimapa |
| Hitches isolados com `top=str` | streaming | `gen.max`/`mesh.max`, `pend`, `str.max`; ajustar `maxMs`; checar molde de POI (§3.7) |
| Hitch a cada ~0,3 s com `top=nav` | `nav_.compute` | §5.7 |
| Hitch a cada ~5 s com `top=mini` | `rebuildMinimap` | §5.7 |
| `top=upl` com `up.max` alto | upload em rajada | §3.6, contrapressão |
| `pend.max` > 24 e `str` já no teto | orçamento insuficiente | gatilhos G2-G4 do §4.3 |
| `chunks`/`gpuMB` crescem sem parar | vazamento de chunks/VBO | checar descarte (`unl.ps` ≈ `gen.ps` em regime) |

### 5.7 Custos periódicos fora do streaming (medir antes de mexer)

- `nav_.compute(world_, px, pz, 60)` (game.cpp:411-418): 128×128 colunas + BFS, recalculado quando o jogador muda de coluna e `navTimer_ <= 0` (0,3 s). Se `sm.nav` ≥ 4 ms: aumentar o intervalo para 0,45 s, recalcular só quando andou ≥ 3 colunas, reduzir `kWindow` para 96 e/ou `maxDistance` para 45.
- `rebuildMinimap()` (game_mission.cpp:375): 25 600 colunas de uma vez. Se `sm.mini` ≥ 4 ms: gerar em fatias de 20 linhas por quadro com cursor e emitir `EV_MINIMAP` só quando completar; o `Bitmap` do Kotlin (160×160) continua igual.
- `buildBigMap()` (256² `mapColor`) roda uma vez na carga: se `load.bigmap` for > 25% da carga, reduzir para 192² ou calcular em duas passadas (decisão posterior, área de HUD/mapa).

---

## 6. Cenário reproduzível e medição sem emulador

### 6.1 Viabilidade

| Pergunta | Resposta |
|---|---|
| Dá para rodar o motor "headless" no PC? | **Não agora.** O código de mundo (`world/*`, `core/random.h`, `core/math.h`) é portátil, mas não há compilador C++ de host instalado (`g++`, `clang`, `cl` ausentes; o NDK só gera binários Android; Visual Studio sem `cl.exe`). Nenhum teste nativo roda no PC. `core/perf.h` fica livre de Android (`ZZ_PERF_HOST`) para que, se um dia houver `g++`, um alvo de host seja trivial. |
| Dá para rodar "headless" no aparelho? | **Sim.** `World` + `IslandGenerator` não dependem de GL. A *bancada* (§6.3) roda por JNI na thread GL, num `World` privado, sem renderizar. |
| Dá para repetir uma rota no jogo real? | **Sim.** O *autopiloto* (§6.2) dirige o jogador por uma rota derivada da ilha (início → ninhos → portal), com `dt` fixo. |
| Funciona sem aparelho conectado? | Não. `adb devices -l` está vazio neste momento. Toda medição exige o aparelho (USB ou `adb connect ip:porta`). Sem aparelho, o implementador só valida compilação e testes Kotlin. |

### 6.2 Autopiloto (`Cfg.PERF`)

Objetivo: mesma trajetória em todo build, para comparar `int.*`/`work.*`/`s.*` de ponta a ponta com GL, zumbis e HUD.

Arquivos: `game/perf_scenario.h` (struct de estado) e `game/game_perf.cpp` (métodos de `Game`), para não inchar `game.cpp`.

```cpp
// game/perf_scenario.h
namespace zz::perf {
struct AutopilotState {
    bool active = false;
    int mode = 0;               // 1 sem inimigos, 2 com inimigos
    int frame = 0, frames = 7200;   // 2 min simulados a 60 fps
    size_t leg = 0;
    std::vector<Vec2> route;    // pontos (x, z)
    Vec3 lastPos;
    char label[16] = "auto";
};
}
```

Em `Game` (game.h): `perf::AutopilotState autopilot_;` e métodos privados `void perfBegin();`, `void perfDrive(InputState& in);`, `void perfEnd();`.

- **`perfBegin()`** (chamado no fim de `startMission` quando `config.perfMode != 0` e `config.mode != 1`): monta `route` = `island_->start()` → ninhos na ordem de `island_->nests()` (centro do POI) → portal; chama `prof_.startRun("auto1" ou "auto2")`; liga `autopilot_.active`. Em modo 1 (sem inimigos) marca `noEnemies_ = true`.
- **Sem inimigos (modo 1):** `spawnZombie` retorna imediatamente (1 linha em game_zombies.cpp) enquanto `noEnemies_`; `updateSpawner` e `updatePois` continuam (não geram nada). Isola streaming/renderer de IA.
- **`perfDrive(in)`** (em `updateMission`, logo após `consumeInput(in)`): 

```cpp
void Game::perfDrive(InputState& in) {
    auto& a = autopilot_;
    ++a.frame;
    invulnerable_ = std::max(invulnerable_, 1.0f);     // nada de morte durante o teste
    health_ = maxHealth_;
    while (a.leg < a.route.size() && distXZ(player_.position, Vec3(a.route[a.leg].x, 0, a.route[a.leg].y)) < 6.0f) ++a.leg;
    if (a.leg >= a.route.size()) a.leg = 0;            // rota circular
    const Vec2 t = a.route[a.leg];
    const float want = yawFromDirection(t.x - player_.position.x, t.y - player_.position.z);
    camYaw_ = wrapAngle(camYaw_ + clampf(wrapAngle(want - camYaw_), -3.0f / 60.0f, 3.0f / 60.0f));   // vira a 3 rad/s
    camPitch_ = -0.12f;
    in.moveX = 0; in.moveY = 1;                        // correr para frente = 6,5 m/s (pior caso de streaming)
    in.lookX = in.lookY = 0; in.fire = false; in.aim = false; in.aimStage = 0;
    if (a.frame % 120 == 0) {                          // preso (água/penhasco)? pula o ponto
        if (distXZ(player_.position, a.lastPos) < 2.0f) ++a.leg;
        a.lastPos = player_.position;
    }
    if (a.frame >= a.frames) perfEnd();
}
```

- **`perfEnd()`**: `prof_.endRun()`; loga `ZZPerf run done ...` incluindo `hash=` (`world_.residentHash(px, pz, streamRadius())` após 0 pendências) e `starved=`; `autopilot_.active = false`; `noEnemies_ = false`. O jogo segue normal (o jogador pode sair).
- `starved`: contagem de quadros em que algum dos 9 chunks 3×3 em volta do jogador não estava gerado (`!world_.columnReady`), verificado em `perfDrive`. **Deve ser 0.**
- Determinismo: `dt` fixo (já no `frame`), `rng_` semeado pela seed, sem toque, sem inimigos (modo 1). A trajetória só depende do código de física.

Kotlin: `Cfg.PERF` e `Cfg.PERF_FRAMES` (§2.6), preenchidos por `PerfRun.applyTo(config)` (§6.5).

### 6.3 Bancada headless de streaming (`NativeBridge.perfBench`)

Mede **só CPU de mundo** (sem GL, sem zumbis, sem HUD): determinística em conteúdo e A/B entre políticas no mesmo aparelho, na mesma sessão térmica.

Arquivos: `game/perf_bench.h`, `game/perf_bench.cpp` (inclui os autotestes do §8).

```cpp
// game/perf_bench.h
namespace zz::perf {
struct BenchConfig {
    uint32_t seed = 1234567;   // < 16 777 216 (limite do FloatArray do Kotlin, comentário em MainActivity)
    int stage = 2;             // 2 = Floresta; 16 = Terra Devastada (névoa curta)
    int radius = 8;            // raio de malha em chunks (passar streamRadius() equivalente)
    int meters = 900;
    float speedMps = 6.5f;
    int policy = 2;            // 0 legado, 1 orçamento, 2 os dois em sequência
    float assumedOtherMs = 8.0f;   // trabalho "do resto do quadro" assumido no orçamento adaptativo
    bool selfTest = true;
};
int runBench(const BenchConfig& cfg);   // 0 = ok; loga tudo na tag ZZPerf
}
```

Algoritmo de `runBench` por política:

1. `Theme theme = applySkyVariant(themeForStage(stage), seed, stage >= 3)` (igual a `startMission`); `IslandGenerator island(theme, stage, seed, true)`; `World world; world.resetStreaming(kIslandSize, kIslandHeight, kIslandSize, &island); world.enableContentHash(true); world.setTrace(&trace);`.
2. Carga inicial como `startMission`: `for (i < 500) if (world.stream(sx, sz, radius, StreamBudget::bulk()).pending == 0) break;` → `init.ms`.
3. Rota: `island.start()` → `island.nests()` na ordem → portal; percorrer em linha reta entre pontos a `speedMps` com `dt = 1/60` até `meters` (reinicia invertida se acabar). Mover é cinemático (sem física): o streaming depende só da posição.
4. A cada quadro simulado: escolher o orçamento — política 0: `legacy(pending > 24 ? 4 : 2, pending > 24 ? 6 : 3)` com `pending` da chamada anterior (replica o código antigo, inclusive o `static`); política 1: `maxMs = clamp(16,7 - assumedOtherMs - 3, 1, 5)`, demais campos padrão. Medir `ms = nowMs()` antes/depois da chamada a `world.stream`. Registrar `ms`, `pending`, `generated`, `meshed`.
5. Verificar `starved`: algum dos 9 chunks 3×3 em volta da posição não gerado após a chamada.
6. Ao fim: 200 quadros extras parado (assentar), calcular `hash = world.residentHash(x, z, radius)` e `verts` (soma dos vértices de todos os chunks malhados, via `trace` ou contador no `World`).
7. Logar o resultado:

```
ZZPerf bench begin seed=1234567 stage=2 radius=8 meters=900 speed=6.5
ZZPerf bench result policy=legacy frames=8308 init.ms=1420 gen=1789 mesh=1512 verts=48210316 hash=0x9f3a21c5d0e4b7aa starved=0 pend.max=31 str.p50=0.02 str.p95=6.8 str.p99=10.2 str.max=17.5 over4=210 over8=48 over16=3 gen.avg=1.9 gen.p95=3.4 gen.max=11.8 mesh.avg=1.3 mesh.p95=2.6 mesh.max=6.0 total.ms=4920
ZZPerf bench result policy=budget frames=8308 ... hash=0x9f3a21c5d0e4b7aa ...
ZZPerf bench compare hashEqual=1 str.p99.delta=-5.0 over8.delta=-44
ZZPerf selftest ring_percentile PASS
...
ZZPerf bench done
```

`str.*` é o custo por *chamada de quadro* de `world.stream` (a contribuição de streaming ao quadro); `over4/over8/over16` contam quadros acima de 4/8/16 ms. `hashEqual=1` é o critério de que o orçamento não muda o mundo.

JNI (`jni_bridge.cpp`):

```cpp
JNIEXPORT jint JNICALL ZZ_FN(perfBench)(JNIEnv*, jobject, jint seed, jint stage, jint radius, jint meters, jint policy, jint flags) {
    zz::perf::BenchConfig c;
    c.seed = static_cast<uint32_t>(seed); c.stage = stage; c.radius = radius; c.meters = meters;
    c.policy = policy; c.selfTest = (flags & 1) != 0;
    return zz::perf::runBench(c);
}
```

`NativeBridge.kt`: `external fun perfBench(seed: Int, stage: Int, radius: Int, meters: Int, policy: Int, flags: Int): Int` (chamar dentro de `engine { }` para rodar na thread GL; bloqueia o desenho do lobby por alguns segundos; é esperado).

Duração estimada: ~4-10 s por política (≈ 1,8 mil chunks × 3 ms) num intermediário.

### 6.4 Prova de que o orçamento não altera o mundo

`World::residentHash(x, z, radius)` soma `hashChunk` (FNV-1a de `blocks` + `cx,cz`) de todo chunk **gerado** dentro do raio após assentar. Como a geração é função pura de (seed, posição do chunk), o valor tem que ser idêntico entre: política legado, política orçamento, bancada e autopiloto na mesma seed/posição final, e entre builds. Registrar o hash do baseline em `docs/superpowers/plans/2026-10-18-expeditions/04-desempenho.md` (Apêndice B). Qualquer divergência é regressão (ou mudança deliberada do gerador, que exige decisão — ver §1.5 item 8).

### 6.5 Gancho de depuração no Kotlin (somente `BuildConfig.DEBUG`)

Arquivo novo `engine/PerfRun.kt` (puro, testável em JVM):

```kotlin
package com.fabiosilva.ecosdeeldra.engine

/** Parâmetros de diagnóstico de desempenho, lidos de extras de Intent (somente build debug). */
data class PerfRun(
    val mode: Int,            // 1 autopiloto sem inimigos, 2 autopiloto com inimigos, 3 bancada headless
    val stage: Int = 2,
    val seed: Int = 1_234_567,        // < 2^24: cabe exato no FloatArray da configuração
    val frames: Int = 7200,
    val quality: Int = -1,            // -1 mantém o ajuste salvo
    val cinematic: Int = -1,
    val zombies: Int = -1,
    val meters: Int = 900,
    val policy: Int = 2,
    val flags: Int = 1,
) {
    fun applyTo(config: FloatArray) {
        config[Cfg.SEED] = seed.toFloat()
        config[Cfg.PERF] = if (mode == MODE_BENCH) 0f else mode.toFloat()
        config[Cfg.PERF_FRAMES] = frames.toFloat()
        if (quality >= 0) config[Cfg.QUALITY] = quality.toFloat()
        if (cinematic >= 0) config[Cfg.CINEMATIC] = cinematic.toFloat()
        if (zombies >= 0) config[Cfg.ZOMBIES] = zombies.toFloat()
    }

    companion object {
        const val MODE_AUTO_CLEAN = 1
        const val MODE_AUTO_ENEMIES = 2
        const val MODE_BENCH = 3

        /** Lê extras `zz_perf`, `zz_stage`, ...; null se `zz_perf` ausente/zero ou inválido. */
        fun parse(getInt: (String, Int) -> Int): PerfRun? {
            val mode = getInt("zz_perf", 0)
            if (mode !in 1..3) return null
            return PerfRun(
                mode = mode,
                stage = getInt("zz_stage", 2).coerceIn(1, 60),
                seed = getInt("zz_seed", 1_234_567).coerceIn(1, 16_000_000),
                frames = getInt("zz_frames", 7200).coerceIn(600, 36_000),
                quality = getInt("zz_quality", -1).coerceIn(-1, 2),
                cinematic = getInt("zz_cinematic", -1).coerceIn(-1, 1),
                zombies = getInt("zz_zombies", -1).coerceIn(-1, 60),
                meters = getInt("zz_meters", 900).coerceIn(100, 5000),
                policy = getInt("zz_policy", 2).coerceIn(0, 2),
                flags = getInt("zz_flags", 1),
            )
        }
    }
}
```

`MainActivity.kt` (arquivo quente com alterações do working tree: aplicar um patch mínimo e reler antes):

```kotlin
private var perfRun: PerfRun? = null

// fim de onCreate, depois de showHome():
if (BuildConfig.DEBUG) {
    perfRun = PerfRun.parse { key, def -> intent.getIntExtra(key, def) }
    perfRun?.let { run -> surface.postDelayed({ startPerfRun(run) }, 1500) }
}

private fun startPerfRun(run: PerfRun) {
    if (run.quality >= 0) surface.setQuality(run.quality)     // escala de resolução vive no Kotlin
    if (run.mode == PerfRun.MODE_BENCH) {
        perfRun = null
        engine { NativeBridge.perfBench(run.seed, run.stage, 8, run.meters, run.policy, run.flags) }
        return
    }
    survivalMode = false
    missionStage = run.stage
    launchMission()   // NÃO usar beginStage: ele conta o toque em JOGAR e pode exibir o intersticial a cada 5º toque
}

// em launchMission(), imediatamente antes de 'val inventory = p.inventory.toIntArray()':
perfRun?.applyTo(config)
perfRun = null        // vale só para esta partida
```

Nenhuma mudança em AdMob. Os anúncios continuam carregando em segundo plano durante a medição (SDK de anúncios pode causar picos de CPU): para isolar, rode com USB e rede de dados/Wi-Fi do aparelho desligada.

### 6.6 Procedimento de medição antes/depois

Pré-requisitos: aparelho conectado (`adb devices -l` lista o serial). Windows PowerShell:

```powershell
$adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
& $adb devices -l
```

Sem aparelho: não há como medir. Wi-Fi: `adb tcpip 5555` (uma vez por USB) e `adb connect IP:5555`.

**Passo 0 — baseline grosseiro agora, sem alterar nada:** instalar o APK debug atual, jogar 2-3 minutos e capturar `& $adb logcat -d -s ZombieZoneNative:I | Select-String "FPS"` (linha a cada 5 s: fps suavizado, chunks, zumbis, partículas).

**Compilar (um Gradle por vez):**

```powershell
.\gradlew.bat :app:assembleDebug --no-daemon --max-workers=1 "-Pandroid.injected.build.abi=arm64-v8a"
& $adb install -r app\build\outputs\apk\debug\app-debug.apk
```

**Condições de medição** (anotar no JSON): mesmo aparelho; bateria > 50% e desconectado do carregador após a instalação; brilho fixo; rede desligada; pausa de 2 min entre execuções (esfriar); registrar `adb shell dumpsys battery | findstr temperature` e `adb shell dumpsys thermalservice | findstr Status` antes e depois; 3 execuções por configuração, descartar a primeira, usar a mediana das outras.

**Execuções (matriz mínima):**

| Id | Intent | Para quê |
|---|---|---|
| R1 | `--ei zz_perf 1 --ei zz_stage 2` | autopiloto limpo, Floresta, qualidade salva |
| R2 | R1 + `--ei zz_quality 0` e `--ei zz_quality 2` | sensibilidade a resolução/raio |
| R3 | R1 + `--ei zz_cinematic 1` | custo do modo cinematográfico |
| R4 | `--ei zz_perf 1 --ei zz_stage 16` | névoa curta (valida o raio efetivo) |
| R5 | `--ei zz_perf 2 --ei zz_stage 8 --ei zz_zombies 30` | estresse com horda |
| B1 | `--ei zz_perf 3 --ei zz_stage 2` e `--ei zz_stage 16` | bancada A/B legado vs orçamento |

Comando-modelo (mata o processo antes para garantir `onCreate` com extras):

```powershell
& $adb logcat -c
& $adb shell am start -S -n com.fabiosilva.ecosdeeldra/.MainActivity --ei zz_perf 1 --ei zz_stage 2 --ei zz_seed 1234567
& $adb logcat -s ZZPerf:I
```

**Script `scripts/zzperf.mjs`** (Node, sem dependências; o repositório já tem `scripts/*.mjs`): automatiza o acima.

```
node scripts/zzperf.mjs run     --label baseline --perf 1 --stage 2 [--quality 1] [--cinematic 0] [--zombies -1] [--install] [--runs 3]
node scripts/zzperf.mjs bench   --label baseline --stage 2
node scripts/zzperf.mjs live                      # tail formatado de ZZPerf
node scripts/zzperf.mjs compare perf-results/baseline-*.json perf-results/depois-*.json
```

Comportamento: localiza `adb` por `ANDROID_HOME`/`local.properties` (`sdk.dir`) ou `%LOCALAPPDATA%\Android\Sdk\platform-tools`; aborta com mensagem clara se `adb devices` vier vazio; `logcat -c`; `am start -S ...`; espera a linha `ZZPerf run done` (ou `ZZPerf bench done`) com timeout de 4 min; `logcat -d -s ZZPerf:I`; parseia com `([\w.]+)=(\S+)`; grava `perf-results/<label>-<AAAAMMDD-HHMMSS>.json` com `{device: ro.product.model, soc: ro.soc.model, android, battery, thermal, args, run: {...}, windows: [...], hitches: [...], load: {...}, bench: [...]}`; `compare` imprime tabela com mediana por métrica e delta %, marcando regressão (> +10% em p95 ou > 2× em máx). Adicionar `perf-results/` ao `.gitignore`.

**Verificações cruzadas independentes do app (opcionais):**

- Apresentação real do `SurfaceView` (inclui GPU/composição): `& $adb shell dumpsys SurfaceFlinger --list | findstr ecosdeeldra`, depois `& $adb shell dumpsys SurfaceFlinger --latency "<nome exato listado>"` (127 últimos quadros; o nome varia com a versão do Android, ex.: sufixo `(BLAST)`).
- Lado Kotlin/HUD (HWUI, thread de UI): `& $adb shell dumpsys gfxinfo com.fabiosilva.ecosdeeldra reset`, jogar, `... gfxinfo com.fabiosilva.ecosdeeldra framestats` (jank do `HudView`, não do GL).
- Perfetto (com os `ZZ_TRACE_SCOPE` do §2.7): `& $adb shell perfetto -o /data/misc/perfetto-traces/zz.pftrace -t 20s sched freq gfx view -a com.fabiosilva.ecosdeeldra`, `& $adb pull /data/misc/perfetto-traces/zz.pftrace`, abrir em `ui.perfetto.dev` (arquivo local).

### 6.7 Metas (calibrar com o baseline; regressão = pior que +10% em p95 ou 2× em máximo)

Aparelho de referência: o aparelho do usuário, qualidade Média (padrão), `cinematic` desligado, autopiloto R1/R5.

- Quadros > 33 ms (`slow33`) ≤ 0,5% do total em R5; sem piora em R1.
- `int.p95` ≤ 20 ms (no máximo um vsync perdido a 60 Hz) em R1; sem piora vs. baseline em R3/R5.
- Streaming: `str.p99` ≤ 5 ms e `str.max` ≤ 14 ms (urgente incluído) em regime; nenhum quadro de regime com `top=str` acima de 10 ms.
- `starved = 0`; `hashEqual = 1`.
- `pend` volta a < 4 em ≤ 2 s depois de cruzar uma fronteira a 6,5 m/s.
- Carga (`load total`) não pior que +10% do baseline.
- R4: `gen.ps` e `up.ps` ≥ 15% menores que em R1-equivalente (raio 7 vs 8) e nenhum buraco visível no horizonte (inspeção visual com capturas de tela).

---

## 7. Plano de implementação (ordem obrigatória)

### Fase A — Instrumentação (nenhuma mudança de comportamento)

| Passo | Arquivos | Entrega |
|---|---|---|
| A1 | `core/perf.h`, `core/perf.cpp`, `CMakeLists.txt` | `FrameProfiler`, `FrameGuard`, `Scoped`, `percentile`, macros de trace. Compila para arm64 e armv7 (`-Wall -Wextra` sem warnings novos). |
| A2 | `world/world.h`, `world/world.cpp` | `StreamStats`, `StreamBudget` (+`legacy`/`bulk`), `StreamTrace`, `ChunkGpu::bytes`; `World::stream(…, const StreamBudget&)` com a **política legada ainda ativa** e a sobrecarga antiga delegando. `resetStreaming` zera EMAs. |
| A3 | `game/game.h`, `game/game.cpp`, `game/game_mission.cpp` | Membros `prof_`, `streamStats_`; `streamWorld()` sem parâmetros e **sem `static`**, ainda com a regra antiga (2/3, dobra com `pending > 24` lido de `streamStats_`); `FrameGuard` e seções em `frame()`; remover `fpsLogTimer`/`LOGI("FPS")`; `skipFrames(3)` nas transições; laço bulk com `StreamBudget::bulk()`; medição de `Sec::Nav` e `Sec::Minimap`. |
| A4 | `render/renderer.h`, `render/renderer.cpp` | `RenderStats`, `UploadBudget` (ainda `unlimited = true`), contadores, `gpuBytes_`, `uploadChunk()` extraído do `syncChunks`. Sem mudança de ordem ou limite. |
| A5 | `game/protocol.h`, `engine/NativeProtocol.kt`, `game.cpp::publishHud` | Bloco `H_PERF_*` (650-659), `H_FPS` da janela, `C_PERF`/`C_PERF_FRAMES`/`C_COUNT = 30` e `Cfg.PERF*`/`Cfg.COUNT = 30`. |
| A6 | `game/game_mission.cpp` | Log `ZZPerf load` com `lap()` por etapa de `startMission`. |
| A7 | scopes `ZZ_TRACE_SCOPE` (§2.7) | Opcional nesta fase. |

**Aceitação da Fase A:** jogo idêntico; logcat mostra `ZZPerf win` a cada 5 s e `ZZPerf load` por missão; `H_FPS` não difere do FPS real em mais de 2; `work.avg` não sobe > 0,1 ms; testes Kotlin do §8 passam; `git diff` não toca `HudView.kt`.

### Fase B — Cenário reproduzível e bancada

| Passo | Arquivos | Entrega |
|---|---|---|
| B1 | `world.cpp/.h` | `enableContentHash`, `hashChunk`, `residentHash`, `setTrace`. |
| B2 | `game/perf_bench.h/.cpp`, `jni_bridge.cpp`, `NativeBridge.kt`, `CMakeLists.txt` | `runBench` (política 0 e 1) + autotestes. |
| B3 | `game/perf_scenario.h`, `game/game_perf.cpp`, `game.h`, `game.cpp`, `game_zombies.cpp` (1 linha) | Autopiloto, `starved`, `hash` no `run done`. |
| B4 | `engine/PerfRun.kt`, `MainActivity.kt` | Gancho debug por extras de Intent. |
| B5 | `scripts/zzperf.mjs`, `.gitignore` | Automação e `compare`. |

**Portão (humano):** o usuário (ou o implementador, com o aparelho conectado) roda a matriz R1-R5 e B1 **no código da Fase B, ainda com a política legada** e grava os números no Apêndice B. Nenhum parâmetro das Fases C/D é fixado antes disso. Se não houver aparelho, as Fases C/D podem ser implementadas, mas ficam marcadas "não medidas" no relatório.

### Fase C — Orçamento

| Passo | Arquivos | Entrega |
|---|---|---|
| C1 | `world.cpp` | Corpo novo de `stream` (§3.3): urgência, piso, orçamento, teto; `timeCapped`; `pendingUrgent`. |
| C2 | `game_mission.cpp` | `streamWorld()` adaptativo (§3.4) + `ZZ_STREAM_LEGACY`; `streamRadius()` (§3.5); contrapressão por `pendingUploads()`. |
| C3 | `renderer.cpp/.h`, `game.cpp` | `syncChunks` com `UploadBudget` por distância (§3.6); `uploadUnlimitedFrames_`. |
| C4 | `island.h`, `game_mission.cpp` | `hasStamp` e pré-aquecimento de moldes (§3.7). |
| C5 | `world.cpp` | `buildChunk` com `scratch_` (D4). |

**Aceitação da Fase C:** §9 (A1-A6); bancada `hashEqual=1`; `str.p99` ≤ 5 ms; sem buraco visível em R4 com raio 7.

### Fase D — Renderer de baixo risco (cada item é um commit; medir R1/R3/R5 antes e depois)

D1 cache de uniformes; D2 culling de personagens/caixas (+ normalizar planos); D5 lista visível compartilhada e D6 billboards só se o perfil indicar.

### Fase E — Condicional (só se um gatilho do §4.3/§5.6 disparar)

E1 espalhar `rebuildMinimap` e reduzir `nav_.compute` (§5.7); E2 otimizações exatas do gerador (§5.3, hash idêntico); E3 fatiar `generate()` (G4); E4 GPU timer queries; E5 thread (G1/G2/G5, com a tabela do §4.2 como checklist).

---

## 8. Testes

Sem emulador e sem compilador de host, a estratégia é: **testes de unidade Kotlin na JVM** para tudo que é Kotlin e para o espelho do protocolo; **autotestes nativos executados no aparelho** pela bancada (`flags & 1`), com saída `PASS/FAIL` no logcat.

### 8.1 Kotlin (JUnit 4, `.\gradlew.bat :app:testDebugUnitTest --no-daemon --max-workers=1`)

- `app/src/test/java/com/fabiosilva/ecosdeeldra/NativeProtocolMirrorTest.kt`
  - `perfBlockMatchesHeader`: lê `src/main/cpp/game/protocol.h` (diretório de trabalho do módulo `app/`), extrai com regex `\bH_PERF_FPS\s*=\s*(\d+)` (e demais `H_PERF_*`, `H_SIZE`, `C_PERF`, `C_PERF_FRAMES`, `C_COUNT`) e compara com `Hud.PERF_*`, `Hud.SIZE`, `Cfg.PERF`, `Cfg.PERF_FRAMES`, `Cfg.COUNT`.
  - `perfBlockFitsInsideHudArray`: `Hud.PERF_FLAGS < Hud.SIZE` e nenhum índice `PERF_*` colide com outro índice `Hud.*` declarado por reflexão (`Hud::class.java.fields`, ignorando `SIZE` e `*_STRIDE`/`*_COUNT`).
- `app/src/test/java/com/fabiosilva/ecosdeeldra/PerfRunTest.kt`
  - `parseReturnsNullWhenDisabled`: sem `zz_perf` ⇒ `null`; `zz_perf = 0` ou `4` ⇒ `null`.
  - `parseClampsAndDefaults`: seed `20_000_000` vira `16_000_000`; `zz_stage = 0` vira 1; `zz_frames = 1` vira 600.
  - `applyToWritesSeedAndMode`: para modo 1, `config[Cfg.PERF] == 1f`, `config[Cfg.SEED] == seed.toFloat()`, `Cfg.QUALITY` só muda se `quality >= 0`.
  - `benchModeDoesNotEnableAutopilot`: modo 3 ⇒ `config[Cfg.PERF] == 0f`.
- Conferir que `RulesTest.kt`/`InventoryTest.kt` continuam verdes (não usam `Cfg.COUNT`; verificado por `grep`).

### 8.2 Autotestes nativos (rodam com `zz_perf 3 --ei zz_flags 1`; arquivo `game/perf_bench.cpp`)

Usam uma `ChunkSource` falsa (`FakeSource`: preenche pedra até y=10, custo ~0) e uma `SlowSource` (espera ocupada de 1,5 ms por chunk) para independência da ilha.

| Nome (linha do log `ZZPerf selftest <nome> PASS/FAIL`) | O que verifica |
|---|---|
| `ring_percentile` | `percentile()` em vetores conhecidos (1..100 ⇒ p95 = 95; n = 1; n = 0). |
| `profiler_window` | alimentar 400 intervalos sintéticos (16,7 ms com 4 picos de 80 ms) em um `FrameProfiler`: `slow50 == 4`, `intMax == 80`, `n == 300` (anel). |
| `profiler_gap_ignored` | intervalo de 900 ms não entra na janela e incrementa `gaps`. |
| `budget_legacy_counts` | `legacy(2,3)` com `FakeSource`: nunca gera > 2 nem malha > 3 por chamada. |
| `budget_floor_progress` | `maxMs = 0`, `urgentRadius = -1`, posição afastada: cada chamada gera ≥ 1 e malha ≥ 1 enquanto houver pendência. |
| `budget_urgent_ignores_time` | `maxMs = 0`, `urgentRadius = 1`: após 1 chamada os 25 chunks do anel 5×5 estão gerados (`FakeSource`) e os 9 centrais malhados. |
| `budget_urgent_cap` | `SlowSource` (1,5 ms) e `urgentMaxMs = 6`: gera ≤ 5 por chamada e conclui o 5×5 em ≤ 6 chamadas. |
| `budget_time_cap` | `SlowSource`, `maxMs = 3`, sem urgência: ≤ 3 gerações por chamada (piso 1 + previsão). Tolerância de ±1 por jitter. |
| `budget_completes_equal_legacy` | rodar até `pending == 0` com política legada e com orçamento no mesmo ponto: `residentHash` igual. |
| `no_starvation_route` | rota curta (300 m) da ilha real a 6,5 m/s: `starved == 0` nas duas políticas. |
| `hash_deterministic` | duas gerações independentes da mesma seed ⇒ mesmo `residentHash`. |

### 8.3 Diagnósticos de integração (manuais, por log)

- `run done` com `hash` idêntico em duas execuções R1 consecutivas e entre builds.
- `ZZPerf win` contém todos os campos do dicionário do §2.5 (conferir por regex no `zzperf.mjs`; falha se faltar).

---

## 9. Critérios de aceitação

| Id | Critério | Fase |
|---|---|---|
| A1 | `adb logcat -s ZZPerf:I` mostra `win` a cada ~5 s com `int.*`, `work.*`, `s.*`, `gen.ps`, `up.ps`, `pend`, `dc.*`. | A |
| A2 | `hitch` nomeia a seção culpada (`top=`) e não passa de 4 linhas/s. | A |
| A3 | `static int pending` e `fpsLogTimer` removidos; `H_FPS` vem da janela. | A |
| A4 | `HudView.kt` intacto; HUD de FPS mostra valor coerente. | A |
| A5 | Autopiloto R1 termina com `run done`; duas execuções seguidas dão o mesmo `hash` e `starved=0`. | B |
| A6 | Bancada imprime as duas políticas e `hashEqual=1`. | B/C |
| A7 | Overhead da instrumentação ≤ 0,1 ms em `work.avg`. | A |
| A8 | Orçamento: `str.p99` ≤ 5 ms; sem quadro de regime acima de 10 ms atribuído a streaming; `slow33` ≤ 0,5% em R5. | C |
| A9 | Urgência: teleporte/queda para o acampamento não faz o jogador cair no vazio (`starved = 0` e nenhuma ocorrência de teleporte repetido por queda). | C |
| A10 | Raio efetivo: Terra Devastada usa raio 7 e não há buracos visíveis. | C |
| A11 | APK compila para `arm64-v8a` e `armeabi-v7a`; testes Kotlin passam. | todas |
| A12 | Nenhuma alteração em IDs AdMob/`ADMOB_TEST_DEVICES`. | todas |

---

## 10. Riscos e mitigações

| Risco | Mitigação |
|---|---|
| Orçamento por tempo é não determinístico (cronologia dos chunks muda entre execuções) | O conteúdo final não muda (função pura); validar por `residentHash`, não por ordem; autopiloto com `dt` fixo. |
| Previsão por EMA subestima unidades "outlier" (molde de POI) | §3.7 pré-aquece moldes; o teto de 4 gerações e o piso limitam; fatiar `generate()` só se G4 disparar. |
| Piso 1+1 por quadro num aparelho já estourado adiciona 2-4 ms nas rajadas | Aceito: garante progresso e é curto (13-21 quadros); a faixa distante é invisível. Ajustável por `minGenerate/minMesh`. |
| Urgência pode somar até 12 ms num quadro (teleporte) | Teto `urgentMaxMs`; evento raro (queda, reviver, desencalhar). |
| Culling por esfera remove sombra/objeto visível | Raio com folga (2,0×escala); sombras testadas contra o volume da luz; contador `cull` no log para auditar; sem culling quando `boundRadius == 0`. |
| Cache de uniformes por ponteiro quebra se alguém passar string dinâmica | Comentário no header; `static_assert`/revisão: todas as chamadas atuais são literais. |
| Instrumentação vira ruído de logcat | Uma linha a cada 5 s + hitch limitado; tag única filtrável. |
| Perda de contexto GL durante o orçamento de upload | Mantém o ramo atual (`gpu = ChunkGpu{}`, `dirty = true`); `uploadQueue_` é recalculada a cada quadro. |
| Fatores de edição concorrente em arquivos quentes (`MainActivity.kt`, `protocol.h`, `NativeProtocol.kt`, `game.h`) | Patches pequenos e localizados; reler o arquivo antes de editar; blocos novos em faixas livres (650-659). |
| Chave de emergência | `-DZZ_STREAM_LEGACY` reverte o streaming; o restante é aditivo. |
| Float diferente entre ABIs (arm64 vs armv7) quebra "mesma ilha entre amigos" | Fora do escopo desta área; registrar para a área de contratos (flag `-ffp-contract=off` + checksum por ABI). Otimizações do gerador só se idênticas. |
| Medição sem aparelho | Documentar "não medido"; nada de números inventados nos relatórios. |
| Hooks de depuração em release | `BuildConfig.DEBUG`; o `perfBench` JNI existe no `.so` de release mas só é chamável por Kotlin que não o referencia em release. |

---

## 11. Interfaces e arquivos quentes

**Cria:** `core/perf.h`, `core/perf.cpp`, `game/perf_bench.h`, `game/perf_bench.cpp`, `game/perf_scenario.h`, `game/game_perf.cpp`, `engine/PerfRun.kt`, `NativeProtocolMirrorTest.kt`, `PerfRunTest.kt`, `scripts/zzperf.mjs`.

**Modifica:** `CMakeLists.txt`, `world/world.h`, `world/world.cpp`, `world/island.h`, `render/renderer.h`, `render/renderer.cpp`, `render/gl_util.h`, `game/game.h`, `game/game.cpp`, `game/game_mission.cpp`, `game/game_zombies.cpp` (1 linha), `game/protocol.h`, `jni_bridge.cpp`, `engine/NativeBridge.kt`, `engine/NativeProtocol.kt`, `MainActivity.kt`, `.gitignore`.

**Compartilhados com outras áreas (conflito provável):** `MainActivity.kt` (gancho debug e `launchMission`), `protocol.h` e `NativeProtocol.kt` (faixa H_PERF 650-659 e `C_PERF*`), `game.h`/`game.cpp`/`game_mission.cpp` (membros e `startMission`), `renderer.h/.cpp`, `world.h`. Se outra área precisar de índices novos no HUD ou na configuração, deve usar as faixas 644-649 e a partir de 660/`C_COUNT = 30`.

**Contrato com 01-hud:** nenhum obrigatório; `Hud.PERF_*` disponíveis (§2.6).
**Contrato com contratos/expedições (etapa 4):** o raio de mundo deriva de `theme_.sky.fogEnd` (§3.5); modificadores como "noite vermelha" encolhem o streaming automaticamente; mais inimigos devem ser validados com R5.

---

## Apêndice A — Decisão pedida ao usuário

1. **Hooks de depuração no app:** aceitar o gancho por extras de Intent em `MainActivity` (somente `BuildConfig.DEBUG`) e as funções nativas de bancada/autopiloto, ou preferir somente a instrumentação passiva (Fase A) e medir jogando? (Recomendação: aceitar; o código fica fora de release.)
2. **Aparelho e meta:** informar o modelo/refresh do aparelho de referência e confirmar a meta "60 fps estáveis em qualidade Média" (ou "30 estáveis em Alta"). Isso calibra `kTargetFrameMs` e as metas do §6.7.

## Apêndice B — Resultados (preencher na Fase B/C)

| Métrica | Baseline (Fase B, legado) | Depois (Fase C) | Depois (Fase D) | Meta |
|---|---|---|---|---|
| Aparelho / Android / SoC | | | | |
| R1 `int.p95` / `int.max` | | | | ≤ 20 / — |
| R1 `work.avg` / `work.p95` | | | | — |
| R1 `slow33` (%) | | | | ≤ 0,5 |
| R5 `slow33` (%) | | | | ≤ 0,5 |
| R1 `str.p99` / `str.max` | | | | ≤ 5 / ≤ 14 |
| R1 `gen.ps` / `up.ps` | | | | — |
| R1 `dc.*` (chunks/chars/boxes) | | | | — |
| R3 (cinematic) `int.p95` | | | | sem piora |
| R4 `gen.ps` vs R1 | | | | −15% |
| B1 `str.p99` legado → orçamento | | | | −40% |
| B1 `hash` | | | | igual |
| Carga `load total` | | | | ≤ +10% |
| Hipóteses H1-H8 confirmadas? | | | | |
