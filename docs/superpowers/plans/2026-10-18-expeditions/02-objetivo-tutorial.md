# 02 - Objetivo, tutorial jogável e documentação

> Área **02-objetivo-tutorial** (etapa 2 do spec `2026-10-18-zombie-zone-expeditions-design.md`, parte "corrigir objetivo/tutorial/documentação").
> Base: working tree de 2026-10-18, branch `feature/ilhas-infinitas`. Referências `arquivo:linha` valem para esse estado.
> Quem implementa lê **só este documento**: regras reais, divergências, código de referência, textos finais, ordem de trabalho, testes e aceitação estão aqui.

## 0. Escopo e limites

**Entrega desta área**

1. Uma única definição do objetivo - **"destrua ninhos → recolha cristais → extraia pelo portal"** - igual no motor, HUD, telas, tutorial e README, protegida por teste.
2. Um tutorial jogável curto na primeira fase (sem tela de texto longa): mover, olhar, atirar, abrir o mapa, seguir um marcador, reconhecer o núcleo do ninho, destruí-lo, recolher o cristal, usar o portal. Implementado como **máquina de estados pura em Kotlin** (testável na JVM), alimentada por eventos do motor + um snapshot do HUD/resultado.
3. Persistência do progresso do tutorial no save e "rever tutorial" sem apagar progresso.
4. Reescrita do `README.md` (texto completo no Apêndice A, inclui a versão) e um teste que impede o README de voltar a divergir.
5. Sinais nativos mínimos que faltam (bloco de cristais caídos no HUD, balizas 3D) - código pronto na seção 7.

**Não é desta área (não editar; só consumir/combinar)**

| Item | Dono | Como esta área interage |
|---|---|---|
| `ui/HudView.kt` (desenho do HUD, layout, `CombatFeed`) | **01-hud** | Seção 8 define o contrato (`showCoach`, `Objective`). Sem editar o arquivo. |
| Formato/versão/backup do save (`SaveStore`, recuperação) | área de save | Só acrescento um objeto JSON `"tutorial"` (seção 5). |
| IDs de anúncio, remoção da "dificuldade adaptativa por receita" | área de ads/dificuldade | Só o texto (README e `SettingsScreen`). **Não trocar IDs reais por IDs de teste** (restrição do usuário; sobrepõe a frase "variantes debug usarão IDs de teste" do spec). |
| Desempenho, contratos, PvP | demais áreas | Nenhuma dependência. |

**Restrições que continuam valendo**: sem emulador (testar no aparelho via `adb`), um Gradle por vez em modo baixa-memória, sem paredes/labirintos, decoração de no máximo 2 blocos, cura escassa, personagens procedurais (nada de GLB/Kenney), sem backend. As balizas 3D desta área são partículas/feixes, não blocos.

---

## 1. Regras REAIS do motor (verificadas no código)

### 1.1 Ciclo do objetivo

| # | Regra | Valor | Fonte |
|---|---|---|---|
| R1 | Ninhos por ilha de campanha | `kNestCount = 3`. Pode haver menos se o gerador não achar local (`nests_.size()`), então o HUD usa `nests_.size()`. | `island.h:24`, `island.cpp:265-272`, `game.cpp:2440` |
| R2 | Onde ficam os ninhos | Distância ao acampamento inicial: 1º 220-420 m, 2º 400-660 m, 3º 600-920 m, multiplicado por `1 + min(0.6, max(0, fase-8)*0.03)`. Ângulo mínimo 0,9 rad entre eles (relaxa após 300 tentativas). Reordenados por vizinho mais próximo: **índice 0 = o mais perto do início**. O jogador nasce olhando para o ninho 0. | `island.cpp:239-298`, `island.cpp:308-312` |
| R3 | Onde fica o portal | POI próprio (`ROLE_PORTAL`) a 350-720 m do início (x escala), ângulo mínimo 0,7 rad dos ninhos. **Não é "o acampamento mais distante"**; é independente dos ninhos. Moldura de obsidiana 5x6; interior livre 3x4. | `island.cpp:273`, `structures.cpp:402-419` |
| R4 | Vida do ninho | `320 + 36 x fase` (fase 1 = 356). Núcleo é uma caixa de ±0,85 m, 2,6 m acima do pedestal. | `game_mission.cpp:51`, `:245`, `structures.cpp:449` |
| R5 | O que fere o ninho | Tiros (raio contra a caixa do núcleo, só se for o alvo mais próximo e sem fumaça no caminho), **lança-chamas** (80% do dano, sem checar linha de visada) e **faca** (< 2,8 m). **Granadas, molotovs, fumaça e lança-granadas NÃO ferem ninhos.** | `game.cpp:1055`, `game_items.cpp:613-617`, `game.cpp:970-974`; `processExplosions` não toca `nests_` |
| R6 | Pressão do ninho | Com o jogador a < 55 m: gera um zumbi a cada `max(1,5; 2,8 - 0,04 x min(fase,30))` s, máx. 7 vivos por ninho. Partículas a < 70 m. Ao descobrir o POI (`columnMeshed`): 4 + min(4, fase/3) guardas (portal: 3 + min(3, fase/4)). Cada dano ao ninho alerta zumbis a 35 m. | `game_mission.cpp:201-237`, `:158-199`, `:277` |
| R7 | Destruir um ninho | `health <= 0` -> `destroyed = true`, explosão visual, **cria um Pickup tipo 0 (cristal)** no núcleo com `crystalIndex = índice do ninho`, sobe 4 m/s; emite `EV_NEST_DESTROYED(a = índice)`. | `game_mission.cpp:278-294` |
| R8 | Recolher o cristal | Ímã a < 3,2 m depois de 0,6 s de vida; coleta a < 1,25 m. Liga o bit do ninho em `crystalsCollectedMask_` e emite `EV_CRYSTAL(a = índice)`. **O cristal não expira** e **não aparece em marcador nenhum** hoje (lacuna G1). Se o chunk descarregar, o cristal "afunda" a y=0,45 mas é reposto no chão quando o chunk volta (`p.position.y < floorY` -> sobe), então não há soft-lock. | `game.cpp:1236-1268`, `:1243`, `:1258` |
| R9 | Quando o portal abre | `needed = (1 << nests_.size()) - 1`; abre quando `needed > 0` e `crystalsCollectedMask_ & needed == needed`. Ou seja: **todos os cristais de todos os ninhos**. Emite `EV_PORTAL_OPEN`. | `game.cpp:435-440` |
| R10 | Matar zumbis é necessário? | **Não.** Nenhuma condição do portal ou da vitória olha `kills_`. Abates valem diamantes (1 por abate), missões diárias e estatísticas. | `game.cpp:435-453` |
| R11 | Vitória | Com portal aberto, entrar na moldura: componente lateral de `d` (sobre o eixo da moldura) com módulo < 1,6, componente na normal com módulo < 1,0 e `-1 < d.y < 3` (`d = pos - portalBase`). Emite `EV_VICTORY`; o Kotlin mostra o resultado 700 ms depois. | `game.cpp:443-453`, `MainActivity.kt:761` |
| R12 | Portal fechado | É só a moldura de obsidiana. O plano roxo e as faíscas só existem com o portal aberto. | `renderer.cpp:1171-1179`, `game.cpp:1424` |
| R13 | Sobrevivência | `IslandGenerator(..., objectives = false)`: **sem ninhos, sem portal**; `needed == 0` nunca abre. O HUD mostra ondas, não objetivo. | `game_mission.cpp:30`, `game.cpp:2045` |
| R14 | Estrelas | Vencer; vida >= 50%; tempo <= `360 + 9 x min(fase,40)` s (fase 1 = 6:09). | `Progress.kt:273-278`, `Campaign.kt:118` |
| R15 | Recompensa de vitória | 1ª vitória (`40 + 10 x min(fase,60)`), 15 por estrela nova, 10 por cristal novo (máscara `0b111`), 1 por abate, diamantes dos baús, 1 por 100 m (máx. 40). Cristais e abates contam também em derrota. | `Progress.kt:281-328` |

### 1.2 O que o HUD já recebe do motor (índices em `NativeProtocol.kt` / `protocol.h`)

| Dado | Índice `Hud.*` | Observação |
|---|---|---|
| Ilha ativa / sobrevivência / PvP | `ISLAND` 537, `SURV` 630, `PVP` 400 | Campanha = ISLAND 1, SURV 0, PVP 0 |
| Estado | `STATE` 33 | 1 = morto, 2 = vitória |
| Posição / giro | `PLAYER_X/Z` 17/18, `CAM_YAW` 19 | |
| Distância andada | `DISTANCE` 534 | Só no bloco da ilha; zera a cada missão |
| Objetivos (até 4) | `OBJ_COUNT` 500, `OBJ` 501, stride 6: `(x, z, estado, sx, sy, naTela)` | estado 1 = ninho vivo, 2 = ninho destruído, 3 = portal fechado, 4 = portal aberto. Projeção em 1,5 m **acima** do núcleo; destruído não projeta. |
| Ninhos | `NESTS_DONE` 525, `NEST_TOTAL` 526, `NEST_NEAR` 539, `NEST_HP` 527 | `NEST_NEAR` = índice do ninho vivo mais próximo **a < 45 m** (senão -1); `NEST_HP` = fração de vida dele (senão -1) |
| Cristais | `CRYSTALS` 8 (coletados), `CRYSTAL_TOTAL` 37 | Só contagens, sem posição |
| Portal | `PORTAL_OPEN` 7, `PORTAL_X/Z` 20/21, `PORTAL_SX/SY/ONSCREEN` 28/29/30 | Projeção em 2 m acima da base |
| Contadores do resultado | `copyResult`: `Res.SHOTS` 7, `Res.HITS` 8, `Res.KILLS` 3, `Res.NESTS` 12 | Monotônicos, atualizados todo quadro por `finishResult()` |

### 1.3 Eventos que o motor já emite para a campanha

`EV_MISSION_READY(-1)`, `EV_POI(K_NEST / K_PORTAL / tipo)` (ao chegar a `raio+5` m, ~15 m do ninho), `EV_NEST_DESTROYED(índice)`, `EV_CRYSTAL(índice)`, `EV_PORTAL_OPEN`, `EV_VICTORY`, `EV_DEFEAT`, `EV_KILL`, `EV_NEW_KIND`, `EV_ELITE`, `EV_CHEST`, `EV_ITEM*`, `EV_SUPPLY`, `EV_STATUS`, `EV_MINIMAP`. **Para o tutorial nenhum evento novo é indispensável** (seção 7).

---

## 2. Divergências encontradas (README x tutorial x HUD x textos x motor)

### 2.1 README (`README.md`)

| ID | Linha | Diz | Motor/código real | Correção |
|---|---|---|---|---|
| D01 | 7 | "eliminar todos os zumbis da ilha e atravessar o portal ... no acampamento mais distante" | Matar zumbis não é exigido (R10). O portal é um POI independente (R3) e só abre com os 3 cristais (R9). | Objetivo oficial (Apêndice A). |
| D02 | 9 | "Cristais: cada fase tem 3. Eles caem dos zumbis." | Caem do **núcleo do ninho destruído** (R7); um por ninho. | Reescrever. |
| D03 | 10 | "Campanha: 20 fases em 4 biomas" | Fases infinitas (`Campaign.stage` procedural), 5 por zona; os 4 biomas repetem em ciclos II, III... | Reescrever. |
| D04 | 11 | "Zumbis: três tipos (comum, corredor e brutamontes)" | 10 tipos liberados por fase (`ZombieKinds`) + 6 mutações elite. | Reescrever. |
| D05 | 13-21 | "6 estilos de mapa", "5 tipos de acampamento", mapa novo por semente | Campanha = **uma** ilha de 6144 blocos (~5,1 km), 2 biomas misturados, rios, trilhas, **13 tipos de local** + ninho + portal. Os 6 estilos (`MapStyle`) valem só para a **arena PvP**. Noite só a partir da fase 3 (`applySkyVariant(..., stage >= 3)`). | Reescrever; mover os 6 estilos para PvP. |
| D06 | 48-58 | Tabela de controles | Faltam CORRER (botão, corrida automática), AGACHAR, FACA, GRANADA (segurar/soltar/✕), barra rápida de 6 espaços, MOCHILA, SUPRIMENTOS. | Reescrever. |
| D07 | 64-76 | "18 skins (os personagens Kenney)" com nomes (Mira, Toren, Brann...) | Personagens são **blocos procedurais** com editor (`Look`, até 6 heróis); as 18 entradas de `Cosmetics` são **peças especiais** (kit básico + 17) com metas diferentes. | Reescrever. |
| D08 | 81 | "Dificuldade adaptativa por receita de anúncios (opcional)" | O spec proíbe dificuldade definida por receita. No working tree o recurso virou automático e sem chave (`Campaign.difficulty(... adRevenueDifficultyEnabled = true)`); `SettingsScreen.kt:63-76` anuncia isso. | **Remover do README** e afirmar "a dificuldade depende só da fase e das melhorias". O código é da área de ads/dificuldade; ver risco K2. |
| D09 | 84 | "Os IDs de AdMob são de teste... troque AD_UNIT_ID em ads/RewardedAds.kt e o APPLICATION_ID no AndroidManifest.xml" | IDs **reais** em `app/build.gradle.kts:20-23`; o aparelho de desenvolvimento está em `ADMOB_TEST_DEVICES` e recebe anúncios de teste. `AD_UNIT_ID` vem de `BuildConfig`. | Reescrever sem pedir troca de IDs. |
| D10 | 88-106 | Estrutura: `assets/characters/` "18 personagens GLB", "carregador GLB", "JSON" em `core/` | Não existem: assets reais = `assets/weapons/` (AK-47 `.zzm`), `assets/icons/`; personagens em `render/blocky.cpp` + `data/Look.kt`; `core/` tem matemática, aleatório, plataforma. `ads/` tem consentimento (UMP), premiado e intersticial. | Reescrever. |
| D11 | 133 | "Versão `0.3.0` (versionCode 3)" | `build.gradle.kts:15-16` = `0.6.1` / `8` | Atualizar + `DocsConsistencyTest` mantém sincronizado. |
| D12 | 139 | Créditos Kenney, licença em `app/src/main/assets/notices/` | Pasta inexistente; Kenney só em `legacy/notices`. Origem/licença do modelo AK-47 não documentada. | Reescrever (decisão U1). |
| D13 | todo | Omissões | Sobrevivência (ondas), mochila/arsenal/loja/melhorias, editor de HUD, modo ultra-realista, intersticiais, pular fase por anúncio, caixas de suprimentos. | Acrescentar. |

