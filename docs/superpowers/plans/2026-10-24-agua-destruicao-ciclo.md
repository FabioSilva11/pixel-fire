# Água, destruição e ciclo Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Água com fluxo, terreno destrutível por armas/granadas e ciclo solar original.

**Architecture:** Adaptadores C++ isolam a simulação importada. World registra
edições por chunk; Game conecta impactos, água e céu; NetSession distribui
alterações autoritativas com ACK. IDs anteriores são preservados.

**Tech Stack:** C++17/NDK, Kotlin, Gradle 512 MB/GC serial/um worker.

**Spec:** docs/superpowers/specs/2026-10-24-agua-destruicao-ciclo-design.md

## Global Constraints

- Sem compilações paralelas, sem apagar saves, preservar alterações locais.
- Geometria original e ciclo de 1.200 s; altura 128.
- Edições persistem dentro da partida, não entre novas partidas.

## Review Focus

- Fluxo não invade chunks não carregados nem nasce na borda do mundo.
- Bloco removido não reaparece após streaming e não deixa chão/luz fantasma.
- Pacote perdido/duplicado não diverge o terreno PvP.
- Pausa congela o céu; relógio visual de efeitos continua separado.
- ARM32 e ARM64 produzem a mesma geometria nos limites de arredondamento.

### Task 1: Precisão e importação de água/relógio

Files: CMakeLists.txt, scripts/test-terrain.ps1, fixtures, world/blocks.h,
world/fluids.{h,cpp}, world/fluid_catalogue.h, world/world_clock.h,
world/block_textures.cpp e assets/horizonte/water*.png.

- [x] Corrigir FMA e regenerar fixtures do original canônico; testar ambas as ABIs.
- [x] Escrever regressões de fluxo/relógio antes de importar funções.
- [x] Copiar algoritmo original com metadados locais e sem sistemas extras.
- [x] Integrar níveis de água à malha e às consultas de imersão.

### Task 2: Mundo editável e impactos

Files: world/world.{h,cpp}, world/destruction.{h,cpp}, game/game*.cpp/.h,
tests/native/world_interactions_tests.cpp.

- [x] Regressão de World::set e streaming deve falhar antes do journal.
- [x] Guardar/reaplicar edições, recalcular chão/luzes e revisão do mundo.
- [x] Ligar tiros, projéteis e granadas do jogador à quebra de blocos.
- [x] Ligar simulação de água e invalidar navegação/mapas ao alterar terreno.

### Task 3: Tempo e sincronização PvP

Files: game/net.{h,cpp}, game/game.cpp, Lua/Kotlin LanWire e bot se necessário.

- [x] Aplicar relógio/cores do Horizonte, fases e lanterna dinâmica.
- [x] Host valida tiros antes de escavar; enviar journal em lotes com ACK.
- [x] Clientes aplicam de forma idempotente; elevar versão do protocolo.

### Task 4: Verificação e aparelho

- [x] Suítes originais de terreno nas duas ABIs, suíte World original e testes JVM; build ARM64/ARMv7 sequencial.
- [x] Revisão independente, correções e regressões acrescentadas; codec do bot RED→GREEN.
- [x] Executar no aparelho as regressões finais de cachoeiras/luzes e repetir a suíte UDP após a reconexão.
- [x] Instalar APK com -r, verificar no aparelho e encerrar diagnósticos.

## Ledger

- Escopo autorizado pelo pedido direto; adaptar os sistemas concretos do projeto de origem.
- Baseline anterior instalada: APK SHA256 41b2dd007bcf49e357da08c6f4709b6c5387dd3206008c89d1971f880e06d1eb, confirmado igual no aparelho.
- Quatro HudLayoutTest e lint ByteOrderMark preexistentes documentados no relatório anterior.
- Investigação independente confirmou FMA na base das ilhas: quatro fixtures ARM64 divergiam. Original ARM64 sem FMA e ARM32 com/sem FMA concordam em 40/40.
- Task 1: implementado; algoritmo original de água, catálogo adaptado, texturas copiadas, superfície por níveis, imersão por altura, relógio/cores originais e flag -ffp-contract=off. Fixtures canônicas independentes substituem os quatro valores dependentes de FMA.
- Task 2: regressão original de chão falhou antes do journal; suíte World passou após integração: streaming/reinício, água (queda/propagação/retração/fontes), profundidade/malha, tiros/esfera/proteções e fases solares.
- Task 3: transporte implementado por agente em arquivos isolados. RED→GREEN de journal/ACK e sockets UDP reais, incluindo endpoint forjado e reconexão. PrefixoZZ3/Kotlin13/13 e codec do bot passaram. Game integra callback host, replay, seed e relógio derivado do tempo da partida.
- Regras de escopo: granadas continuam nos modos onde já existem; explosões de zumbis não escavam; água não adiciona inventário/lava dinâmica; não apagar progresso.
- Task 4: em verificação das duas arquiteturas, revisão e compilação.
- Retomada: 40/40 fixtures e suíte de terreno passaram em ARM64 e ARMv7; bibliotecas de produção compilaram nas duas ABIs em sequência.
- Revisão final: corrigidos três P2 (altura real de faces de cachoeira, efeitos de luz/fogo do layout PvP e B_COUNT do bot). Revisor confirmou as correções por leitura. Bot RED→GREEN, 2/2 testes.
- Novas suítes World e UDP compilaram para ambas as ABIs. Regressões finais aguardam execução: aparelho não aparece no ADB; endereço anterior não respondeu e mDNS vazio. Solicitada reconexão, sem alterar dados.
- Gradle executando testes JVM e assembleDebug em sequência, com heap de 512 MB.
- Gradle final: 251 testes, 247 passaram e as mesmas quatro falhas anteriores de HudLayoutTest; LanWire 13/13 passou. `--continue` completou assembleDebug, incluindo as duas ABIs e os ajustes de revisão; confirmação isolada do assemble em andamento.
- Assemble isolado confirmou BUILD SUCCESSFUL (59 s). APK atualizado em output/ZombieZone-Horizonte-debug.apk; ambas as bibliotecas coincidem por SHA-256 com o build e as texturas estão empacotadas. Registro agua-apk-verification.json. Instalação e regressões finais ainda pendentes: ADB e mDNS vazios na última verificação.
- 25/10: aparelho redescoberto em 192.168.1.231:39059. Regressão antiga de cachoeira falhou como esperado; suítes World e UDP corrigidas passaram em ARM64 e ARMv7. Instalação -r concluída; SHA-256 instalado igual a 352ae3a9d1e6d0c1917c1a0488e8e28ca01afab346745747134031bde4f2bb74. Missão seed42 abriu, imagem inspecionada, sem erros fatais nos logs capturados. Diagnóstico encerrado e aplicativo reaberto normalmente. Task 4 concluída; falhas anteriores de HUD/lint continuam documentadas.
