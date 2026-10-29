# Terreno Horizonte Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reutilizar o gerador contínuo completo do Horizonte no Zombie Zone.

**Architecture:** Importar o núcleo C++ com equivalências de blocos locais.
Os geradores de missão/arena aplicam suas áreas seguras como sobreposições.
Mapas e consultas usam o mesmo núcleo; altura de 128 e estruturas com chave
de 64 bits preservam as ilhas flutuantes.

**Tech Stack:** C++17, Android NDK 28.2, Kotlin, Gradle e testes nativos MSVC.

**Spec:** docs/superpowers/specs/2026-10-24-terreno-horizonte-design.md

## Global Constraints

- Terreno contínuo dentro de 6144 por 6144 blocos; altura 128; água em y=16.
- Preservar fórmulas, precisão double e hash de 64 bits do Horizonte.
- Preservar alterações locais existentes e não editar Horizonte.
- Manter missões, contratos e combate; nenhum novo sistema econômico.
- Trabalhar no checkout atual: contém as alterações locais necessárias.
- Não criar commits com trabalho anterior do usuário; entregar alterações revisáveis.

## Review Focus

- Semente zero e coordenadas negativas no núcleo conservam a origem.
- Árvores, aldeias e estruturas nas bordas não apresentam cortes.
- Um teto flutuante não muda o chão inferior usado para spawn/navegação.
- Posições acima de y=63 e x/z altos não colidem na chave de estruturas.
- PvP recusa clientes com a versão antiga do gerador.

### Task 1: Núcleo importado e equivalências

**Files:** Create `app/src/main/cpp/world/horizonte/{terrain.h,terrain.cpp,terrain_noise.h,terrain_noise.cpp,terrain_grid.h,chunk_generator.cpp,village_generator.cpp,block_mapping.cpp}`; create `tests/native/terrain_tests.cpp`, `scripts/test-terrain.ps1`.

**Interfaces:** Produces `zz::horizonte::Terrain(uint64-compatible int64_t seed)`,
`groundHeight(int,int)`, `biome(int,int)`, `chunk(int,int)`, `villagesNear(int,int,int)`.

- [ ] Write/run native regression against existing generator: island height must be 128; record expected failure.
- [ ] Import source modules with the local block mapping; add them to Android CMake.
- [ ] Compare noise and translated chunks against an independently compiled original source namespace; cover zero, negative coordinates, tree/village/chunk boundaries.

### Task 2: Missões, ilhas, mapas e arenas

**Files:** Modify `world/island.{h,cpp}`, `world/terrain.{h,cpp}`, `world/world.cpp` only if integration requires it; add `world/horizonte_adapter.{h,cpp}` and `world/position_key.h`.

**Interfaces:** Consumes Task 1; produces first-free ground height, floating-column query,
and safe POI placement using original terrain; retains `IslandGenerator` public API.

- [ ] Add/run failing tests for actual floating blocks, dry mission spawn, three distinct nests/portal, key round-trip and deterministic regeneration.
- [ ] Increase height and use 64-bit stamp keys; generate original chunks then deterministic mission overlays, clipping neighboring stamps.
- [ ] Replace old terrain/map sampling, keep lower navigation ground, and exclude imported villages/floating footprints from objective sites.
- [ ] Use same nucleus for finite PvP/lobby terrain; preserve finite boundaries and authored safe areas. Verify seeds and map consistency.

### Task 3: Compatibilidade, verificação e entrega

**Files:** Modify `game/net.cpp`, `net/LanWire.kt`, `scripts/lan-bot.mjs`, `LanWireTest.kt`, README.

**Interfaces:** New lobby prefix `ZZ2` and game version 2 reject old terrain clients.

- [ ] Add/run rejection regression for `ZZ1`, then update sender/parser/tests and bot coherently.
- [ ] Run native tests, full JVM suite, ARM64/ARMv7 assembleDebug and lintDebug.
- [ ] Review whole change with a fresh reviewer and fix material findings with regressions.
- [ ] If possible, inspect a locally rendered terrain preview and use the connected Android device for runtime checks without replacing the user's installed app/save.
- [ ] Update README, record verification and deliver APK plus a concise report.

## Execution ledger

- Design approved by user with “continue”. Execute inline without another approval handoff.
- Pre-flight: Tasks 2/3 consume the terrain and height interfaces of Task 1; no conflicts.
- Decision: reuse current feature branch and preserve its pre-existing dirty changes; no worktree migration or automatic commits.
- Ruling: MSVC está instalado sem o SDK de headers do Windows; os testes nativos são compilados pelo NDK e executados no aparelho Android conectado. Isso testa a arquitetura real sem alterar o app instalado.
- Baseline: Java 17 encontrado em `C:/Program Files/Microsoft/jdk-17.0.18.8-hotspot`; a suíte anterior executou 250 testes, com quatro falhas preexistentes em `HudLayoutTest` (layout estreito, espelhamento, escala, separação dos botões).
- Task 1: complete — 40 fixtures de chunks gerados pelo código original, incluindo semente zero e coordenadas negativas, passaram no ARM64.
- Task 2: complete — regressões de ilhas flutuantes, posição das estruturas, ninhos/portal, início seco, sobrevivência, regeneração, arena e vértices distantes passaram. As regressões falharam antes da integração/correção correspondente.
- Ruling: o motor anterior armazenava vértices globais em int16 e transbordava depois de 4096 blocos. Usar x/z locais ao chunk e somar a origem no shader é necessário para o terreno completo de 6144; regressão de malha opaca e plantas passou após falhar.
- Ruling: a decoração da arena ocupa alguns centros de acampamento. Escolher spawn após decorar, procurando chão seco com espaço livre, resolve a falha de headroom observada.
- Task 3: in progress — regressão de recusa do protocolo antigo falhou antes da troca para ZZ2/version 2; compilação das duas ABIs, lint e suíte JVM em andamento.
- Final review: revisão independente confirmou fidelidade das fórmulas e encontrou `maxY` desatualizado no mundo finito. A regressão falhou antes da correção; agora verifica também malha elevada e topo do minimapa da arena.
- Final: fixed `maxY` — altura completa do chunk, malha e topo da arena passam.
- Ruling: a representação de materiais no mapa é um critério explícito da integração; classifiquei a omissão de aldeias/água desenhada como gelo como um ajuste necessário do mapa. A regressão de telhado exposto falhou antes da correção e passou depois; a água rasa de neve também é validada.
- Final: fixed representação do mapa — aldeias importadas, água e projeção elevada passam no teste nativo.
- Ruling: o computador de 4 GB ficou sem memória durante Gradle/lint e compilador nativo simultâneos (logs confirmam malloc/out of memory). Executar C++ serialmente sem JVM e depois Gradle com heap reduzido conserva a qualidade da verificação sem alterar o produto.
- Native final suite: PASS, ARM64 no aparelho conectado, incluindo 40 fixtures, mapas, missões, sobrevivência, arena, chaves e coordenadas distantes. Log: output/terrain/tests-final.log.
- Android native ARM64 and ARMv7: PASS, compilações sequenciais pelo Ninja.
- assembleDebug: PASS (40 s, heap 512 MB com GC serial).
- User steering: instalar no aparelho conectado e usar configuração para 4 GB de RAM. Instalado com `adb install -r` (Success), preservando dados. Configuração persistente em gradle.properties: heap 512 MB, GC serial, um worker, sem tarefas paralelas; C++ já usa um compilador por vez.