### 2.2 Textos do jogo, telas e HUD

| ID | Onde | Problema | Correção (texto final no Apêndice B) |
|---|---|---|---|
| T01 | `MainActivity.kt:630-631` | "DESTRUA OS 3 NINHOS!" + dica "Siga as setas roxas": objetivo incompleto e não existem setas (a bússola usa losangos). | Mensagem = frase oficial; sem dica de "setas". |
| T02 | `MainActivity.kt:634-638` | `tutorialSeen = true` já no início da 1ª missão e só mostra "Arraste à direita para mirar": não há tutorial; o flag não prova que o jogador aprendeu. | Substituir pelo `CoachController` (seção 4). |
| T03 | `HudView.kt:987-1000` (`drawObjective`) | "NINHOS 0/3 • ◆ 0/3 • ☠ n": não diz a ordem; `◆` é cristal aqui e **diamante (moeda)** em `Header`, recompensas e baús; `☠` parece parte do objetivo. | **01-hud** consome `Objective.pills` (seção 8). |
| T04 | `HudView.kt:998-999,1013-1019` | Ramo `else` (mundo sem ilha): "☠ ABATES • RESTAM x DE y" reforça o mito "eliminar todos". Código morto na campanha (sempre ilha). | **01-hud** remove (junto com `drawPortalMarker`, `HudView.kt:774-792`, também morto). |
| T05 | `HudView.kt:727` bússola | Rótulo "PRÓXIMO NINHO": quando restam só cristais no chão (3 ninhos destruídos, portal ainda fechado) **não há alvo nenhum** (G1). | Alvo = portal aberto > cristal caído > ninho vivo (seção 8). |
| T06 | `HudView.kt:949-953` + `MainActivity.kt:521` | Em **sobrevivência** `CRYSTAL_TOTAL = 3` (config) e o HUD mostra "◆ 0/3" sem existir cristal. | `Cfg.CRYSTAL_TOTAL = 0` quando `survivalMode` (seção 6) e `H_CRYSTAL_TOTAL = 0` no nativo (seção 7). |
| T07 | `Overlays.kt:335` | Legenda do mapa "◆ ninho ... ◉ portal": sem cristal; `◆` ambíguo. | Legenda e ponto de cristal (seção 6). |
| T08 | `Overlays.kt:186-209` `PauseOverlay` | Pausa só diz "Fase N · título"; não repete o objetivo. | Segunda linha com a frase oficial (campanha). |
| T09 | `StagesScreen.kt:114` | "Objetivo: destrua os 3 ninhos, pegue os cristais e entre no portal." (verbos "pegue/entre" divergem). | `ObjectiveText.SENTENCE_CAPITALIZED`. |
| T10 | `Overlays.kt:79-91` dicas de carregamento | Nenhuma dica traz a frase oficial; a de índice 1 (fase 1) é "Tiros na cabeça...". | Nova dica 0 com o objetivo; usar índice 0 na 1ª corrida. |
| T11 | `MainActivity.kt:659` | "NINHO DESTRUÍDO! PEGUE O CRISTAL" (verbo "pegue"). | "NINHO n/3 DESTRUÍDO! RECOLHA O CRISTAL". |
| T12 | `MainActivity.kt:655` | "CRISTAL COLETADO ◆" (verbo "coletar" e ◆). | "CRISTAL n/3 RECOLHIDO". |
| T13 | `MainActivity.kt:759` | "PORTAL ABERTO!" sem dizer o que fazer. | "PORTAL ABERTO! EXTRAIA!". |
| T14 | `Progress.kt:170`, `Cosmetics.kt:88` | "Colete N cristais": verbo diferente do oficial. | "Recolha N cristais" (baixa prioridade). |
| T15 | `Campaign.kt:14-15,83-90,107-117` | `speaker`, `story` e `lines` ficaram sem leitor depois da remoção do `StoryOverlay` (grep sem usos). Os textos estão coerentes, mas são código morto. | Remover ou reaproveitar `lines` como dicas (opcional; arquivo compartilhado com a limpeza de dificuldade). |
| T16 | `SettingsScreen.kt:63-76` | "Dificuldade adaptativa ativa automaticamente..." | Remover o bloco (área de ads/dificuldade). |
| T17 | `docs/.../2026-10-18-zombie-zone-hud-redesign.md` Task 3 | Chama os cristais de "diamantes" na barra superior. | Terminologia da seção 3: **cristal ≠ diamante**. Corrigir o plano ou o HUD (01-hud). |
| T18 | Spec, "Confiança e continuidade" | "Variantes debug usarão IDs de teste" | **Sobreposto pela restrição do usuário**: IDs reais permanecem; aparelho em `ADMOB_TEST_DEVICES`. Não implementar. |

### 2.3 Lacunas do motor/HUD que impedem o objetivo de ser legível

| ID | Lacuna | Efeito | Solução |
|---|---|---|---|
| G1 | Cristal caído **sem marcador** (HUD, bússola, minimapa, mapa). | Jogador destrói um ninho com o rifle de precisão a 90 m, o cristal cai lá e ninguém sabe que precisa ir buscá-lo. | Bloco `H_CRYSTAL_*` (7.1) + baliza 3D (7.2) + marcadores na 01-hud/`MapOverlay`. |
| G2 | `H_OBJ` só comporta 4 entradas (3 ninhos + portal). | Se `kNestCount` subir, o portal some do HUD. | Aviso no código (7.1) / aumentar `kMaxHudObjectives` junto. |
| G3 | Sem realce 3D do núcleo/cristal/portal. | "Reconhecer o núcleo" depende só do brilho do cubo roxo. | Balizas (7.2), opcionais mas recomendadas. |
| G4 | `EV_NEST_DESTROYED`/`EV_CRYSTAL` sem contagem. | Texto "n/3" exige ler o HUD. | Opcional: `b`/`c` = feitos/total (7.3). Não é necessário (o HUD já está atualizado quando o evento é tratado). |
| G5 | `crystalsSpawnedMask_` só é escrita; `MissionConfig.crystalMask` não é lida. | Estado morto que sugere persistência entre tentativas. | Documentar; remover quando mexerem em `game.h`. |
| G6 | Marcador 3D do ninho (`sx,sy`) cai perto do centro quando o ninho está alinhado com a câmera (o jogador nasce olhando para ele). | Colide com a mira e com a regra "área central é do combate". | **01-hud** desloca/clipa o marcador. |

---

## 3. Texto oficial e glossário

| Termo | Significa | Nunca chamar de |
|---|---|---|
| **ninho** | Estrutura roxa de infecção (3 por ilha). | "base", "colmeia" |
| **núcleo** | Cubo roxo pulsante com casca verde no centro do ninho; o único ponto vulnerável. | "coração", "cristal" |
| **cristal** | Item ciano que cai do núcleo destruído; objetivo. | "diamante", "gema" |
| **diamante** (◆) | Moeda do jogo (recompensas, loja, baús). | "cristal" |
| **portal / extração** | Moldura de obsidiana; abre com os 3 cristais; entrar = vencer. | "saída" |

Verbos oficiais do objetivo: **destrua** (ninhos), **recolha** (cristais), **extraia** (pelo portal). Evitar "pegue", "colete", "entre" em textos de objetivo (em dicas de ação pontual, "entre na moldura" é aceitável).

Frase única (constantes em `ObjectiveText`, seção 4.2):

- `SENTENCE` = `destrua ninhos → recolha cristais → extraia pelo portal`
- `SENTENCE_UPPER` = `DESTRUA NINHOS → RECOLHA CRISTAIS → EXTRAIA PELO PORTAL`
- `SENTENCE_CAPITALIZED` = `Destrua ninhos → recolha cristais → extraia pelo portal`

Regra de uso: a frase aparece **inteira** em README, tela da fase, carregamento, pausa e mensagem de início; no HUD persistente ela vira três pílulas na mesma ordem (`NINHOS 0/3 → CRISTAIS 0/3 → PORTAL`). `◆` é reservado a diamantes; o HUD usa a palavra CRISTAIS (ou um ícone de cristal próprio), nunca `◆`, para o objetivo.

---

## 4. Módulo Kotlin puro `tutorial`

### 4.1 Arquivos

```
app/src/main/java/com/fabiosilva/ecosdeeldra/tutorial/
  ObjectiveText.kt       textos oficiais (fonte única)
  Objective.kt           fases, pílulas e próxima ação, a partir do array do HUD
  TutorialSnapshot.kt    fatos do quadro (HUD + resultado) que a máquina consome
  TutorialMachine.kt     Lesson, CoachHint, CoachFocus, estado, entradas e reduce() (PURO)
  TutorialPolicy.kt      quem aprende, migração do save, ajuste leve da 1ª corrida (PURO)
  CoachController.kt     cola Android: Handler 10 Hz, NativeBridge, CoachSink, TipCoachSink (NÃO testável na JVM)
app/src/test/java/com/fabiosilva/ecosdeeldra/tutorial/
  ObjectiveTest.kt  TutorialSnapshotTest.kt  TutorialMachineTest.kt  TutorialPolicyTest.kt
app/src/test/java/com/fabiosilva/ecosdeeldra/DocsConsistencyTest.kt
```

Regra de pacote: os cinco primeiros arquivos **não importam `android.*` nem `NativeBridge`**. Podem importar `engine.Hud`/`engine.Ev`/`engine.Res` (objetos de constantes, sem biblioteca nativa) e `data.Difficulty`.

### 4.2 `ObjectiveText.kt` e `Objective.kt` (referência)

```kotlin
package com.fabiosilva.ecosdeeldra.tutorial

object ObjectiveText {
    const val SENTENCE = "destrua ninhos → recolha cristais → extraia pelo portal"
    const val SENTENCE_CAPITALIZED = "Destrua ninhos → recolha cristais → extraia pelo portal"
    val SENTENCE_UPPER: String = SENTENCE.uppercase()

    const val PILL_NESTS = "NINHOS"
    const val PILL_CRYSTALS = "CRISTAIS"
    const val PILL_PORTAL = "PORTAL"

    const val ACTION_DESTROY = "DESTRUA O PRÓXIMO NINHO"
    const val ACTION_PICK = "RECOLHA O CRISTAL"
    const val ACTION_EXIT = "ENTRE NO PORTAL!"

    fun nestDestroyed(done: Int, total: Int) = "NINHO $done/$total DESTRUÍDO! RECOLHA O CRISTAL"
    fun crystalPicked(done: Int, total: Int) = "CRISTAL $done/$total RECOLHIDO"
    const val PORTAL_OPEN = "PORTAL ABERTO! EXTRAIA!"
    const val LOADING_TIP = "Objetivo: destrua ninhos → recolha cristais → extraia pelo portal. Não é preciso matar todos os zumbis."
}
```

```kotlin
package com.fabiosilva.ecosdeeldra.tutorial

import com.fabiosilva.ecosdeeldra.engine.Hud
import kotlin.math.max

enum class ObjectivePhase { NESTS, CRYSTALS, PORTAL, DONE }
enum class NextAction { DESTROY_NEST, PICK_CRYSTAL, ENTER_PORTAL, NONE }
enum class PillState { TODO, CURRENT, DONE }
data class Pill(val label: String, val value: String, val state: PillState, val attention: Boolean = false)

/** Estado do objetivo da campanha. `active = false` em sobrevivência, PvP e fora da ilha. */
data class Objective(
    val active: Boolean,
    val nestsDestroyed: Int,
    val nestTotal: Int,
    val crystals: Int,
    val portalOpen: Boolean,
    val victory: Boolean,
) {
    val crystalTotal: Int get() = nestTotal                    // um cristal por ninho
    val pendingCrystals: Int get() = max(0, nestsDestroyed - crystals)  // caídos e ainda no chão
    val phase: ObjectivePhase get() = when {
        victory -> ObjectivePhase.DONE
        portalOpen -> ObjectivePhase.PORTAL
        nestTotal > 0 && nestsDestroyed >= nestTotal -> ObjectivePhase.CRYSTALS
        else -> ObjectivePhase.NESTS
    }
    /** O que fazer agora (prioridade: portal > cristal no chão > próximo ninho). */
    val next: NextAction get() = when {
        !active || victory -> NextAction.NONE
        portalOpen -> NextAction.ENTER_PORTAL
        pendingCrystals > 0 -> NextAction.PICK_CRYSTAL
        nestsDestroyed < nestTotal -> NextAction.DESTROY_NEST
        else -> NextAction.NONE
    }
    val pills: List<Pill> get() = listOf(
        Pill(ObjectiveText.PILL_NESTS, "$nestsDestroyed/$nestTotal",
            when { nestsDestroyed >= nestTotal && nestTotal > 0 -> PillState.DONE; phase == ObjectivePhase.NESTS -> PillState.CURRENT; else -> PillState.TODO }),
        Pill(ObjectiveText.PILL_CRYSTALS, "$crystals/$crystalTotal",
            when { crystals >= crystalTotal && crystalTotal > 0 -> PillState.DONE; phase == ObjectivePhase.CRYSTALS -> PillState.CURRENT; else -> PillState.TODO },
            attention = pendingCrystals > 0),
        Pill(ObjectiveText.PILL_PORTAL, if (portalOpen) "ABERTO" else "",
            when { victory -> PillState.DONE; portalOpen -> PillState.CURRENT; else -> PillState.TODO }),
    )

    companion object {
        val INACTIVE = Objective(false, 0, 0, 0, portalOpen = false, victory = false)

        /** Lê o array do HUD (mesmo `FloatArray(Hud.SIZE)` que o HudView usa). */
        fun from(hud: FloatArray): Objective {
            val campaign = hud[Hud.ISLAND] > 0.5f && hud[Hud.SURV] < 0.5f && hud[Hud.PVP] < 0.5f
            val total = hud[Hud.NEST_TOTAL].toInt()
            if (!campaign || total <= 0) return INACTIVE
            return Objective(
                active = true,
                nestsDestroyed = hud[Hud.NESTS_DONE].toInt(),
                nestTotal = total,
                crystals = hud[Hud.CRYSTALS].toInt(),
                portalOpen = hud[Hud.PORTAL_OPEN] > 0.5f,
                victory = hud[Hud.STATE].toInt() == 2,
            )
        }
    }
}
```

Observações: `pendingCrystals = nestsDestroyed - crystals` é exato porque cada ninho destruído gera exatamente um cristal e cristais só vêm de ninhos (R7/R8); funciona **sem** o bloco nativo novo. O bloco nativo (seção 7.1) serve para a **posição** do cristal, não para a contagem.

### 4.3 `TutorialSnapshot.kt`

```kotlin
data class TutorialSnapshot(
    val valid: Boolean,               // campanha na ilha (não sobrevivência/PvP)
    val dead: Boolean,
    val victory: Boolean,
    val yaw: Float,                   // Hud.CAM_YAW
    val distanceWalked: Float,        // Hud.DISTANCE
    val shots: Int,                   // Res.SHOTS
    val objective: Objective,
    val nearNest: Int,                // Hud.NEST_NEAR (ninho vivo a < 45 m) ou -1
    val nearNestHp: Float,            // Hud.NEST_HP ou -1
    val nearestNestIndex: Int,        // ninho vivo mais próximo em H_OBJ (qualquer distância) ou -1
    val nearestNestDist: Float,       // metros; Float.MAX_VALUE se não houver
) {
    companion object {
        fun from(hud: FloatArray, res: FloatArray): TutorialSnapshot {
            val obj = Objective.from(hud)
            var best = Float.MAX_VALUE; var bestIdx = -1
            val n = hud[Hud.OBJ_COUNT].toInt().coerceIn(0, 4)
            for (i in 0 until n) {
                val o = Hud.OBJ + i * Hud.OBJ_STRIDE
                if (hud[o + 2].toInt() != 1) continue          // 1 = ninho vivo
                val d = kotlin.math.hypot(hud[o] - hud[Hud.PLAYER_X], hud[o + 1] - hud[Hud.PLAYER_Z])
                if (d < best) { best = d; bestIdx = i }
            }
            return TutorialSnapshot(
                valid = obj.active,
                dead = hud[Hud.STATE].toInt() == 1,
                victory = hud[Hud.STATE].toInt() == 2,
                yaw = hud[Hud.CAM_YAW], distanceWalked = hud[Hud.DISTANCE], shots = res[Res.SHOTS].toInt(),
                objective = obj, nearNest = hud[Hud.NEST_NEAR].toInt(), nearNestHp = hud[Hud.NEST_HP],
                nearestNestIndex = bestIdx, nearestNestDist = best,
            )
        }
    }
}
```

(Importar `Res` de `engine`. O índice do ninho em `H_OBJ` é o mesmo de `nests_`, portanto igual ao `a` de `EV_NEST_DESTROYED`.)

### 4.4 `TutorialMachine.kt` (referência quase completa)

**As 9 lições** (ordem = ordem didática do spec; `ordinal` = prioridade sequencial; `bit = 1 shl ordinal`; `Lesson.ALL = 0x1FF`):

| # | `Lesson` | Título | Corpo do aviso (<= 80 car.) | Realce (`CoachFocus`) | Fica "disponível" quando | Conclui quando |
|---|---|---|---|---|---|---|
| 1 | `MOVE` | MOVER | Arraste o polegar na metade ESQUERDA da tela para andar. | `JOYSTICK` | início | andou >= 6 m (`DISTANCE`) |
| 2 | `LOOK` | OLHAR | Arraste o dedo na metade DIREITA da tela para olhar ao redor. | `LOOK_AREA` | após MOVE | giro acumulado >= 1,0 rad (soma dos módulos de cada Δyaw, com wrap em ±π) |
| 3 | `SHOOT` | ATIRAR | Segure ATIRAR para disparar. MIRAR dá zoom e mais precisão. | `FIRE_BUTTON` | após LOOK | `Res.SHOTS` +3 desde o início da corrida |
| 4 | `MAP` | MAPA | Toque no MINIMAPA para abrir o mapa da ilha. Toque de novo para voltar. | `MAP_BUTTON` | após SHOOT | `MapOpened` (callback `onMapRequested`/`openMapFromPause`). **Pulada em silêncio** se um ninho vivo estiver a <= 150 m ou MARKER já foi concluída. |
| 5 | `MARKER` | SEGUIR O MARCADOR | Siga o marcador roxo NINHO: ele aparece no mundo, na bússola e no mapa. | `NEST_MARKER` | após MAP/skip; só se houver ninho vivo | `NEST_NEAR >= 0` (ninho vivo a < 45 m). Barra de progresso = aproximação (`1 - (d-45)/(d0-45)`). |
| 6 | `CORE` | NÚCLEO DO NINHO | O núcleo roxo pulsante é o ponto fraco do ninho. Atire nele! | `NEST_CORE` | **contextual**: `NEST_NEAR >= 0` | 1º dano: `NEST_NEAR >= 0 && NEST_HP < 0,999` (ou ninho já destruído) |
| 7 | `DESTROY` | DESTRUIR O NINHO | Continue atirando até a barra do ninho zerar. Ele chama zumbis: não pare de se mexer. | `NEST_CORE` | após CORE | `EV_NEST_DESTROYED` **ou** `NESTS_DONE >= 1`. Progresso = `1 - NEST_HP`. |
| 8 | `CRYSTAL` | RECOLHER O CRISTAL | Um cristal ciano caiu do ninho. Ande até ele para recolher. | `CRYSTAL` | **contextual**: `pendingCrystals > 0` | `EV_CRYSTAL` **ou** `CRYSTALS >= 1` |
| 9 | `PORTAL` | EXTRAIR | Portal aberto! Entre na moldura roxa para extrair e vencer a fase. | `PORTAL` | **contextual**: `PORTAL_OPEN` | `EV_VICTORY` / `STATE == 2` |

Mensagem de conclusão ("celebração", 1,4 s; 3,2 s para DESTROY e CRYSTAL): padrão "Muito bem!"; DESTROY: "Ninho destruído! Agora recolha o cristal."; CRYSTAL: "Cristal recolhido. Repita nos outros ninhos: com os 3 cristais, o portal abre."

**Regras de seleção do aviso ativo** (`pick`): contextuais primeiro (PORTAL > CRYSTAL > CORE/DESTROY perto do ninho), depois a primeira lição sequencial não concluída entre MOVE, LOOK, SHOOT, MAP, MARKER. Entre ninhos (depois do 1º cristal e antes do portal) não há aviso: só a linha persistente do HUD.

**Conclusões em silêncio**: uma lição concluída por fato enquanto **não** era a ativa não gera celebração. Concluir uma lição implica as anteriores (exceto MAPA, que só se aprende abrindo).

```kotlin
package com.fabiosilva.ecosdeeldra.tutorial

import com.fabiosilva.ecosdeeldra.engine.Ev
import kotlin.math.PI
import kotlin.math.abs

enum class CoachFocus { NONE, JOYSTICK, LOOK_AREA, FIRE_BUTTON, MAP_BUTTON, NEST_MARKER, NEST_CORE, CRYSTAL, PORTAL }

enum class Lesson(val title: String, val body: String, val focus: CoachFocus) {
    MOVE("MOVER", "Arraste o polegar na metade ESQUERDA da tela para andar.", CoachFocus.JOYSTICK),
    LOOK("OLHAR", "Arraste o dedo na metade DIREITA da tela para olhar ao redor.", CoachFocus.LOOK_AREA),
    SHOOT("ATIRAR", "Segure ATIRAR para disparar. MIRAR dá zoom e mais precisão.", CoachFocus.FIRE_BUTTON),
    MAP("MAPA", "Toque no MINIMAPA para abrir o mapa da ilha. Toque de novo para voltar.", CoachFocus.MAP_BUTTON),
    MARKER("SEGUIR O MARCADOR", "Siga o marcador roxo NINHO: ele aparece no mundo, na bússola e no mapa.", CoachFocus.NEST_MARKER),
    CORE("NÚCLEO DO NINHO", "O núcleo roxo pulsante é o ponto fraco do ninho. Atire nele!", CoachFocus.NEST_CORE),
    DESTROY("DESTRUIR O NINHO", "Continue atirando até a barra do ninho zerar. Ele chama zumbis: não pare de se mexer.", CoachFocus.NEST_CORE),
    CRYSTAL("RECOLHER O CRISTAL", "Um cristal ciano caiu do ninho. Ande até ele para recolher.", CoachFocus.CRYSTAL),
    PORTAL("EXTRAIR", "Portal aberto! Entre na moldura roxa para extrair e vencer a fase.", CoachFocus.PORTAL);

    val bit: Int get() = 1 shl ordinal
    companion object { val ALL: Int = (1 shl values().size) - 1 }
}

/** O que a HUD precisa para desenhar o cartão de dica. Imutável; comparável por igualdade. */
data class CoachHint(
    val lesson: Lesson,
    val step: Int,                  // quantas lições concluídas + 1 (1..total)
    val total: Int,                 // 9
    val title: String,
    val body: String,
    val progress: Float?,           // 0..1 para barra opcional (MARKER, DESTROY)
    val focus: CoachFocus,
    val focusIndex: Int = -1,       // índice do ninho (H_OBJ) / do cristal (H_CRYSTAL_ITEMS); -1 = n/a
    val celebrate: Boolean = false, // true = "✓ concluída": mostrar `body` como confirmação
    val skippable: Boolean = true,  // mostrar o botão PULAR TUTORIAL
)

data class TutorialState(
    val doneMask: Int = 0,
    val base: Base? = null,         // fatos no 1º tick da corrida
    val lastYaw: Float = 0f,
    val yawAccum: Float = 0f,
    val mapOpened: Boolean = false,
    val markerStartDist: Float = 0f,
    val celebrate: Lesson? = null,
    val celebrateUntil: Long = 0L,
    val finished: Boolean = false,
) {
    data class Base(val distance: Float, val shots: Int)
    fun isDone(l: Lesson) = doneMask and l.bit != 0
}

sealed interface TutorialInput {
    val nowMs: Long
    data class Tick(val snap: TutorialSnapshot, override val nowMs: Long) : TutorialInput
    data class Engine(val type: Int, val a: Int, val b: Int, val c: Int, override val nowMs: Long) : TutorialInput
    data class MapOpened(override val nowMs: Long) : TutorialInput
    data class Skip(override val nowMs: Long) : TutorialInput
}

/** Resultado de um passo: novo estado, aviso a mostrar (null = nada) e o que mudou. */
data class Step(val state: TutorialState, val hint: CoachHint?, val newlyDone: Int = 0, val finished: Boolean = false)

object TutorialMachine {
    const val MOVE_METERS = 6f
    const val LOOK_RADIANS = 1.0f
    const val SHOTS_NEEDED = 3
    const val NEST_NEAR_METERS = 45f      // igual ao raio de NEST_NEAR no nativo
    const val MAP_SKIP_METERS = 150f
    const val CELEBRATE_MS = 1_400L
    const val CELEBRATE_LONG_MS = 3_200L

    fun start(savedMask: Int): TutorialState {
        val mask = savedMask and Lesson.ALL
        return TutorialState(doneMask = mask, finished = mask == Lesson.ALL)
    }

    fun reduce(prev: TutorialState, input: TutorialInput): Step {
        if (prev.finished) return Step(prev, null)
        var s = prev
        val now = input.nowMs

        fun mark(l: Lesson, loud: Boolean, implyEarlier: Boolean = true) {
            if (s.isDone(l)) return
            var mask = s.doneMask or l.bit
            if (implyEarlier) for (e in Lesson.values()) if (e.ordinal < l.ordinal && e != Lesson.MAP) mask = mask or e.bit
            s = s.copy(doneMask = mask)
            if (loud) {
                val long = l == Lesson.DESTROY || l == Lesson.CRYSTAL
                s = s.copy(celebrate = l, celebrateUntil = now + if (long) CELEBRATE_LONG_MS else CELEBRATE_MS)
            }
            if (s.doneMask == Lesson.ALL) s = s.copy(finished = true)
        }
        fun result(hint: CoachHint?) = Step(s, hint, s.doneMask and prev.doneMask.inv(), s.finished && !prev.finished)

        when (input) {
            is TutorialInput.Skip -> {
                s = s.copy(doneMask = Lesson.ALL, finished = true, celebrate = null)
                return result(null)
            }
            is TutorialInput.MapOpened -> {
                s = s.copy(mapOpened = true)
                mark(Lesson.MAP, loud = true, implyEarlier = false)
                return result(null)   // o jogo está pausado; o próximo Tick monta o aviso
            }
            is TutorialInput.Engine -> {
                when (input.type) {
                    Ev.NEST_DESTROYED -> mark(Lesson.DESTROY, loud = true)
                    Ev.CRYSTAL -> mark(Lesson.CRYSTAL, loud = true)
                    Ev.VICTORY -> { mark(Lesson.PORTAL, loud = false); s = s.copy(doneMask = Lesson.ALL, finished = true) }
                }
                return result(null)
            }
            is TutorialInput.Tick -> Unit
        }

        val snap = (input as TutorialInput.Tick).snap
        if (!snap.valid || snap.dead) return result(null)
        val obj = snap.objective

        if (s.base == null) s = s.copy(base = TutorialState.Base(snap.distanceWalked, snap.shots), lastYaw = snap.yaw)
        val base = s.base!!
        s = s.copy(yawAccum = s.yawAccum + abs(wrapAngle(snap.yaw - s.lastYaw)), lastYaw = snap.yaw)

        // Fatos. "loud" só para a lição que estava no cartão; as outras fecham em silêncio.
        val active = pick(s, snap)
        fun fact(l: Lesson, ok: Boolean, implyEarlier: Boolean = true) { if (ok) mark(l, loud = l == active, implyEarlier = implyEarlier) }
        fact(Lesson.MOVE, snap.distanceWalked - base.distance >= MOVE_METERS)
        fact(Lesson.LOOK, s.yawAccum >= LOOK_RADIANS)
        fact(Lesson.SHOOT, snap.shots - base.shots >= SHOTS_NEEDED)
        fact(Lesson.MARKER, snap.nearNest >= 0 || obj.nestsDestroyed >= 1)
        fact(Lesson.CORE, (snap.nearNest >= 0 && snap.nearNestHp in 0f..0.999f) || obj.nestsDestroyed >= 1)
        fact(Lesson.DESTROY, obj.nestsDestroyed >= 1)
        fact(Lesson.CRYSTAL, obj.crystals >= 1)
        if (snap.victory) { mark(Lesson.PORTAL, loud = false); s = s.copy(doneMask = Lesson.ALL, finished = true) }
        if (snap.nearestNestDist <= MAP_SKIP_METERS || s.isDone(Lesson.MARKER)) mark(Lesson.MAP, loud = false, implyEarlier = false)
        if (s.finished) return result(null)

        // Aviso
        val celebrating = s.celebrate
        if (celebrating != null && now < s.celebrateUntil) return result(celebrationHint(celebrating, s, obj))
        val lesson = pick(s, snap) ?: return result(null)
        if (lesson == Lesson.MARKER && s.markerStartDist <= 0f && snap.nearestNestDist < Float.MAX_VALUE)
            s = s.copy(markerStartDist = snap.nearestNestDist)
        return result(hintFor(lesson, s, snap))
    }

    /** Lição que deve aparecer agora (contextuais antes das sequenciais). */
    fun pick(s: TutorialState, snap: TutorialSnapshot): Lesson? {
        val o = snap.objective
        fun open(l: Lesson) = !s.isDone(l)
        if (o.portalOpen && open(Lesson.PORTAL)) return Lesson.PORTAL
        if (o.pendingCrystals > 0 && open(Lesson.CRYSTAL)) return Lesson.CRYSTAL
        if (snap.nearNest >= 0) {
            if (open(Lesson.CORE)) return Lesson.CORE
            if (open(Lesson.DESTROY)) return Lesson.DESTROY
        }
        for (l in listOf(Lesson.MOVE, Lesson.LOOK, Lesson.SHOOT, Lesson.MAP, Lesson.MARKER)) {
            if (!open(l)) continue
            if (l == Lesson.MARKER && o.nestsDestroyed >= o.nestTotal) continue  // sem ninho vivo para seguir
            return l
        }
        return null
    }

    private fun hintFor(l: Lesson, s: TutorialState, snap: TutorialSnapshot): CoachHint {
        val progress = when (l) {
            Lesson.MARKER -> if (s.markerStartDist > NEST_NEAR_METERS && snap.nearestNestDist < Float.MAX_VALUE)
                (1f - (snap.nearestNestDist - NEST_NEAR_METERS) / (s.markerStartDist - NEST_NEAR_METERS)).coerceIn(0f, 1f) else null
            Lesson.DESTROY -> if (snap.nearNestHp in 0f..1f) 1f - snap.nearNestHp else null
            else -> null
        }
        val index = when (l) {
            Lesson.MARKER, Lesson.CORE, Lesson.DESTROY -> if (snap.nearNest >= 0) snap.nearNest else snap.nearestNestIndex
            Lesson.CRYSTAL -> 0
            else -> -1
        }
        val step = (Integer.bitCount(s.doneMask) + 1).coerceAtMost(Lesson.values().size)
        return CoachHint(l, step, Lesson.values().size, l.title, l.body, progress, l.focus, index)
    }

    private fun celebrationHint(l: Lesson, s: TutorialState, o: Objective): CoachHint {
        val text = when (l) {
            Lesson.DESTROY -> "Ninho destruído! Agora recolha o cristal."
            Lesson.CRYSTAL -> if (o.portalOpen) "Cristal recolhido. O portal abriu!" else "Cristal recolhido. Repita nos outros ninhos: com os 3 cristais, o portal abre."
            else -> "Muito bem!"
        }
        return CoachHint(l, Integer.bitCount(s.doneMask).coerceAtLeast(1), Lesson.values().size, l.title, text, null, CoachFocus.NONE, celebrate = true)
    }

    private fun wrapAngle(a: Float): Float {
        var x = a
        val tau = (PI * 2).toFloat()
        while (x > PI) x -= tau
        while (x < -PI) x += tau
        return x
    }
}
```

Notas de implementação:

- `Ev` é `engine.Ev` (`NEST_DESTROYED = 14`, `CRYSTAL = 3`, `VICTORY = 5`); `const val` permite `when`.
- `reduce` é determinístico: tempo só entra por `nowMs`. O controlador passa `SystemClock.uptimeMillis()`.
- `Step.newlyDone` é uma máscara (todas as lições recém-fechadas, ruidosas ou não). O controlador compara `state.doneMask` antes/depois para persistir; compara `state.celebrate` para tocar o som de "lição concluída".

### 4.5 `TutorialPolicy.kt`

```kotlin
object TutorialPolicy {
    const val VERSION = 1

    /** Máscara de lições já feitas a partir da qual ensinar; null = não ensinar. */
    fun maskToTeach(savedVersion: Int, savedMask: Int, completedStages: Int): Int? {
        if (completedStages > 0) return null                 // veterano: nunca interrompe
        val mask = if (savedVersion >= VERSION) savedMask and Lesson.ALL else 0
        return if (mask == Lesson.ALL) null else mask
    }

    fun isDone(savedVersion: Int, savedMask: Int) = savedVersion >= VERSION && (savedMask and Lesson.ALL) == Lesson.ALL

    data class Saved(val version: Int, val mask: Int)

    /** Save sem o objeto "tutorial" (versões antigas). `tutorialSeen` antigo NÃO prova aprendizado: só estágio vencido prova. */
    fun migrate(hasTutorialObject: Boolean, version: Int, mask: Int, completedStages: Int): Saved = when {
        hasTutorialObject -> Saved(version, mask and Lesson.ALL)
        completedStages > 0 -> Saved(VERSION, Lesson.ALL)
        else -> Saved(0, 0)
    }

    /** Ajuste leve SÓ na corrida com tutorial: menos zumbis e dano; nunca depende de anúncios. */
    fun tune(d: Difficulty): Difficulty = d.copy(
        zombieCount = minOf(d.zombieCount, 4),
        zombieDamage = d.zombieDamage * 0.7f,
        playerHealth = d.playerHealth * 1.25f,
    )
}
```

### 4.6 `CoachController.kt` (cola Android)

```kotlin
interface CoachSink { fun showCoach(hint: CoachHint?) }

class CoachController(
    initialMask: Int,
    private val sink: CoachSink,
    // A persistência fica nos callbacks: no "rever tutorial" eles gravam em memória (replayMask), nunca no save.
    private val onMask: (Int) -> Unit,               // MainActivity grava progress.tutorialMask + save()
    private val onFinished: () -> Unit,              // MainActivity grava progress.tutorialVersion = VERSION
    private val onCelebrate: () -> Unit,             // som + vibração leve
) {
    private val handler = Handler(Looper.getMainLooper())
    private val hudBuf = FloatArray(Hud.SIZE)
    private val resBuf = FloatArray(Res.SIZE)
    private var state = TutorialMachine.start(initialMask)
    private var lastHint: CoachHint? = null
    private var running = false
    private val ticker = object : Runnable { override fun run() { if (!running) return; tick(); handler.postDelayed(this, 100L) } }

    val active: Boolean get() = !state.finished

    fun start() { if (state.finished || running) return; running = true; handler.post(ticker) }
    fun pause() { running = false; handler.removeCallbacks(ticker) }          // mapa, pausa, mochila, anúncio, derrota
    fun resume() { if (!state.finished && !running) { running = true; lastHint = null; handler.post(ticker) } }  // republica o cartão
    fun stop() { pause(); sink.showCoach(null); lastHint = null }

    fun onEngineEvent(type: Int, a: Int, b: Int, c: Int) = apply(TutorialInput.Engine(type, a, b, c, SystemClock.uptimeMillis()))
    fun onMapOpened() = apply(TutorialInput.MapOpened(SystemClock.uptimeMillis()))
    fun skip() { apply(TutorialInput.Skip(SystemClock.uptimeMillis())) }

    private fun tick() {
        NativeBridge.copyHud(hudBuf); NativeBridge.copyResult(resBuf)
        apply(TutorialInput.Tick(TutorialSnapshot.from(hudBuf, resBuf), SystemClock.uptimeMillis()))
    }

    private fun apply(input: TutorialInput) {
        val step = TutorialMachine.reduce(state, input)
        if (step.state.doneMask != state.doneMask) {
            onMask(step.state.doneMask)
            Log.i("ZZCoach", "mascara=0x%x".format(step.state.doneMask))
        }
        if (step.state.celebrate != state.celebrate && step.state.celebrate != null) onCelebrate()
        val finished = step.finished
        state = step.state
        if (step.hint != lastHint) { lastHint = step.hint; sink.showCoach(step.hint) }
        if (finished) { onFinished(); pause(); sink.showCoach(null) }
    }
}

/** Fallback que só usa a API que o HudView já tem (tip), para o APK ficar jogável antes da 01-hud entregar o cartão. */
class TipCoachSink(private val hud: HudView) : CoachSink {
    private var last: CoachHint? = null
    private var shownAt = 0L
    override fun showCoach(hint: CoachHint?) {
        if (hint == null) { last = null; return }
        val now = SystemClock.uptimeMillis()
        if (hint.lesson != last?.lesson || hint.celebrate != last?.celebrate || now - shownAt > 4_000L) {
            hud.tip(if (hint.celebrate) "✓ ${hint.body}" else "${hint.step}/${hint.total} · ${hint.title}: ${hint.body}")
            shownAt = now
        }
        last = hint
    }
}
```

Observações: `NativeBridge.copyHud/copyResult` já são usados pelo `HudView.onDraw` na thread de UI e protegidos por mutex no nativo; chamar a 10 Hz é barato (660 floats). O controlador roda inteiro na UI thread (eventos do motor chegam via `runOnUiThread` em `MainActivity.onGameEvent`). Log de depuração: `Log.i("ZZCoach", "lição ${l} concluída mask=...")` a cada mudança de máscara (verificação por `adb logcat`).

---

## 5. Persistência e "rever"

### 5.1 Dados salvos

`Progress.kt` (ao lado de `tutorialSeen`, que **continua existindo** para não gerar conflito com a área de save; passa a ser escrito como `TutorialPolicy.isDone(...)`):

```kotlin
var tutorialSeen = false      // legado; derivado de isDone(version, mask) ao salvar
var tutorialVersion = 0       // maior versão do tutorial cumprida (TutorialPolicy.VERSION = 1)
var tutorialMask = 0          // bit por Lesson já concluída (0x1FF = tudo)
```

`SaveStore.kt` (chaves simples, sem depender de versão do save):

```kotlin
// encode(), junto de put("tutorialSeen", ...)
put("tutorialSeen", TutorialPolicy.isDone(p.tutorialVersion, p.tutorialMask))
put("tutorial", JSONObject().put("version", p.tutorialVersion).put("mask", p.tutorialMask))

// decode(), depois de carregar "stages" (precisa de completedStages)
val t = o.optJSONObject("tutorial")
val saved = TutorialPolicy.migrate(t != null, t?.optInt("version") ?: 0, t?.optInt("mask") ?: 0, p.completedStages())
p.tutorialVersion = saved.version
p.tutorialMask = saved.mask
p.tutorialSeen = TutorialPolicy.isDone(saved.version, saved.mask)
```

Casos (cobertos em `TutorialPolicyTest`):

| Save | Resultado |
|---|---|
| Novo jogador / sem chave `tutorial`, sem fase vencida | `version 0, mask 0` -> ensina tudo |
| Save antigo com `tutorialSeen = true` mas **nenhuma fase vencida** (o flag antigo era marcado ao iniciar a 1ª missão) | `0, 0` -> ensina (o flag não prova nada) |
| Save antigo com qualquer fase vencida/pulada | `VERSION, ALL` -> nunca interrompe |
| Tutorial parcial (ex.: parou após a lição 5) | continua das lições restantes na próxima corrida (nova ilha) |
| `VERSION` subiu no futuro | máscara antiga é descartada, exceto veteranos (que continuam isentos) |

### 5.2 Quando persistir

- A cada lição concluída: `progress.tutorialMask = mask; save()` (callback `onMask`). `save()` é barato (SharedPreferences `apply`).
- Ao terminar (todas as lições ou `Skip` ou `EV_VICTORY`): `progress.tutorialVersion = TutorialPolicy.VERSION; progress.tutorialMask = Lesson.ALL; progress.tutorialSeen = true; save()`.
- Derrota ou abandono **não** perdem lições já feitas.

### 5.3 Rever

- **Settings > CONTROLES** ganha o botão `▶ REVER TUTORIAL` (oculto quando `fromPause = true`). Toque -> `Popup` "Rever tutorial: vamos jogar a fase 1 com as dicas passo a passo. Seu progresso e suas fases vencidas não mudam." -> `MainActivity.replayTutorial()`.
- `replayTutorial()` define `replayMask = 0` (em memória) e chama `startStage(1)` (não `startCampaignPlay`, para não contar o toque no anúncio de cada 5º JOGAR). Enquanto `replayMask != null`, os callbacks do controlador atualizam só `replayMask` (e `onFinished` não faz nada): **nada é gravado no save**; sair da corrida e voltar não perturba quem já aprendeu.
- `replayMask` é atualizado a cada lição (para o "tentar de novo" continuar de onde parou) e zerado em `showHome()`, `showStages()` e `showSurvival()`.
- Fase 1 de propósito: dificuldade baixa e previsível. A fase 1 já vencida paga só bônus de repetição (abates, exploração), igual a qualquer repetição.
- Botão **PULAR TUTORIAL** no cartão (HUD) chama `coach.skip()` e persiste "concluído".

---

## 6. Integração em `MainActivity` e telas (hunks)

Todos os trechos preservam as mudanças não commitadas já existentes (intersticial a cada 5º JOGAR, `StoryOverlay` removido, `stageLaunchPending`).

### 6.1 `MainActivity.kt`

1. **Campos**: `private var coach: CoachController? = null` e `private var replayMask: Int? = null`.
2. **`launchMission()`** (`:498`): decidir antes de montar o `config`:
   ```kotlin
   val teachMask: Int? = if (survivalMode) null
       else replayMask ?: TutorialPolicy.maskToTeach(p.tutorialVersion, p.tutorialMask, p.completedStages())
   val diff = (if (survivalMode) Campaign.survivalDifficulty(p) else Campaign.difficulty(...))
       .let { if (teachMask != null) TutorialPolicy.tune(it) else it }
   ...
   config[Cfg.CRYSTAL_TOTAL] = if (survivalMode) 0f else stage.crystalTotal.toFloat()   // corrige T06
   ...
   coach?.stop()
   coach = teachMask?.let { mask ->
       CoachController(mask, sink = TipCoachSink(hud),
           onMask = { m -> if (replayMask != null) replayMask = m else { p.tutorialMask = m; save() } },
           onFinished = {
               if (replayMask == null) {
                   p.tutorialVersion = TutorialPolicy.VERSION; p.tutorialMask = Lesson.ALL; p.tutorialSeen = true; save()
               }
           },
           onCelebrate = { audio.playUi("reward", 0.5f); hud.haptic(false) })
   }
   ```
   No replay (`replayMask != null`) os callbacks só mexem em `replayMask`; fora dele gravam no `Progress` e chamam `save()`.
3. **`onMissionReady()`** (`:617-639`): no ramo campanha, trocar por `hud.message(ObjectiveText.SENTENCE_UPPER, Palette.lime)`; só chamar `hud.tip("Explore: baús escondidos guardam diamantes e itens")` quando `coach == null`. **Apagar** o bloco `if (!progress.tutorialSeen) {...}` e chamar `coach?.start()` no fim.
4. **`onGameEvent()`** (`:641`): logo após `if (!inMission) return`, `coach?.onEngineEvent(type, a, b, c)`. Trocar os textos:
   - `Ev.NEST_DESTROYED` -> `val o = currentObjective(); hud.message(ObjectiveText.nestDestroyed(o.nestsDestroyed, o.nestTotal), Palette.purple)`
   - `Ev.CRYSTAL` -> `hud.message(ObjectiveText.crystalPicked(o.crystals, o.crystalTotal), Palette.cyan)`
   - `Ev.PORTAL_OPEN` -> `hud.message(ObjectiveText.PORTAL_OPEN, Palette.purple)`
   onde `private fun currentObjective(): Objective { val h = FloatArray(Hud.SIZE); NativeBridge.copyHud(h); return Objective.from(h) }` (o HUD já está atualizado quando o evento é tratado: `publishHud()` roda antes de `drainEvents`).
5. **Mapa**: em `onMapRequested()` (`:1011`): `coach?.onMapOpened(); coach?.pause()` (o mapa fecha por `resumeMission()`, que retoma o coach); em `openMapFromPause()` (`:1043`): só `coach?.onMapOpened()` (o coach já está pausado pela pausa).
6. **Ciclo de vida do coach**: `coach?.pause()` em `pauseMission()`, `openInventoryInMatch()`, `onSupplyRequested()` (antes do anúncio), `showDefeat()`, `showSurvivalDefeat()`; `coach?.resume()` em `resumeMission()` e em `reviveWithAd()`; `coach?.stop(); coach = null` em `finishMission()`, `recordDefeat()` e `endMissionUi()`. Importar `android.util.Log` em `CoachController.kt`.
7. **`pauseSubtitle()`** (`:1007`): campanha -> `"Fase ${n} · ${título}\n${ObjectiveText.SENTENCE_UPPER}"`.
8. **`replayTutorial()`**: ver 5.3. `showHome()/showStages()/showSurvival()` -> `replayMask = null`.
9. **`HudView.Listener`**: quando a 01-hud adicionar `onCoachSkipRequested()`, implementar com `coach?.skip()`.

### 6.2 Outras telas

| Arquivo | Mudança |
|---|---|
| `ui/StagesScreen.kt:114` | `goals.text = "Objetivo: ${ObjectiveText.SENTENCE_CAPITALIZED}.\n★ vencer   ★ vida ≥ 50%   ★ tempo ≤ $target"` |
| `ui/Overlays.kt:79-91` | inserir `ObjectiveText.LOADING_TIP` como índice 0 de `tips`; em `LoadingOverlay` usar `tipIndex = 0` quando `coach` for ensinar (adicionar parâmetro `tipOverride: Int? = null` ao construtor secundário) |
| `ui/Overlays.kt:335` | legenda: `"◆ ninho   ✕ destruído   cristal (ciano)   ◉ portal   ■ suprimentos   ▲ você"` e, em `onDraw`, desenhar cada cristal de `Hud.CRYSTAL_ITEMS` (quando `Hud.CRYSTAL_COUNT > 0`): losango ciano pequeno + rótulo de distância |
| `ui/SettingsScreen.kt` | botão `▶ REVER TUTORIAL` (seção 5.3) logo abaixo de `✥ PERSONALIZAR CONTROLES`, oculto se `fromPause`; remover o bloco `revenueText` (`:63-76`) é da área de ads/dificuldade |
| `data/Progress.kt:170`, `data/Cosmetics.kt:88` | "Colete" -> "Recolha" (T14) |
| `data/Campaign.kt` | opcional: apagar `speaker`, `story`, `lines` (T15) |

---

## 7. Sinais nativos: existentes x faltantes

| Sinal | Para quê | Existe? | Ação |
|---|---|---|---|
| `EV_NEST_DESTROYED(índice)` | lição DESTROY, mensagem | sim | nada |
| `EV_CRYSTAL(índice)` | lição CRYSTAL | sim | nada |
| `EV_PORTAL_OPEN`, `EV_VICTORY`, `EV_DEFEAT` | lição PORTAL, parar o coach | sim | nada |
| `Res.SHOTS` | lição SHOOT | sim (`finishResult` todo quadro) | nada |
| `Hud.DISTANCE`, `Hud.CAM_YAW` | MOVE, LOOK | sim | nada |
| `Hud.NEST_NEAR`, `Hud.NEST_HP` | MARKER, CORE, DESTROY (aproximação, 1º dano, barra) | sim | nada |
| `Hud.OBJ` (x, z, estado, sx, sy, naTela) | alvo do realce do ninho/portal | sim | nada |
| Callback `onMapRequested` | MAP | sim (Kotlin) | nada |
| **Posição/projeção do cristal caído** | lição CRYSTAL, bússola, minimapa, mapa | **não** (G1) | **7.1 (obrigatório para realce do cristal)** |
| Balizas 3D (núcleo/cristal/portal) | "destacar" sem depender do HUD | não (G3) | 7.2 (recomendado) |
| Contagens em `EV_NEST_DESTROYED`/`EV_CRYSTAL` | texto "n/3" | não (G4) | 7.3 (opcional) |
| `H_CRYSTAL_TOTAL` em sobrevivência | evitar "◆ 0/3" | errado (T06) | 7.4 (correção) |
| Evento "primeira vez perto do ninho" / "1º tiro no ninho" / "tiro disparado" | lições CORE/SHOOT | não | **desnecessários**: derivados de `NEST_NEAR`/`NEST_HP`/`Res.SHOTS` |

### 7.1 Bloco de cristais caídos no HUD (obrigatório)

`cpp/game/protocol.h`:

```cpp
    H_WAVE_FLAGS = 643,
    // Cristais caídos e ainda não recolhidos (área 02): contagem + (x, z, sx, sy, naTela) x 3
    H_CRYSTAL_COUNT = 644, H_CRYSTAL_ITEMS = 645,   // 645..659; H_SIZE não muda
    H_SIZE = 660
};
constexpr int kHudCrystalStride = 5;
constexpr int kMaxHudCrystals = 3;
static_assert(H_CRYSTAL_ITEMS + kMaxHudCrystals * kHudCrystalStride <= H_SIZE, "bloco de cristais cabe no HUD");
```

`cpp/game/game.cpp`, dentro de `publishHud()`, no bloco `if (mode_ == Mode::Mission && island_)` (logo antes do `H_SUPPLY_X`, ~`:2591`):

```cpp
        // Cristais caídos ainda não recolhidos: posição e projeção (HUD, minimapa, mapa e tutorial).
        int cn = 0;
        for (const Pickup& p : pickups_) {
            if (p.type != 0 || p.taken || cn >= kMaxHudCrystals) continue;
            const int o = H_CRYSTAL_ITEMS + cn * kHudCrystalStride;
            float cx = 0, cy = 0;
            const bool on = project(p.position + Vec3(0, 0.6f, 0), cx, cy);
            h[o] = p.position.x; h[o + 1] = p.position.z; h[o + 2] = cx; h[o + 3] = cy; h[o + 4] = on ? 1.0f : 0.0f;
            ++cn;
        }
        h[H_CRYSTAL_COUNT] = static_cast<float>(cn);
```

`engine/NativeProtocol.kt` (`object Hud`): `const val CRYSTAL_COUNT = 644`, `const val CRYSTAL_ITEMS = 645 // (x, z, sx, sy, naTela) x 3`, `const val CRYSTAL_STRIDE = 5`.
Coordenação: os índices 644-659 ficam **reservados à área 02**. Se outra área precisar de mais espaço, aumentar `H_SIZE` (e `Hud.SIZE`) juntos; `jni_bridge.cpp` e `FloatArray(Hud.SIZE)` já acompanham a constante. Nota G2: se `kNestCount` crescer, aumentar `kMaxHudObjectives` e `kMaxHudCrystals`.

### 7.2 Balizas 3D (recomendado, ~15 linhas)

`cpp/game/game.cpp`, em `buildScene()`, logo depois do laço "Ninhos de infecção: núcleo pulsante" (`:1890-1904`). Usa só `s.beams`/`s.decals` já existentes (sem blocos novos, sem custo de malha):

```cpp
    // Balizas didáticas: feixes verticais para achar o que importa de longe (independem do HUD).
    if (mode_ == Mode::Mission && !survival()) {
        auto ring = [&](const Vec3& at, float radius, const Vec4& color) {
            int gx = static_cast<int>(std::floor(at.x)), gz = static_cast<int>(std::floor(at.z));
            float gy = world_.columnReady(gx, gz) ? static_cast<float>(world_.standHeight(gx, gz)) : at.y;
            s.decals.push_back({{at.x, gy, at.z}, radius, color, 1});
        };
        for (const Nest& n : nests_) {          // núcleo vivo a < 60 m
            if (n.destroyed || distXZ(n.core, player_.position) > 60.0f) continue;
            s.beams.push_back({n.core + Vec3(0, 0.9f, 0), n.core + Vec3(0, 9.0f, 0), {0.85f, 0.4f, 1.0f, 0.35f}, 0.18f});
        }
        for (const Pickup& p : pickups_) {      // cristal caído, sempre
            if (p.type != 0 || p.taken) continue;
            s.beams.push_back({p.position, p.position + Vec3(0, 9.0f, 0), {0.3f, 1.0f, 0.95f, 0.55f}, 0.22f});
            ring(p.position, 1.8f, {0.3f, 1.0f, 0.95f, 0.7f});
        }
        if (portalOpen_) {                      // portal aberto, sempre
            s.beams.push_back({layout_.portalBase + Vec3(0, 4.0f, 0), layout_.portalBase + Vec3(0, 18.0f, 0), {0.75f, 0.45f, 1.0f, 0.5f}, 0.5f});
            ring(layout_.portalBase, 3.2f, {0.75f, 0.45f, 1.0f, 0.7f});
        }
    }
```

Larguras (0,18 / 0,22 / 0,5 m) são ponto de partida: ajustar no aparelho (um feixe de 0,2 m some a ~100 m). Se a cena ficar poluída, manter só cristal e portal.

### 7.3 Contagens nos eventos (opcional)

`game_mission.cpp:293`: `emit(EV_NEST_DESTROYED, index, destroyedCount, static_cast<int>(nests_.size()))`; `game.cpp:1266`: `emit(EV_CRYSTAL, p.crystalIndex, collectedCount, static_cast<int>(nests_.size()))`. Atualizar os comentários de `protocol.h`. O Kotlin ignora `b`/`c` hoje, então é retrocompatível. Só vale a pena se o HUD quiser animar "n/3" sem ler o array.

### 7.4 Correções e diagnóstico

- `game.cpp:2440`: `h[H_CRYSTAL_TOTAL] = survival() ? 0.0f : static_cast<float>(nests_.empty() ? config_.crystalTotal : static_cast<int>(nests_.size()));`
- Linhas de log (tag `ZombieZoneNative`) para verificar as regras no aparelho sem ferramentas extras:
  - `damageNest` ao destruir: `LOGI("OBJ ninho_destruido idx=%d total=%zu t=%.1f", index, nests_.size(), missionTime_);`
  - `updatePickups` ao coletar cristal: `LOGI("OBJ cristal idx=%d mascara=0x%x", p.crystalIndex, crystalsCollectedMask_);`
  - `updateMission` ao abrir o portal: `LOGI("OBJ portal_aberto t=%.1f abates=%d", missionTime_, kills_);`
  - ao vencer: `LOGI("OBJ vitoria t=%.1f abates=%d", missionTime_, kills_);`

### 7.5 Opcional (não fazer sem decisão): ilha-tutorial menor

`MissionConfig.tutorial` (+`C_TUTORIAL`) reduziria as distâncias de `island.cpp:240` e adiaria o `spawnTimer_` inicial. **Não é necessário**: o trajeto de 220-420 m até o 1º ninho **é** a aula de "seguir o marcador" (1 a 1,5 min), e o Kotlin já alivia a 1ª corrida com `TutorialPolicy.tune` (4 zumbis, 70% de dano, 125% de vida). Ver decisão U2.

---

## 8. Contrato com a área 01-hud (sem conflito em `HudView.kt`)

A área 02 **não edita** `HudView.kt`. A 01 implementa, na mesma entrega do redesenho:

```kotlin
// ui/HudView.kt (01-hud)
fun showCoach(hint: CoachHint?)            // thread de UI; null remove; chamado a <= 10 Hz; idempotente
interface Listener { /* ... */ fun onCoachSkipRequested() = Unit }   // botão PULAR TUTORIAL do cartão
```

Tipos (`CoachHint`, `CoachFocus`, `Objective`, `Pill`, `NextAction`) vêm do pacote `tutorial` e **não** dependem de Android. Se a 01 preferir não importar o pacote, ela pode copiar só a leitura dos campos (a API é de dados).

**Requisitos de comportamento**

1. `HudView.reset()` **não** apaga o coach (o `reset()` é chamado a cada pausa); o `CoachController` também republica em `resume()`.
2. **Região própria**, sem competir com a mira nem com o aviso de combate (spec: "só o aviso de combate mais recente aparece"): sugestão faixa inferior-central (x 28-72% da largura, y 70-80% da altura, a região hoje usada por `drawTip`, `HudView.kt:1129-1145`), largura <= 44%, título 11 sp + corpo <= 2 linhas a 12 sp, barra de progresso fina quando `progress != null`, chip `PASSO n/9` e botão PULAR. Não usar `message()`/`CombatFeed`.
3. Enquanto houver coach ativo, `tip()` genérico é descartado (sem fila).
4. **Realce por `focus`** (somente se o controle estiver visível; `HudControl` com `visible = false` não recebe anel):

| `CoachFocus` | Desenho |
|---|---|
| `JOYSTICK` | anel pulsante em `stickHomeX/Y` |
| `LOOK_AREA` | seta/arrasto animado na metade oposta ao joystick (respeitar canhoto) |
| `FIRE_BUTTON` | anel em `g(HudControl.FIRE)` |
| `MAP_BUTTON` | anel em `mapX/mapY` |
| `NEST_MARKER` | destacar o losango da bússola e o marcador 3D do ninho `focusIndex` (`Hud.OBJ + idx*6`) |
| `NEST_CORE` | anel + rótulo "NÚCLEO" em `(sx, sy)` do ninho `focusIndex`, deslocado para o centro do núcleo: `+1,5 m * focal/dist` pixels para baixo, onde `focal = (h/2)/tan(Hud.FOV/2)` |
| `CRYSTAL` | anel em `Hud.CRYSTAL_ITEMS` (`sx, sy`, `naTela`); fora da tela, seta na borda pelo ângulo `atan2(dx,-dz) - CAM_YAW` |
| `PORTAL` | anel em `Hud.PORTAL_SX/SY` (ou seta de borda) |

5. **Objetivo persistente** (substitui `drawObjective` e `drawNestBar`/rótulos duplicados): usar `Objective.from(hud)` no máximo a cada 100 ms e desenhar as três `pills` na ordem `NINHOS → CRISTAIS → PORTAL`, `CURRENT` destacada, `DONE` riscada, `attention` pulsando; ao lado do cronômetro. Texto longo (`SENTENCE_UPPER`) só em pausa/mapa/carregamento. `◆` fica reservado a diamantes.
6. **Bússola e rótulo** (`HudView.kt:667-732`): alvo = portal aberto > cristal caído (`CRYSTAL_ITEMS`) > ninho vivo mais próximo; losango magenta (ninho), ciano (cristal), violeta (portal); rótulo "CRISTAL 12 m", "PRÓXIMO NINHO 276 m" ou "PORTAL 40 m" **uma vez só** (sem duplicar a distância do marcador 3D).
7. **Minimapa** (`HudView.kt:873-888`): ponto ciano por cristal caído.
8. Remover o código morto de mundo sem ilha (T04) e o texto `RESTAM x DE y`.
9. Telas estreitas (640x360 dp, `HudLayoutTest`): o cartão deve caber sem tocar a barra de itens nem os botões.

**Fallback enquanto a 01 não entrega**: `TipCoachSink` (seção 4.6) usa só `hud.tip()`. A troca é uma linha em `launchMission()` (`TipCoachSink(hud)` -> `hud`, quando `HudView : CoachSink`). Por isso a área 02 pode ser integrada **antes ou depois** da 01 e o APK permanece jogável nos dois casos.

---

## 9. Ordem de implementação

Rodar **um Gradle por vez**, sempre `--no-daemon --max-workers=1`; compilação de APK com `-Pandroid.injected.build.abi=arm64-v8a`; sem emulador.

- [ ] **P1 - Núcleo puro**: criar `ObjectiveText.kt`, `Objective.kt`, `TutorialSnapshot.kt`, `TutorialMachine.kt`, `TutorialPolicy.kt` (seção 4). Nenhum outro arquivo muda. Rodar `.\gradlew.bat :app:testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.tutorial.*" --no-daemon --max-workers=1`.
- [ ] **P2 - Testes**: criar os quatro `*Test.kt` (seção 10) e `DocsConsistencyTest.kt` (este começa com o README ainda antigo: marque os testes de README como `@Ignore("README em P8")` ou escreva-os por último).
- [ ] **P3 - Persistência**: `Progress.kt` (2 campos) e `SaveStore.kt` (encode/decode, seção 5.1). Rodar `RulesTest` + `TutorialPolicyTest`.
- [ ] **P4 - Controlador + MainActivity** (seção 4.6 e 6.1) com `TipCoachSink`. APK jogável: dicas aparecem como "tip".
- [ ] **P5 - Textos das telas** (6.2): `StagesScreen`, `LoadingOverlay`, `PauseOverlay`, legenda do mapa, "Colete" -> "Recolha", botão REVER TUTORIAL.
- [ ] **P6 - Nativo** (seção 7): `protocol.h`, `NativeProtocol.kt`, `publishHud` (7.1), balizas (7.2), correção de sobrevivência (7.4), logs. Compilar `arm64-v8a` e testar no aparelho (seção 11).
- [ ] **P7 - Handoff 01-hud**: abrir a tarefa/nota para a 01 com a seção 8; trocar `TipCoachSink` por `hud` quando `showCoach` existir; remover o fallback.
- [ ] **P8 - README**: aplicar o Apêndice A integralmente, remover `@Ignore`s, rodar `DocsConsistencyTest`.
- [ ] **P9 - Verificação final**: `.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a --no-daemon --max-workers=1`, instalar com `adb install -r -t app/build/intermediates/apk/debug/app-debug.apk` e executar a checklist da seção 11.

Arquivos quentes (compartilhados): `MainActivity.kt`, `Progress.kt`, `SaveStore.kt`, `NativeProtocol.kt`, `protocol.h`, `game.cpp`, `game_mission.cpp`, `Overlays.kt`, `SettingsScreen.kt`, `Campaign.kt`, `README.md`. Fazer commits pequenos por passo para facilitar o merge com as outras áreas.

---

## 10. Testes

Convenção: JUnit 4 (já no projeto), pacote `com.fabiosilva.ecosdeeldra.tutorial`, sem Android.

**`ObjectiveTest`**
- `sentenceConstantsMatchTheOfficialText`: `SENTENCE == "destrua ninhos → recolha cristais → extraia pelo portal"`, `SENTENCE_UPPER` e `SENTENCE_CAPITALIZED` derivam dela.
- `phaseFollowsNestsThenCrystalsThenPortal`: (0/3, 0) NESTS; (3/3, 1) CRYSTALS; portal aberto PORTAL; vitória DONE.
- `pendingCrystalsIsDestroyedMinusCollected`: (2, 1) -> 1.
- `nextActionPrefersPortalThenCrystalThenNest`.
- `pillsMarkDoneCurrentAndAttention`.
- `fromHudReadsCampaignIndices`: `FloatArray(Hud.SIZE)` com `ISLAND=1, NEST_TOTAL=3, NESTS_DONE=2, CRYSTALS=1, PORTAL_OPEN=0` -> valores esperados.
- `survivalAndPvpAreInactive`: `SURV=1` ou `PVP=1` -> `INACTIVE`.

**`TutorialSnapshotTest`**
- `readsDistanceYawShotsAndNearNest`; `nearestNestIgnoresDestroyedAndPortalEntries` (estado 1 só); `deadAndVictoryFromState`.

**`TutorialMachineTest`** (monta `TutorialSnapshot` sintéticos)
- `startsOnMoveLesson`; `moveCompletesAfterSixMeters`; `lookAccumulatesAcrossAngleWrap` (+3,0 -> -3,0 rad conta ~0,28, não 6,0); `shootCompletesAfterThreeShots`;
- `mapLessonCompletesOnMapOpenedWithoutImplyingOthers`; `mapLessonSkippedSilentlyWhenNestWithin150m`;
- `markerCompletesWhenNestIsWithin45m` e `markerProgressGrowsWhileApproaching`;
- `coreLessonAppearsOnlyNearNestAndCompletesOnFirstDamage`;
- `destroyCompletesOnNestDestroyedEventAndOnSnapshotFallback` (evento perdido não trava);
- `crystalLessonShowsWhenCrystalIsPendingAndCompletesOnEvent`;
- `portalLessonShowsWhenPortalOpensAndFinishesOnVictory`;
- `contextualLessonOverridesSequentialOne` (perto do ninho com MAP pendente mostra CORE);
- `completingALessonImpliesEarlierOnesExceptMap`;
- `silentCompletionDoesNotCelebrate` (lição fechada por fato quando não era a ativa);
- `celebrationIsShownBeforeNextHintAndExpires` (usa `nowMs`);
- `noHintBetweenNestsAfterFirstCrystal`;
- `restoredMaskSkipsDoneLessons` (`start(0b111)` começa em MAP);
- `deadSnapshotShowsNoHintAndKeepsProgress`;
- `invalidSnapshotNeverActivates` (sobrevivência/PvP);
- `skipFinishesAndMarksEverythingDone`; `finishedMachineIgnoresFurtherInput`;
- `reduceIsDeterministicForSameInputs`.

**`TutorialPolicyTest`**
- `newPlayerGetsFullTutorial`; `partialMaskContinues`; `completedTutorialStopsTeaching`; `veteranNeverSeesTutorial`; `olderVersionRestartsForNonVeterans`;
- `migrateKeepsSavedObject`; `migrateVeteranSkipsTutorial`; `legacyTutorialSeenAloneDoesNotCountAsLearned` (`migrate(false, 0, 0, completed=0) == Saved(0,0)`);
- `tuneLowersPressureOnlyForTutorialRun` (zombieCount <= 4, dano x0,7, vida x1,25 e **não** altera velocidade/vida dos zumbis).

**`DocsConsistencyTest`** (lê `../README.md` e `../app/build.gradle.kts` com `File(...)`; `Assume.assumeTrue(exists)` para não quebrar em ambientes sem o repo inteiro)
- `readmeStatesTheOfficialObjective`: README (minúsculas) contém `ObjectiveText.SENTENCE`.
- `readmeVersionMatchesGradle`: extrai `versionName`/`versionCode` por regex e exige `Versão \`$name\` (versionCode $code)`.
- `readmeHasNoObsoleteRules`: falha se contiver: `eliminar todos os zumbis`, `Eles caem dos zumbis`, `20 fases`, `Kenney`, `acampamento mais distante`, `três tipos`, `assets/characters`, `são **de teste**`, `Dificuldade adaptativa`.
- `sourcesDoNotRepeatObsoleteObjectiveTexts`: percorre `app/src/main/java/**/*.kt` e falha se achar `DESTRUA OS 3 NINHOS`, `setas roxas`, `PEGUE O CRISTAL`, `RESTAM $`.

**Não testável na JVM**: `CoachController` e o realce no HUD. Cobertos pela checklist do aparelho (seção 11) e pelos logs `ZZCoach`/`ZombieZoneNative`.

---

## 11. Verificação no aparelho (adb, sem emulador)

Preparar: `adb install -r -t app/build/intermediates/apk/debug/app-debug.apk`; limpar o save para simular jogador novo (`adb shell pm clear com.fabiosilva.ecosdeeldra` - atenção: apaga todo o progresso do aparelho; usar só se o usuário aceitar, senão testar com o botão REVER TUTORIAL). Logs: `adb logcat -s ZombieZoneNative:I ZZCoach:I`.

| # | Passo | Esperado |
|---|---|---|
| V1 | JOGAR fase 1 (jogador novo) | Carregamento mostra a dica 0 (objetivo). Mensagem inicial = frase oficial. Cartão "1/9 MOVER". |
| V2 | Andar 6 m, girar, atirar 3 vezes | Cada lição fecha com "Muito bem!" (~1,4 s) e o cartão avança: OLHAR -> ATIRAR -> MAPA. `ZZCoach` registra a máscara. |
| V3 | Abrir o mapa | Lição MAPA fecha; mapa mostra ninhos, portal e legenda nova. Fechar volta ao jogo com o cartão "SEGUIR O MARCADOR". |
| V4 | Caminhar até < 45 m do ninho | Cartão muda para NÚCLEO (realce no núcleo) e há feixe roxo; barra de vida do ninho no HUD. |
| V5 | Atirar no núcleo | Cartão vira DESTRUIR com barra de progresso. Ao zerar: "Ninho destruído! Agora recolha o cristal." (3,2 s), `ZombieZoneNative: OBJ ninho_destruido`. |
| V6 | Afastar-se ~40 m **sem pegar o cristal** | Feixe ciano e anel no chão visíveis; marcador do cristal na bússola/minimapa/mapa (após a 01-hud); cartão RECOLHER O CRISTAL. |
| V7 | Recolher o cristal | "CRISTAL 1/3 RECOLHIDO"; celebração "Repita nos outros ninhos..."; **nenhum cartão** entre ninhos. Pílulas: NINHOS 1/3, CRISTAIS 1/3. |
| V8 | Repetir nos outros dois ninhos **sem matar todos os zumbis** | Portal abre com o 3º cristal (`OBJ portal_aberto ... abates=<pequeno>`): confirma R10. Mensagem "PORTAL ABERTO! EXTRAIA!", feixe violeta, cartão EXTRAIR. |
| V9 | Entrar no portal | Vitória; `OBJ vitoria`; tutorial marcado como concluído. Nova corrida: sem cartões. |
| V10 | Matar o app no meio (após a lição 5) e reabrir | Próxima corrida (nova ilha) retoma na lição 6 em diante; lições 1-5 não reaparecem. |
| V11 | Settings > REVER TUTORIAL | Popup; inicia fase 1 com cartões desde a lição 1; ao voltar ao menu o save não muda (`tutorial.mask` igual). |
| V12 | Sobrevivência | Sem cartões, sem pílulas, **sem "◆ 0/3"**; pausa mostra só "Sobrevivência • Onda n". |
| V13 | Pausar em campanha | Pausa mostra a frase oficial em maiúsculas. |

---

## 12. Critérios de aceitação

1. `ObjectiveText.SENTENCE` aparece, igual, em: README, tela de fase, dica de carregamento, pausa, mensagem inicial; o HUD usa as três pílulas na mesma ordem (após a 01-hud).
2. Nenhum texto do jogo ou do README sugere que é preciso eliminar todos os zumbis, que cristais caem de zumbis, que há "20 fases" ou "3 tipos de zumbi" (garantido por `DocsConsistencyTest`).
3. Jogador novo conclui a fase 1 seguindo só os cartões (V1-V9), sem tela de texto longa; cada cartão tem <= 80 caracteres de corpo.
4. Cristal caído sempre é localizável: baliza 3D + marcador (V6).
5. Progresso do tutorial persiste lição a lição; veterano nunca vê o tutorial; "rever" não altera o save (V10, V11).
6. Sobrevivência não mostra nada de objetivo/cristal (V12).
7. `.\gradlew.bat :app:testDebugUnitTest` verde (testes novos + existentes) e APK `arm64-v8a` instala e roda no aparelho.
8. IDs de AdMob em `app/build.gradle.kts` **inalterados**; nenhuma variante debug com IDs de teste.
9. `README.md` descreve versão, regras e estrutura realmente existentes (Apêndice A) e o teste de versão passa.

## 13. Riscos e decisões

**Riscos**

| ID | Risco | Mitigação |
|---|---|---|
| K1 | Conflito em arquivos quentes (`MainActivity`, `game.cpp`, `NativeProtocol.kt`, `protocol.h`, `SaveStore`, `README`). | Hunks pequenos e localizados; passos P1-P9 em commits separados; índices `644-659` reservados. |
| K2 | README diz "a dificuldade nunca depende de anúncios" enquanto o código de receita ainda existe até a área de ads/dificuldade concluir. | Não publicar o README final antes de `grep -rn adRevenue app/src/main` zerar; ou publicar com a frase e abrir a pendência. |
| K3 | A 01-hud demora: sem o cartão/pílulas a UX fica no fallback (`tip`) e a frase persistente só aparece em pausa/carregamento/início. | `TipCoachSink` mantém o APK jogável; o contrato (seção 8) é a única dependência. |
| K4 | `HudView.reset()` apaga o cartão a cada pausa. | Requisito 1 da seção 8 + `resume()` republica. |
| K5 | Lições concluídas por engano (aim assist gira a câmera; auto-tiro dispara sozinho). | Aceito: a mira assistida só puxa a câmera enquanto atira, e o auto-tiro mostra o tiro ao jogador; a lição existe para apresentar os controles, não para exigir uso. |
| K6 | Feixes 3D poluem a cena ou somem à distância. | Larguras ajustáveis; começar só com cristal e portal se necessário. |
| K7 | `kNestCount` ou `kMaxHudObjectives` mudarem sem atualizar bloco de cristais/`H_OBJ`. | `static_assert` do bloco e comentário G2. |
| K8 | UMP/consentimento de anúncios pode aparecer sobre a 1ª missão. | O coach só começa em `EV_MISSION_READY`; `pause()` ao mostrar anúncio. |
| K9 | A 1ª corrida pode demorar 6-10 min até o portal (3 ninhos a 220-920 m). | É o desenho atual da fase; o tutorial cobre as lições 1-8 em ~3-5 min e a 9 só ao final. Ver U2. |

**Decisões que só o usuário pode tomar**

- **U1**: origem e licença do modelo da arma (`app/src/main/assets/weapons/ak47.zzm` e `ak47_texture.jpg`, convertidos por `tools/convert_glb_to_zzm.py`). O README antigo só creditava os personagens Kenney (removidos); sem essa informação os créditos ficam incompletos. O Apêndice A traz o parágrafo com a lacuna marcada.
- **U2** (opcional): encurtar as distâncias dos ninhos apenas na 1ª corrida (seção 7.5)? Recomendação: **não**; manter a ilha real e reavaliar após o teste no aparelho.

---

## Apêndice A - Novo `README.md` (substituir o arquivo inteiro)

Mantém a seção PvP como estava (a área de PvP altera depois). Valores de versão: os atuais de `app/build.gradle.kts` (0.6.1 / 8); se alguém subir a versão antes do merge, o `DocsConsistencyTest` avisa.

~~~markdown
# Zombie Zone

Jogo Android de tiro em terceira pessoa num **mundo aberto de blocos** (estilo Minecraft), escrito em **Kotlin + C++ (NDK/OpenGL ES 3.0)**. Funciona offline: não há conta, servidor, WebView nem JavaScript. Abra a pasta do projeto no Android Studio e rode.

## Como jogar

**Objetivo de cada fase: destrua ninhos → recolha cristais → extraia pelo portal.**

1. **Destrua os 3 ninhos de infecção.** Cada ninho tem um núcleo roxo pulsante: atire nele (tiros, lança-chamas ou faca; granadas, molotovs e lança-granadas não ferem ninhos). Com você a menos de 55 m, o ninho gera zumbis.
2. **Recolha o cristal** que cai do núcleo de cada ninho destruído: basta chegar perto. O cristal fica no chão até você pegá-lo.
3. **Extraia pelo portal.** Com os 3 cristais em mãos, o portal de obsidiana abre. Entre nele para vencer a fase. O portal fica num ponto da ilha independente dos ninhos (aparece como PORTAL no mapa).

Não é preciso matar todos os zumbis: os abates rendem diamantes, mas não abrem o portal.

- **Estrelas:** vencer a fase, terminar com vida ≥ 50% e terminar dentro do tempo-alvo (cresce com a fase; 6:09 na fase 1).
- **Cristais x diamantes:** cada fase tem 3 **cristais** (um por ninho); eles dão um bônus na primeira vez. Os **diamantes** (◆) são a moeda do jogo: vêm de abates, baús, estrelas, missões e vitórias.
- **Campanha infinita:** 5 fases por zona. Os quatro biomas (Floresta, Deserto, Neve e Terra Devastada) se repetem em ciclos cada vez mais difíceis (II, III...). A noite só aparece a partir da fase 3.
- **A ilha:** cada partida gera uma ilha nova de cerca de 5 km (6144 blocos), com dois biomas misturados, rios, trilhas e pontos de interesse: acampamento, ruínas, cemitério, depósito, fazenda, torre de vigia, posto avançado, casa, vilarejo, igreja, posto de gasolina, prédio em ruínas e celeiro. Entre nas construções (as portas ficam abertas) e suba nas torres: há baús escondidos com diamantes e itens. A cura é escassa.
- **Zumbis:** 10 tipos que aparecem conforme a fase (errante, corredor, brutamontes, arremessador, incendiário, retalhador, inchado, gritador, cuspidor e atirador de facas), mais variações elite (blindado, gigante, veloz, regenerador, tóxico e furioso). Eles contornam o relevo com um campo de fluxo (BFS), escutam tiros e cercam o jogador.
- **Mapa:** tocar no minimapa abre o mapa da ilha inteira (o jogo pausa) com ninhos, cristais caídos, portal e suprimentos.
- **Dificuldade:** depende só da fase e das melhorias que você comprou. Nunca depende de anúncios.
- **Sobrevivência:** modo à parte, numa ilha livre sem ninhos nem portal. Os zumbis chegam em ondas cada vez maiores, vindos de até 3 frentes; a cada 5 ondas vem uma GRANDE HORDA. O ranking é local (neste aparelho).

### PvP em Wi-Fi local

Na tela inicial, toque em **⚔ PvP WI-FI**:

1. Um jogador toca em **CRIAR SALA**.
2. Os outros veem a sala na lista e tocam em **ENTRAR**. Se a lista não aparecer, use **CONECTAR POR IP** com o IP mostrado na tela do anfitrião.
3. O anfitrião escolhe o limite de abates (5, 10, 15 ou 20) e o tempo (3, 5 ou 8 minutos), e toca em **INICIAR PARTIDA**.

Regras e detalhes:

- **Formato:** todos contra todos, de 2 a 8 jogadores.
- **Mira:** sem mira assistida e sem tiro automático.
- **Arena:** uma arena procedural nova a cada partida, em um de seis estilos (ilha, arquipélago, vale entre montanhas com rio, mesetas em degraus, cratera com lago e planície de rios).
- **Renascimento:** 3 segundos depois de ser eliminado, longe dos inimigos.
- **Recompensa:** 2 diamantes por abate, mais 25 pela vitória (5 por participar).
- **Rede:** os aparelhos precisam estar no mesmo Wi-Fi, ou um deles roteando o hotspot.
- **Portas UDP:** 47777 (sala) e 47778 (partida).
- **Autoridade:** o anfitrião decide a vida, os abates e o placar.

Para testar com um celular só, crie a sala no celular e rode no PC (mesmo Wi-Fi) o jogador-robô:

```powershell
node scripts/lan-bot.mjs <ip-do-celular>
```

### Controles (toque)

| Ação | Como |
|---|---|
| Mover | Joystick dinâmico: aparece onde o polegar toca na metade esquerda |
| Correr | Botão CORRER liga a corrida automática (toque de novo, ou puxe o joystick para trás, para parar); empurrar o joystick até o fim também corre |
| Câmera | Arrastar em qualquer ponto livre da metade direita |
| Atirar | Segurar ATIRAR; arrastar o botão também gira a câmera |
| Mirar | MIRAR alterna o zoom (o rifle de precisão tem luneta de 3 níveis) |
| Pular / Recarregar / Agachar | Botões próprios (a recarga também é automática com o pente vazio) |
| Faca | FACA golpeia de perto (também fere o núcleo do ninho) |
| Granada | Segure para ver a trajetória e solte para lançar; arraste até o ✕ para cancelar (granada, molotov ou fumaça, conforme o item escolhido) |
| Itens | Barra rápida de 6 espaços: toque para equipar a arma, escolher o arremessável ou usar o consumível; MOCHILA abre o inventário (o jogo pausa) |
| Suprimentos | Botão SUPRIMENTOS (anúncio premiado): caixa de paraquedas com vida, munição e itens |
| Mapa | Tocar no minimapa abre o mapa da ilha (o jogo pausa) |

Nos Ajustes há sensibilidade, mira assistida, tiro automático, eixo Y invertido, personalização completa do HUD (mover, aumentar ou esconder cada botão), modo canhoto, vibração, volumes, qualidade gráfica (60/80/100% da resolução), modo ultra-realista, FPS e lembretes. **REVER TUTORIAL** refaz as dicas da primeira fase sem mexer no seu progresso.

## Armas, mochila e melhorias

- **Mochila estilo Minecraft:** 6 espaços na barra rápida + 18 na mochila. As armas nunca somem; consumíveis empilham.
- **8 armas:** fuzil de assalto, pistola, escopeta, submetralhadora, rifle de precisão, metralhadora, lança-chamas e lança-granadas. Elas liberam por fase vencida e se compram com diamantes no Arsenal.
- **Consumíveis:** kit médico, bandagem, antídoto, adrenalina, munição incendiária, colete, granada, molotov e granada de fumaça.
- **10 melhorias** compradas com diamantes (vitalidade, poder de fogo, recarga, blindagem, regeneração, agilidade, mira mortal, pente estendido, sorte e explosivos); o limite de nível sobe a cada zona liberada.

## Personagem, progresso e anúncios

- **Editor de personagem** em blocos (até 6 heróis). As peças comuns são livres; 17 peças especiais (além do kit básico) são liberadas por objetivos (abates, tiros na cabeça, fases, estrelas, cristais, minutos jogados), por dias de login, por anúncios assistidos ou por diamantes.
- **Calendário diário** com 7 prêmios crescentes (30 → 150 diamantes), resgatado uma vez por dia.
- **3 missões diárias** sorteadas por data, mais um bônus por completar as três.
- **Baú grátis a cada 4 horas.** Dá para abrir antes do tempo assistindo a um anúncio.
- **Lembretes locais** (notificação) quando o baú fica pronto e para o prêmio do dia seguinte.
- **Anúncios (AdMob):**
  - Premiados, sempre opcionais: +25 diamantes (até 5 por dia), reviver uma vez por partida, dobrar a recompensa da vitória, abrir o baú antes da hora, pedir suprimentos durante a partida, caixas de suprimentos no Arsenal (até 6 por dia) e pular uma fase travada (depois de 2 derrotas).
  - Intersticiais: entre partidas (com intervalo mínimo) e antes de cada 5º início manual de fase pela campanha. Nunca durante a partida.
  - O consentimento (UMP) é pedido quando exigido; em Ajustes aparece "Privacidade dos anúncios" quando disponível.
  - Os IDs reais ficam em `app/build.gradle.kts` (`admobAppId`, `ADMOB_REWARDED_ID`, `ADMOB_INTERSTITIAL_ID`). O aparelho de desenvolvimento está em `ADMOB_TEST_DEVICES` e recebe anúncios de teste; não clique em anúncios reais ao desenvolver.

## Estrutura

```text
app/src/main/
  cpp/                      Motor C++ (CMake, OpenGL ES 3.0)
    core/                   matemática, aleatório/ruído, plataforma (assets e imagens via JNI)
    world/                  blocos, texturas procedurais 16x16, mundo em chunks com streaming,
                            ilha procedural (island), estruturas e construções
    render/                 renderizador (céu, nuvens, água, partículas, sombras), personagens de blocos
                            (blocky/character), arma importada (weapon_asset)
    game/                   jogador, câmera, armas e itens, missão da ilha (ninhos, cristais, portal),
                            sobrevivência por ondas, zumbis (IA + campo de fluxo), PvP (net.cpp, UDP),
                            HUD/eventos e protocolo JNI
    jni_bridge.cpp          funções nativas usadas por NativeBridge.kt
  java/com/fabiosilva/ecosdeeldra/
    MainActivity.kt         fluxo de telas, missão, anúncios, eventos do motor
    engine/                 NativeBridge (JNI), GameSurfaceView (GLSurfaceView, 60 FPS), protocolo
    data/                   campanha, itens, progresso e regras (testáveis), visual do personagem, SaveStore (JSON)
    tutorial/               objetivo oficial e tutorial jogável (máquina de estados pura, testável)
    ui/                     telas em Views nativas, HUD/controles em Canvas, sobreposições
    audio/                  SoundPool (efeitos) + MediaPlayer (música)
    net/                    LanLobby: salas PvP no Wi-Fi (broadcast UDP)
    ads/  notify/           AdMob (consentimento, premiado, intersticial) e lembretes (AlarmManager)
  assets/weapons/           modelo da arma (.zzm + textura)
  assets/icons/             ícones SVG de armas
  res/raw/                  áudio OGG Vorbis gerado por scripts/build-audio.ps1
```

O mundo, as texturas, os personagens, os sons e as músicas são **gerados pelo projeto**: terreno, texturas e personagens em tempo de execução, e áudio sintetizado por `scripts/generate-audio.mjs`. Para regerar o áudio é preciso ter Node.js e ffmpeg:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build-audio.ps1
```

## Compilar e testar

No Android Studio, use **Run** com o celular conectado. Só a ABI do aparelho é compilada.

Pela linha de comando (configurada para PCs com pouca RAM; rode um Gradle por vez):

```powershell
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug --no-daemon --max-workers=1
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Para iterar mais rápido num aparelho arm64:

```powershell
.\gradlew.bat :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a --no-daemon --max-workers=1
adb install -r -t app/build/intermediates/apk/debug/app-debug.apk
```

- **Requisitos:** NDK 28.2.13676358 e CMake 3.22.1 (pelo SDK Manager), JDK 17.
- **ID do app:** `com.fabiosilva.ecosdeeldra`. Versão `0.6.1` (versionCode 8).

## Versão anterior

A versão WebView/Three.js, os mapas de masmorra, o editor de mapas e os testes web foram movidos para `legacy/`. Eles não entram no APK.

## Créditos

Mundo, texturas, personagens, sons e músicas são gerados pelo projeto. O modelo da arma em `app/src/main/assets/weapons/` foi convertido por `tools/convert_glb_to_zzm.py`; **[preencher origem e licença - decisão U1]**. Os personagens Kenney Blocky Characters (CC0) existem apenas em `legacy/`, com a licença em `legacy/notices/`.
~~~

---

## Apêndice B - Textos finais (copiar e colar)

| Onde | Texto |
|---|---|
| Mensagem de início (`onMissionReady`) | `DESTRUA NINHOS → RECOLHA CRISTAIS → EXTRAIA PELO PORTAL` |
| Dica fora do tutorial (`onMissionReady`) | `Explore: baús escondidos guardam diamantes e itens` |
| Ninho destruído | `NINHO 1/3 DESTRUÍDO! RECOLHA O CRISTAL` |
| Cristal recolhido | `CRISTAL 1/3 RECOLHIDO` |
| Portal aberto | `PORTAL ABERTO! EXTRAIA!` |
| Tela da fase (`StagesScreen`) | `Objetivo: Destrua ninhos → recolha cristais → extraia pelo portal.` + `★ vencer   ★ vida ≥ 50%   ★ tempo ≤ m:ss` |
| Carregamento (dica 0) | `Objetivo: destrua ninhos → recolha cristais → extraia pelo portal. Não é preciso matar todos os zumbis.` |
| Pausa (campanha, 2ª linha) | `DESTRUA NINHOS → RECOLHA CRISTAIS → EXTRAIA PELO PORTAL` |
| Legenda do mapa | `◆ ninho   ✕ destruído   cristal (ciano)   ◉ portal   ■ suprimentos   ▲ você` |
| Pílulas do HUD | `NINHOS 0/3` → `CRISTAIS 0/3` → `PORTAL` (`ABERTO` quando abre) |
| Ação atual (rótulo opcional) | `DESTRUA O PRÓXIMO NINHO` / `RECOLHA O CRISTAL` / `ENTRE NO PORTAL!` |
| Botão de Ajustes | `▶ REVER TUTORIAL` |
| Popup de rever | Título `Rever tutorial`; corpo `Vamos jogar a fase 1 com as dicas passo a passo. Seu progresso e suas fases vencidas não mudam.`; botões `COMEÇAR` / `CANCELAR` |
| Missões diárias / conquistas | `Recolha 3 cristais nas fases`, `Recolha N cristais nas fases` |
