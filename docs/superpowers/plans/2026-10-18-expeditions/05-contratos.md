# Etapa 4 — Contratos de expedição, preparação e resultado legível

> Documento de design para o agente implementador. Base: branch `feature/ilhas-infinitas`, árvore de trabalho de 18/10/2026 (merge de dificuldade adaptativa ainda não commitado, `StoryOverlay` já removido, intersticial a cada 5º toque em JOGAR).
> Spec-fonte: `docs/superpowers/specs/2026-10-18-zombie-zone-expeditions-design.md` (seção "Segundo projeto: decisões de expedição" e etapa 4 da "Ordem de implementação").
> Números de linha citados valem para a árvore de trabalho de 18/10; **sempre localize pelo trecho de texto**, as linhas deslocam quando outras áreas editam os mesmos arquivos.

## 0. Resumo executivo (decisões já tomadas)

1. **Contrato = recompensa explícita + regra de risco + objetivo opcional.** Modelo 100% Kotlin puro (`data/Contracts.kt`), testável sem Android. Os textos de custo/recompensa que o jogador lê são **gerados dos mesmos números** que o motor recebe (um teste garante que não divergem).
2. **Subconjunto mínimo viável desta etapa (o que realmente muda o motor):**
   - **Noite vermelha** (primeira variação, barata e clara): céu e névoa vermelhos com visão curta, zumbis mais atentos/rápidos, recompensa maior. Custo nativo ≈ 60 linhas.
   - **Recuperação silenciosa** (segunda): poucos inimigos, **munição escassa com reserva real** (hoje a recarga é infinita; é preciso criar a reserva), bônus por poucos zumbis alertados. Custo nativo ≈ 160 linhas. É também o alicerce da "munição limitada" do kit de campo da etapa 6.
   - **Resistência** (sobrevivência como variante de expedição): sem mudança de regra nativa; passa a usar a mesma tela de preparação, o mesmo plano/código e o mesmo histórico.
   - **Ninho instável** e **Caixa perdida** ficam **modelados, com ids, textos, recompensa e codec prontos, mas escondidos** (`engineReady = false`) até a etapa 4C/4D (seções 5.6 e 5.7 trazem os pontos exatos do motor).
3. **Tela de PREPARAÇÃO** nova (`ui/ExpeditionPrepScreen.kt`): modo (Campanha / Sobrevivência), lista de contratos com custo e recompensa, equipamento (barra de itens + atalho para a mochila), **código da ilha** (copiar, nova ilha, colar código). Entra entre os botões JOGAR/SOBREVIVER e o carregamento. Antes da primeira vitória o jogo **parte direto** (novo jogador não vê contratos).
4. **Tela de RESULTADO** nova (`ui/ExpeditionResultOverlay.kt`) **estende** o `VictoryOverlay` atual: objetivos cumpridos x pedidos, recompensa em duas colunas (EXPEDIÇÃO / CONTRATO), código da ilha. `DefeatOverlay` e `SurvivalResultOverlay` ganham o chip do contrato e o código.
5. **Recompensa NÃO monetizada:** nenhum anúncio para escolher, liberar ou receber contrato; o botão DOBRAR do resultado dobra **somente a parte base** (o bônus do contrato nunca é dobrado); suprimentos por anúncio **não repõem a reserva de munição** do contrato. Detalhes e a única decisão aberta na seção 7.
6. **Código curto reproduzível** `ZZ-XXXX-XXXX-XXXXX` (13 símbolos base32, CRC-12). **Formato idêntico ao já esboçado em `scratchpad/ref_code.py` pela área de compartilhamento (etapa 5)**; este documento fixa o formato e a API Kotlin (`ExpeditionCode`). Quem implementar primeiro cria o arquivo; a etapa 5 só consome (seção 4).
7. **Persistência** aditiva e retrocompatível: um único campo `expeditions` (string compacta) no `SaveStore`, parseada por código puro testável. Sem bump de versão de save.
8. **Reservas de índices no protocolo** para não colidir com outras áreas: `Cfg` 40..55, `Hud` 700..719 (`H_SIZE` passa a 720), `Res` 24..31 (`R_SIZE` passa a 32), `Ev` 40 (seção 5.1).
9. **Restrições do projeto respeitadas:** IDs reais do AdMob intocados; sem emulador (testar no aparelho por adb); Gradle sempre `--no-daemon --max-workers=1`; sem paredes/labirintos; sem backend.

## 1. Fatos do código atual que sustentam este projeto

| Fato | Onde | Consequência |
|---|---|---|
| A recarga é **infinita**: ao terminar, `ammo_ = magazine;`. Não existe reserva de munição. | `game.cpp` ~L861-871 (`updateWeapon`) | "Munição escassa" exige **criar** a reserva (seção 5.3). |
| Cada arma guarda o próprio pente em `clips_[16]`/`clipsInit_[16]`. | `game_items.cpp` L180-190, `game.h` L565-566 | A reserva é medida em **pentes** (centésimos de pente), neutra entre armas. |
| Baús, suprimentos (anúncio) e `revive()` enchem o pente. | `game_mission.cpp` L306; `game.cpp` L1277-1283 e L199 | Três pontos a adaptar quando a reserva é limitada. |
| `alerted` vira `true` em 5 lugares do código de zumbis e 1 no de ondas. | `game_zombies.cpp` L177, L236, L298, L558, L733, L880; `game_survival.cpp` L109 | Contar alertas = centralizar numa função `alertZombie()`. |
| Visão do zumbi: `sight = night ? 14 : 21` (+6 atiradores, x0,6 agachado); alerta de perto 6,5 m (3,5 agachado). | `game_zombies.cpp` L290-309 | Agressividade da Noite vermelha = multiplicador nesses números. |
| A névoa vem do tema (`theme_.sky.fogStart/fogEnd`) e `buildScene` já limita por `viewRadius_`. | `game.cpp` L1626-1634; `terrain.cpp` L341-377 (`applySkyVariant`) | Visão curta = ajustar o tema **antes** de `renderer_.setTheme` em `startMission`. |
| O terreno da ilha **não** lê `theme.sky` (só campos de bioma). | `island.cpp`/`theme.h` | Mudar o céu não altera a ilha: a mesma semente+fase gera a mesma ilha em qualquer contrato. |
| Guardas de POI sorteados com `Rng prng(hash2(cell, seed+99))`; os baús do POI usam o mesmo `prng` depois do laço de guardas. | `game_mission.cpp` L173-198 | Escalar guardas **sem consumir menos números aleatórios** (seção 5.4) para não mudar os baús. |
| A semente é `Random.nextInt(1, 16_000_000)` e viaja num `float` (exato até 2^24). | `MainActivity.kt` L526-527 | Semente cabe em 24 bits; o plano passa a **ser dono** da semente (necessário para o código). |
| Sobrevivência usa `Campaign.survivalDifficulty(p)`, bioma aleatório `Random(0,4)*5+1`, ilha sem ninhos/portal (`objectives=false`). | `MainActivity.kt` L942-949; `game_mission.cpp` L30 | Bioma e semente entram no plano/código. |
| **`SurvivalScreen` hoje não é alcançável** a partir da Home (nenhum botão chama `showSurvival()`; só as saídas de uma partida de sobrevivência). | `HomeScreen.kt` (sem entrada), grep de `showSurvival` | A tela de preparação, com abas Campanha/Sobrevivência, **devolve** o acesso ao modo sem criar botão novo na Home. |
| JOGAR (Home) e JOGAR (Fases) chamam `startCampaignPlay`, que conta o toque para o intersticial do 5º toque. | `HomeScreen.kt` L270, `StagesScreen.kt` L74, `MainActivity.kt` L465-496 | O contador passa a ser contado em **PARTIR**; JOGAR só abre a preparação. |
| `Rules.applyResult`/`applySurvivalResult` creditam diamantes e atualizam estatísticas; testes existentes constroem `MissionResult` posicionalmente. | `Progress.kt` L281-328, L493-518; `RulesTest.kt` | **Não alterar** essas funções nem `MissionResult`: o bônus do contrato é aplicado por uma função nova (`ExpeditionRules`) por cima delas. |
| `Hud.SIZE=660`, `Cfg.COUNT=28`, `Res.SIZE=15`; `H_WAVE_FLAGS=643` existe só no C++. | `protocol.h`, `NativeProtocol.kt` | Blocos novos em faixas reservadas (seção 5.1) e **teste de paridade** C++↔Kotlin (seção 10). |
| `showInventory(tab, back)` já aceita callback de retorno. | `MainActivity.kt` L411-417 | A preparação reabre a mochila e volta para si mesma. |

## 2. Escopo

### 2.1 Entra nesta etapa (ordem de entrega no capítulo 9)
- Modelo de contratos, plano de expedição, código, histórico, regras de recompensa (Kotlin puro + testes).
- Tela de preparação, resultado novo, chip do contrato no HUD, mensagens/dicas.
- Contratos **Noite vermelha** e **Recuperação silenciosa** no motor; **Resistência** na sobrevivência (inclusive Noite vermelha como variante de sobrevivência).
- Persistência (último contrato por modo, contadores, histórico dos últimos 40).
- Protocolo reservado para 4C/4D já declarado (campos `NEST_BOMB`, `CARGO`) mas **sem comportamento** até as sub-etapas diferidas.

### 2.2 Diferido (modelado e desenhado aqui, implementação em sub-etapas posteriores)
- **4C Ninho instável** (seção 5.6): ninhos geram Inchados; destruir todos concede um **módulo** (item colecionável persistido, consumido pelas receitas da etapa 6).
- **4D Caixa perdida** (seção 5.7): carga opcional a localizar antes da extração.

### 2.3 Fora de escopo
Cartão de resultado em imagem e compartilhamento nativo/QR (etapa 5; esta etapa só entrega o código, o texto `Contracts.shareText` e o histórico), recursos/deployables (etapa 6), normalização das melhorias do jogador em desafios por código (ver risco R9), qualquer mudança em PvP.

## 3. Modelo Kotlin puro (`app/src/main/java/com/fabiosilva/ecosdeeldra/data/`)

Arquivos **novos** (nenhum edita `Progress.kt` além de uma linha):

| Arquivo | Conteúdo |
|---|---|
| `Contracts.kt` | `ExpeditionKind`, `ContractId`, `ExpeditionPlan`, `EngineRules`, `RewardTuning`, `ContractDef`, `ContractCard`, `ContractRun`, `ContractReward`, `PlanCheck`, `object Contracts` (catálogo, regras do motor, recompensa, textos, desbloqueio, validação, dicas de equipamento) |
| `ExpeditionCode.kt` | `object ExpeditionCode` (codificar, decodificar, extrair do texto, mensagens de erro) |
| `ExpeditionLog.kt` | `ExpeditionOutcome`, `ExpeditionRecord`, `class ExpeditionLog` (histórico, contadores, módulos, último contrato; codificação em texto) |
| `ExpeditionRules.kt` | `object ExpeditionRules` (`applyCampaign`, `applySurvival`), `ExpeditionSummary`, `SurvivalExpeditionSummary` |

Arquivos **editados**: `Progress.kt` (uma linha: `val expeditions = ExpeditionLog()` dentro de `class Progress`), `SaveStore.kt` (duas linhas, ver cap. 8).

### 3.1 Tipos centrais

```kotlin
package com.fabiosilva.ecosdeeldra.data

import kotlin.math.min
import kotlin.math.roundToInt
import kotlin.random.Random

/** ORDEM FIXA: o ordinal vai no código da expedição (0 campanha, 1 sobrevivência). */
enum class ExpeditionKind { CAMPAIGN, SURVIVAL }

/** O número vai no código da expedição e no protocolo do motor: nunca reutilize nem renumere. */
enum class ContractId(val code: Int) {
    NONE(0), SILENT_RECOVERY(1), UNSTABLE_NEST(2), RED_NIGHT(3), LOST_CRATE(4), ENDURANCE(5);

    companion object {
        fun fromCode(code: Int): ContractId? = entries.firstOrNull { it.code == code }
    }
}

/** O que será jogado: tipo, fase (ou bioma, na sobrevivência), semente da ilha e contrato. */
data class ExpeditionPlan(
    val kind: ExpeditionKind,
    val stage: Int,                 // campanha: número da fase; sobrevivência: 1, 6, 11 ou 16 (bioma)
    val seed: Int,                  // 1..ExpeditionCode.SEED_MAX; cabe exato em float
    val contract: ContractId,
    val sharedSeed: Boolean = false, // veio de um código: "tentar de novo" mantém a ilha
) {
    val code: String? get() = ExpeditionCode.encode(this)

    /** Nova tentativa: ilha nova, salvo quando a ilha veio de um código compartilhado. */
    fun retry(random: Random = Random.Default): ExpeditionPlan =
        if (sharedSeed) this else copy(seed = Contracts.randomSeed(random))

    companion object {
        fun standard(stage: Int, random: Random = Random.Default) =
            ExpeditionPlan(ExpeditionKind.CAMPAIGN, stage.coerceAtLeast(1), Contracts.randomSeed(random), ContractId.NONE)

        fun survival(contract: ContractId = ContractId.ENDURANCE, random: Random = Random.Default) =
            ExpeditionPlan(ExpeditionKind.SURVIVAL, random.nextInt(0, 4) * Campaign.STAGES_PER_ZONE + 1, Contracts.randomSeed(random), contract)
    }
}
```

`ExpeditionPlan.survival()` preserva exatamente o sorteio de bioma que hoje está em `MainActivity.startSurvival()` (L946).

### 3.2 Regras que viajam ao motor e ajustes de dificuldade

```kotlin
/** Valores neutros = expedição padrão. 0 em ammoClips = reserva ilimitada (convenção do protocolo). */
data class EngineRules(
    val zombieCountMul: Float = 1f,
    val zombieSpeedMul: Float = 1f,
    val zombieDamageMul: Float = 1f,
    val zombieHealthMul: Float = 1f,
    val guardMul: Float = 1f,        // guardas dos pontos de interesse (nativo)
    val ammoClips: Float = 0f,       // reserva em pentes; 0 = ilimitada
    val chestAmmoPct: Int = 0,       // reserva ganha por baú, em centésimos de pente
    val visionMul: Float = 1f,       // x alcance da névoa
    val aggression: Float = 1f,      // x percepção e persistência dos zumbis
    val redSky: Boolean = false,
    val nestBomb: Float = 0f,        // 4C: chance de o ninho gerar Inchado
    val cargo: Boolean = false,      // 4D: existe carga opcional
) {
    /** Só o que é multiplicável no lado Kotlin; o resto vai pelo protocolo. */
    fun applyTo(d: Difficulty): Difficulty = d.copy(
        zombieCount = (d.zombieCount * zombieCountMul).roundToInt().coerceAtLeast(3),
        zombieHealth = d.zombieHealth * zombieHealthMul,
        zombieDamage = d.zombieDamage * zombieDamageMul,
        zombieSpeed = d.zombieSpeed * zombieSpeedMul,
    )
}
```

`Difficulty` já é `data class` (Campaign.kt L24): `copy` funciona. **Não** use nem reintroduza receita de anúncio: `applyTo` recebe a `Difficulty` que o `launchMission` já calcula (qualquer que seja a assinatura de `Campaign.difficulty` depois da limpeza da área de dificuldade).

### 3.3 Catálogo e tabela de afinação (valores iniciais; calibrar no aparelho)

```kotlin
enum class ObjectiveKind { NONE, SILENCE, CARGO, ALL_NESTS_MODULE }

data class RewardTuning(
    val extractionBase: Int = 0, val extractionPerStage: Int = 0, val extractionCap: Int = 0, // ◆ ao extrair (campanha)
    val runPct: Int = 0,                      // +% sobre abates + baús + exploração (campanha) ou sobre a recompensa de ondas
    val objectiveMax: Int = 0,                // teto do bônus do objetivo opcional
    val objectivePenalty: Int = 0,            // SILENCE: perda por zumbi alertado
    val moduleId: String? = null,             // ALL_NESTS_MODULE
)

data class ContractDef(
    val id: ContractId,
    val name: String,
    val tagline: String,
    val color: Int,                 // ARGB (sem android.graphics.Color: o arquivo é testável na JVM)
    val kinds: Set<ExpeditionKind>,
    val unlockStage: Int,           // libera quando highestUnlockedStage() >= unlockStage
    val engineReady: Boolean,       // false = escondido na UI e recusado ao colar código
    val danger: Int,                // 0..3 (bolinhas na lista)
    val objective: ObjectiveKind,
    val reward: RewardTuning,
)
```

| id | nome | tipos | libera | engineReady | perigo | regras do motor (`engineRules(id, stage)`) | recompensa (`RewardTuning`) |
|---|---|---|---|---|---|---|---|
| 0 NONE | Expedição padrão | Campanha | 1 | sim | 1 | neutro | nada extra |
| 1 SILENT_RECOVERY | Recuperação silenciosa | Campanha | 2 | sim | 2 | `zombieCountMul=0.55`, `guardMul=0.5`, `ammoClips=min(7, 3.0+0.1*min(stage,40))`, `chestAmmoPct=50` | extração `20 + 1*fase` (teto 80); `runPct=25`; objetivo SILENCE `max 60`, `-3` por alerta |
| 2 UNSTABLE_NEST | Ninho instável | Campanha | 4 | **não (4C)** | 2 | `nestBomb=0.35` | extração `10 + 1*fase` (teto 60); `runPct=30`; módulo `"nucleo_instavel"` |
| 3 RED_NIGHT | Noite vermelha | Campanha + Sobrevivência | 3 | sim | 3 | `zombieSpeedMul=1.10`, `zombieDamageMul=1.15`, `visionMul=0.45`, `aggression=1.35`, `redSky=true` | extração `25 + 2*fase` (teto 125; **zero na sobrevivência**); `runPct=50` |
| 4 LOST_CRATE | Caixa perdida | Campanha | 3 | **não (4D)** | 2 | `cargo=true` | extração `10 + 1*fase` (teto 60); objetivo CARGO `max 60` |
| 5 ENDURANCE | Resistência | Sobrevivência | 1 | sim | 2 | neutro | nada extra (a recompensa de ondas já existe em `applySurvivalResult`) |

Notas de afinação (todas tunáveis só em Kotlin, exceto os fatores nativos de percepção):
- `fase` na fórmula de extração é `min(fase, 60)` (mesmo teto de `Campaign.stage()`), para a recompensa não escalar sem fim.
- **Bônus de contrato só existe em vitória** (extração pelo portal). Derrota/abandono: zero de contrato, as recompensas base de abates/baús/exploração continuam como hoje.
- Na sobrevivência a "vitória" é sempre verdadeira (a partida só acaba ao cair): contrato paga `runPct`% da recompensa de ondas, sem valor fixo.
- `unlockStage` é só progressão de apresentação (o novo jogador vê pouca coisa); não bloqueia nada por receita de anúncio.

Funções do `object Contracts` (assinaturas finais):

```kotlin
object Contracts {
    const val SEED_RANDOM_MAX = 16_000_000                       // faixa que o motor já usa (exclusiva)
    fun randomSeed(random: Random = Random.Default): Int = random.nextInt(1, SEED_RANDOM_MAX)

    val all: List<ContractDef>
    fun get(id: ContractId): ContractDef
    fun engineRules(id: ContractId, stage: Int): EngineRules

    // apresentação e desbloqueio
    fun prepUnlocked(p: Progress): Boolean = p.stats.wins > 0 || p.highestUnlockedStage() >= 2
    fun options(kind: ExpeditionKind, p: Progress): List<ContractOption>   // só engineReady && kind; com `locked`
    fun defaultContract(kind: ExpeditionKind, p: Progress): ContractId      // último usado, se ainda válido
    fun card(id: ContractId, kind: ExpeditionKind, stage: Int): ContractCard
    fun briefing(plan: ExpeditionPlan): String?                              // 1 linha para o HUD ao começar
    fun loadoutAdvice(id: ContractId, inventory: Inventory): String?
    fun shareText(plan: ExpeditionPlan): String?                             // para a etapa 5 reutilizar

    // validação de um plano (código colado ou retomado)
    fun check(plan: ExpeditionPlan, p: Progress): PlanCheck

    // recompensa
    fun reward(plan: ExpeditionPlan, victory: Boolean, runBase: Int, run: ContractRun): ContractReward
}

data class ContractOption(val def: ContractDef, val locked: Boolean, val lockReason: String?)
data class PlanCheck(val ok: Boolean, val message: String?)               // message em português para o Popup
data class ContractRun(val alerts: Int = 0, val cargoCollected: Boolean = false, val nestsDestroyed: Int = 0, val nestsTotal: Int = 0)
data class ContractReward(val extraction: Int = 0, val runBonus: Int = 0, val objective: Int = 0, val moduleId: String? = null) {
    val total: Int get() = extraction + runBonus + objective
    companion object { val NONE = ContractReward() }
}
```

`Contracts.reward` (única fonte do cálculo):

```kotlin
fun reward(plan: ExpeditionPlan, victory: Boolean, runBase: Int, run: ContractRun): ContractReward {
    val def = get(plan.contract)
    if (!victory || plan.contract == ContractId.NONE) return ContractReward.NONE
    val t = def.reward
    val extraction = if (plan.kind == ExpeditionKind.CAMPAIGN)
        min(t.extractionCap, t.extractionBase + t.extractionPerStage * min(plan.stage, 60)) else 0
    val runBonus = runBase.coerceAtLeast(0) * t.runPct / 100
    val objective = when (def.objective) {
        ObjectiveKind.SILENCE -> (t.objectiveMax - t.objectivePenalty * run.alerts).coerceIn(0, t.objectiveMax)
        ObjectiveKind.CARGO -> if (run.cargoCollected) t.objectiveMax else 0
        else -> 0
    }
    val module = if (def.objective == ObjectiveKind.ALL_NESTS_MODULE && run.nestsTotal > 0 && run.nestsDestroyed >= run.nestsTotal) t.moduleId else null
    return ContractReward(extraction, runBonus, objective, module)
}
```

`runBase` é: campanha = `base.killBonus + base.chestBonus + base.exploreBonus` (exclui primeira vitória, estrelas e cristais, que são progressão de uma vez só); sobrevivência = `SurvivalSummary.reward`.

### 3.4 Textos de custo/recompensa (gerados dos números)

`Contracts.card(id, kind, stage)` devolve:

```kotlin
data class ContractCard(
    val title: String, val tagline: String,
    val risks: List<String>,      // seção "CUSTO / RISCO"
    val rewards: List<String>,    // seção "RECOMPENSA"
    val objective: String?,       // seção "OBJETIVO OPCIONAL"
    val danger: Int,
    val summary: String,          // uma linha para a lista: "◆ +45 ao extrair • +50% no saque • visão curta"
)
```

Formatação **sem depender do locale da JVM** (testes rodam em qualquer máquina): `private fun num1(v: Float) = "%.1f".format(java.util.Locale.US, v).replace('.', ',')` e `private fun pct(v: Float) = (v * 100f).roundToInt()`.

Textos finais (os números entre `${}` vêm de `engineRules`/`RewardTuning`):

- **Recuperação silenciosa** — *"Entre, pegue o que precisa e saia sem acordar a ilha."*
  - Custo: `"Poucos inimigos: ${pct(1-countMul)}% menos zumbis e ${pct(1-guardMul)}% menos guardas nos ninhos"`; `"Munição escassa: ${num1(ammoClips)} pentes de reserva (cada baú dá +${num1(chestAmmoPct/100f)} pente)"`; `"Suprimentos por anúncio só completam o pente atual — não repõem a reserva"`.
  - Recompensa: `"Ao extrair: ◆ +${extração}"`; `"+${runPct}% sobre abates, baús e exploração"`.
  - Objetivo opcional: `"Silêncio: até ◆ +${max}, −◆ ${penalty} por zumbi alertado (os gerados pelos ninhos não contam)"`.
- **Noite vermelha** — *"A lua sangra. Você enxerga pouco; eles, não."*
  - Custo: `"Visão curta: névoa a ${pct(visionMul)}% do alcance normal"`; `"Zumbis mais atentos: percebem você de mais longe e perseguem por mais tempo (+${pct(aggression-1)}%)"`; `"Zumbis ${pct(speed-1)}% mais rápidos e ${pct(damage-1)}% mais fortes"`.
  - Recompensa (campanha): `"Ao extrair: ◆ +${extração}"`, `"+${runPct}% sobre abates, baús e exploração"`. Na sobrevivência: só `"+${runPct}% na recompensa de ondas e abates"`.
  - Sem objetivo opcional.
- **Resistência** — *"Sobreviva às ondas. Cada onda vencida paga."* Custo: `"Sem ninhos nem portal: a expedição só termina quando você cai"`, `"A cada 5 ondas vem uma GRANDE HORDA"`. Recompensa: `"◆ 8 por onda vencida e ◆ 1 a cada 2 abates"`, `"+◆ 30 ao bater o recorde (60 s ou mais)"` — os 8/2/30 duplicam literais de `Rules.applySurvivalResult`; o teste `survivalCardMatchesRules` roda a regra real e compara.
- **Expedição padrão** — *"Destrua os ninhos, pegue os cristais e extraia pelo portal."* Sem custo extra; recompensa: `"Recompensas normais da fase"`.
- **Ninho instável** (oculto) — *"Os ninhos vazam gás: parte dos filhotes explode."* Custo: `"${pct(nestBomb)}% dos zumbis dos ninhos são Inchados (explodem ao chegar perto)"`. Recompensa: extração, `runPct`, `"Destruir todos os ninhos: módulo raro Núcleo instável"`.
- **Caixa perdida** (oculto) — *"Uma carga caiu em algum ponto da ilha. Ache-a antes de extrair."* Custo: `"A carga fica longe da rota dos ninhos: exige desvio"`. Objetivo: `"Carga: ◆ +${max} se for encontrada antes de entrar no portal"`.

`Contracts.loadoutAdvice` (puro sobre `Inventory`): Silenciosa → se `granadas+molotovs+fumaças < 2`: *"Leve granadas e fumaça: a reserva de munição é pequena."*; Noite vermelha → se `count(MEDKIT)+count(BANDAGE) == 0`: *"Sem curas na mochila — você só enxerga de perto."*; Resistência → se `weaponCount() < 2`: *"Mais de uma arma ajuda nas ondas."*; caso contrário `null`.

`Contracts.briefing(plan)` (dica de 1 linha no começo da partida): Noite vermelha → `"NOITE VERMELHA: visão curta e zumbis atentos • extraia para receber ◆"`; Silenciosa → `"SILÊNCIO: poupe munição, evite alertar • extraia para receber ◆"`; Resistência/padrão → `null` (o `onMissionReady` já tem as mensagens atuais).

### 3.5 Desbloqueio e validação

- `prepUnlocked(p)`: `p.stats.wins > 0 || p.highestUnlockedStage() >= 2`. Antes disso `showExpeditionPrep` não abre a tela e parte direto (primeira expedição = tutorial jogável sem escolhas).
- `options(kind, p)`: contratos com `engineReady && kind in def.kinds`, ordenados como a tabela; `locked = p.highestUnlockedStage() < def.unlockStage` com `lockReason = "Libera na fase ${unlockStage}"`.
- `check(plan, p)` (usado ao colar código **e** ao retomar):
  1. contrato incompatível com o tipo → *"Este código mistura tipo e contrato inválidos."*
  2. `!def.engineReady` → *"O contrato ${nome} ainda não está disponível nesta versão do jogo."*
  3. campanha com `plan.stage > p.highestUnlockedStage()` → *"Este código é da fase ${plan.stage}. Conclua a fase ${plan.stage - 1} para jogá-lo."* **Regra anti-exploit:** sem isso um código de fase 60 pagaria `firstClearDiamonds` (até ◆ 640) e estrelas a um jogador novo (`Rules.applyResult` credita `record(stage)` sem checar a ordem).
  4. contrato bloqueado por progressão → *"Contrato liberado na fase ${unlockStage}."*
  5. senão `ok`.

### 3.6 Histórico e contadores (`ExpeditionLog.kt`)

```kotlin
enum class ExpeditionOutcome { VICTORY, DEFEAT }

data class ExpeditionRecord(
    val code: String,                 // "-" se a fase passou de 1023
    val kind: ExpeditionKind, val stage: Int, val contract: ContractId,
    val outcome: ExpeditionOutcome,
    val seconds: Int, val kills: Int, val alerts: Int,
    val contractDiamonds: Int, val totalDiamonds: Int,
    val day: String,                  // yyyy-MM-dd (MainActivity.today())
)

class ExpeditionLog {
    var lastCampaign = ContractId.NONE
    var lastSurvival = ContractId.ENDURANCE
    val records = ArrayDeque<ExpeditionRecord>()       // mais recente primeiro, no máximo MAX_RECORDS
    val modules = linkedMapOf<String, Int>()           // 4C: "nucleo_instavel" -> quantidade
    fun playsOf(id: ContractId): Int
    fun winsOf(id: ContractId): Int
    fun lastOf(id: ContractId): ExpeditionRecord?
    fun remember(plan: ExpeditionPlan)                 // atualiza lastCampaign/lastSurvival
    fun add(r: ExpeditionRecord)                       // soma plays/wins, corta no limite
    fun grantModule(id: String)
    fun encode(): String
    fun load(text: String)                             // tolerante: lixo vira padrão, nunca lança
    companion object { const val MAX_RECORDS = 40 }
}
```

Codificação (uma `String`, só `[A-Za-z0-9 ~,;:|=.-]`, nada de JSON aninhado):

```
v1|lc=1|ls=5|pl=0,3,0,1,0,0,0,0|wi=0,1,0,1,0,0,0,0|mo=nucleo_instavel:1|r=ZZ-8080-Y4G0-1011S~V~0~2~1~212~19~6~38~118~2026-10-18;ZZ-...~D~...
```
Registro: `código~V|D~tipo(0/1)~fase~contrato~segundos~abates~alertas~◆contrato~◆total~dia`. Campos desconhecidos ou registros malformados são **ignorados** (cada campo numérico usa `toIntOrNull() ?: 0`; contrato inválido derruba só o registro). `pl`/`wi` têm 8 posições (ids 0..7).

### 3.7 `ExpeditionRules.kt` (aplica o contrato por cima de `Rules`)

```kotlin
data class ExpeditionSummary(val base: RewardSummary, val contract: ContractReward, val record: ExpeditionRecord) {
    val total: Int get() = base.total + contract.total
    /** O que o anúncio pode dobrar. O bônus do contrato NUNCA entra. */
    val doublable: Int get() = base.total
}
data class SurvivalExpeditionSummary(val base: SurvivalSummary, val contract: ContractReward, val record: ExpeditionRecord) {
    val total: Int get() = base.reward + contract.total
    val doublable: Int get() = base.reward
}

object ExpeditionRules {
    fun applyCampaign(p: Progress, plan: ExpeditionPlan, stage: Stage, r: MissionResult, run: ContractRun, day: String): ExpeditionSummary {
        val base = Rules.applyResult(p, stage, r)               // já credita base.total em p.diamonds
        val runBase = base.killBonus + base.chestBonus + base.exploreBonus
        val bonus = Contracts.reward(plan, r.victory, runBase, run)
        p.diamonds += bonus.total
        bonus.moduleId?.let { p.expeditions.grantModule(it) }
        val record = ExpeditionRecord(plan.code ?: "-", plan.kind, plan.stage, plan.contract,
            if (r.victory) ExpeditionOutcome.VICTORY else ExpeditionOutcome.DEFEAT,
            r.elapsedSeconds.toInt(), r.kills, run.alerts, bonus.total, base.total + bonus.total, day)
        p.expeditions.add(record)
        return ExpeditionSummary(base, bonus, record)
    }

    fun applySurvival(p: Progress, plan: ExpeditionPlan, name: String, day: String, r: SurvivalResult, run: ContractRun): SurvivalExpeditionSummary {
        val base = Rules.applySurvivalResult(p, name, day, r)   // já credita base.reward
        val bonus = Contracts.reward(plan, victory = true, runBase = base.reward, run = run)
        p.diamonds += bonus.total
        val record = ExpeditionRecord(plan.code ?: "-", plan.kind, plan.stage, plan.contract, ExpeditionOutcome.VICTORY,
            r.seconds.toInt(), r.kills, run.alerts, bonus.total, base.reward + bonus.total, day)
        p.expeditions.add(record)
        return SurvivalExpeditionSummary(base, bonus, record)
    }
}
```

Em derrota `Contracts.reward` devolve `ContractReward.NONE`, então `p.diamonds` só recebe o que `Rules.applyResult` já dava. O histórico registra a derrota (útil para a etapa 5).

## 4. Código da expedição (`ExpeditionCode.kt`)

### 4.1 Formato (fixo — compatível com `scratchpad/ref_code.py` da etapa 5)

Carga útil de **53 bits**, MSB primeiro, seguida de **CRC-12** (12 bits): 65 bits = **13 símbolos** de 5 bits.

| Campo | Bits | Valor |
|---|---|---|
| `ver` | 2 | `1` (versão do formato) |
| `kind` | 1 | 0 campanha, 1 sobrevivência (`ExpeditionKind.ordinal`) |
| `stage` | 10 | 1..1023 (campanha: fase; sobrevivência: 1, 6, 11 ou 16) |
| `seed` | 24 | 1..16 777 215 (o app sorteia < 16 000 000) |
| `contract` | 8 | `ContractId.code` |
| `mods` | 8 | **sempre 0 na versão 1** (reservado; valor ≠ 0 é recusado como "código de versão mais nova") |
| `crc` | 12 | CRC-12, polinômio `0x80F`, valor inicial `0xFFF`, MSB primeiro, sem inversão final, sobre os 53 bits |

Alfabeto (Crockford, sem I, L, O, U): `0123456789ABCDEFGHJKMNPQRSTVWXYZ`. Exibição: `ZZ-` + grupos 4-4-5, p.ex. `ZZ-80W0-Y4G0-100RS`. Fases acima de 1023 não têm código (`encode` devolve `null`; a UI esconde o bloco do código).

### 4.2 API

```kotlin
object ExpeditionCode {
    const val FORMAT_VERSION = 1
    const val STAGE_MAX = 1023
    const val SEED_MAX = 0xFF_FFFF

    enum class Problem { EMPTY, LENGTH, ALPHABET, CHECKSUM, VERSION, STAGE, SEED, CONTRACT, KIND_CONTRACT, MODS }
    sealed interface Decoded {
        data class Ok(val plan: ExpeditionPlan) : Decoded        // sharedSeed = true
        data class Bad(val problem: Problem) : Decoded
    }

    fun encode(plan: ExpeditionPlan): String?
    fun decode(text: String): Decoded
    fun extract(text: String): String?        // acha "ZZ-XXXX-XXXX-XXXXX" dentro de uma mensagem colada
    fun message(problem: Problem): String     // português para o Popup/erro do campo
}
```

Esqueleto (as constantes e o CRC são a especificação; o resto é mecânico):

```kotlin
private const val ALPHABET = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
private const val POLY = 0x80F
private const val PAYLOAD_BITS = 53
private const val CRC_BITS = 12
private const val SYMBOLS = 13

private fun crc12(bits: IntArray, count: Int): Int {
    var crc = 0xFFF
    for (i in 0 until count) {
        val top = (crc shr 11) and 1
        crc = (crc shl 1) and 0xFFF
        if ((top xor bits[i]) == 1) crc = crc xor POLY
    }
    return crc
}

fun encode(plan: ExpeditionPlan): String? {
    if (plan.stage !in 1..STAGE_MAX || plan.seed !in 1..SEED_MAX) return null
    val bits = IntArray(PAYLOAD_BITS + CRC_BITS)
    var at = 0
    fun put(value: Int, n: Int) { for (i in n - 1 downTo 0) bits[at++] = (value shr i) and 1 }
    put(FORMAT_VERSION, 2); put(plan.kind.ordinal, 1); put(plan.stage, 10)
    put(plan.seed, 24); put(plan.contract.code, 8); put(0, 8)
    put(crc12(bits, PAYLOAD_BITS), CRC_BITS)
    val raw = buildString { for (s in 0 until SYMBOLS) { var v = 0; for (k in 0 until 5) v = (v shl 1) or bits[s * 5 + k]; append(ALPHABET[v]) } }
    return "ZZ-${raw.substring(0, 4)}-${raw.substring(4, 8)}-${raw.substring(8)}"
}
```

`decode` normaliza: maiúsculas, remove `-` e qualquer espaço em branco (inclui `\n` colado), mapeia `O→0`, `I→1`, `L→1`, remove o prefixo `ZZ` **só quando o comprimento é 15** (um código de 13 símbolos pode começar legitimamente por "ZZ"), exige 13 símbolos do alfabeto, confere o CRC, depois valida **nesta ordem**: `ver == 1` (senão `VERSION`), `mods == 0` (`MODS`), `stage in 1..1023` (`STAGE`), `seed in 1..SEED_MAX` (`SEED`), contrato existente (`CONTRACT`), contrato compatível com o tipo (`KIND_CONTRACT`: `kind in Contracts.get(id).kinds`). O `ExpeditionPlan` devolvido tem `sharedSeed = true`. Disponibilidade no motor (`engineReady`) **não** é checada aqui, e sim em `Contracts.check`, para que códigos de contratos futuros continuem decodificáveis.

`extract` usa a regex `ZZ-[0-9A-HJKMNP-TV-Z]{4}-[0-9A-HJKMNP-TV-Z]{4}-[0-9A-HJKMNP-TV-Z]{5}` sobre o texto em maiúsculas.

Mensagens (`message`): EMPTY *"Digite ou cole um código."*; LENGTH *"O código tem 13 letras e números (ex.: ZZ-80W0-Y4G0-100RS)."*; ALPHABET *"Há um caractere inválido. O código só usa números e letras (sem I, L, O, U)."*; CHECKSUM *"O código não confere. Confira se digitou tudo certo."*; VERSION/MODS *"Este código vem de uma versão mais nova do jogo."*; STAGE/SEED/CONTRACT/KIND_CONTRACT *"Este código tem dados inválidos."*

### 4.3 Vetores dourados (gerados por `scratchpad/contract_vectors.py`, mesmo algoritmo)

| Caso | Código | CRC |
|---|---|---|
| campanha, fase 1, semente 1, padrão | `ZZ-8040-0008-003W3` | `F83` |
| campanha, fase 7, semente 123456, silenciosa | `ZZ-80W0-Y4G0-100RS` | `319` |
| campanha, fase 12, semente 15999999, noite vermelha | `ZZ-81KT-27ZR-302ZS` | `BF9` |
| sobrevivência, bioma 11, semente 4242, resistência | `ZZ-C1C0-114G-502VA` | `B6A` |
| limites: fase 1023, semente 16777215, caixa perdida | `ZZ-BZZZ-ZZZR-40262` | `8C2` |
| campanha, fase 3, semente 777, ninho instável | `ZZ-80C0-0628-20395` | `D25` |
| **dispositivo D1**: fase 2, semente 123456, silenciosa | `ZZ-8080-Y4G0-1011S` | `439` |
| **dispositivo D2**: fase 3, semente 123456, noite vermelha | `ZZ-80C0-Y4G0-300CK` | `193` |
| **dispositivo D3**: sobrevivência, bioma 1, semente 2024, resistência | `ZZ-C040-0FT0-50355` | `CA5` |
| **dispositivo D4**: sobrevivência, bioma 6, semente 99, noite vermelha | `ZZ-C0R0-00RR-301G3` | `603` |

Propriedades verificadas por força bruta no script: **toda** troca de um símbolo (13 posições x 31 alternativas) em cada vetor é rejeitada; 0 falsos positivos em 12 276 trocas de dois símbolos adjacentes no vetor 2. O teste `singleCharacterSubstitutionIsAlwaysRejected` repete a verificação em Kotlin.

## 5. Contratos no motor (C++/JNI)

### 5.1 Protocolo: blocos reservados e espelho em Kotlin

Para não colidir com outras áreas que também acrescentam campos (HUD, tutorial, desempenho, ficha), **esta área reserva faixas explícitas** e declara o resto como livre:

| Espaço | Faixa desta área | Livre para outros |
|---|---|---|
| `Cfg` / `ConfigIndex` | **40..55** (`C_COUNT = 56`) | 28..39 |
| `Hud` / `HudIndex` | **700..719** (`H_SIZE = 720`) | 644..699 |
| `Res` / `ResultIndex` | **24..31** (`R_SIZE = 32`) | 15..23 |
| `Ev` / `EventType` | **40** (`EV_CONTRACT`) | 26..39 |

Se outra área já tiver movido `C_COUNT`/`H_SIZE`/`R_SIZE` para um valor **maior** que o daqui, mantenha o maior e deixe os blocos desta área nas faixas acima (os valores explícitos `= 40`, `= 700`, `= 24` garantem que nada desloca).

**`protocol.h` (edições exatas):**

```cpp
// ConfigIndex — depois de C_EXPLOSIVES, substituir "C_COUNT" por:
    // Contrato de expedição (etapa 4). 0 = neutro/ilimitado; o JNI traduz.
    C_CONTRACT_ID = 40,   // 0 nenhum, 1 silenciosa, 2 ninho instável, 3 noite vermelha, 4 caixa perdida, 5 resistência
    C_AMMO_CLIPS,         // 41 reserva de munição em pentes (0 = ilimitada)
    C_CHEST_AMMO,         // 42 reserva por baú, em centésimos de pente
    C_VISION_MUL,         // 43 x alcance da névoa (0 = 1)
    C_AGGRESSION,         // 44 x percepção e persistência dos zumbis (0 = 1)
    C_RED_SKY,            // 45 1 = céu da noite vermelha
    C_GUARD_MUL,          // 46 x guardas dos pontos de interesse (0 = 1)
    C_NEST_BOMB,          // 47 (4C) chance de o ninho gerar Inchado
    C_CARGO,              // 48 (4D) 1 = existe carga opcional
    C_COUNT = 56

// HudIndex — depois de H_WAVE_FLAGS = 643, substituir "H_SIZE = 660" por:
    // Contrato de expedição (etapa 4): bloco reservado 700..719. Só vale quando H_CT_ID != 0.
    H_CT_ID = 700,        // ContractId da missão (0 = sem contrato)
    H_CT_RESERVE_ROUNDS,  // 701 balas de reserva para a arma em mãos (-1 = ilimitada)
    H_CT_RESERVE_CLIPS,   // 702 pentes de reserva (-1 = ilimitada)
    H_CT_ALERTS,          // 703 zumbis alertados até agora
    H_CT_CARGO_STATE,     // 704 (4D) 0 sem carga, 1 oculta, 2 revelada, 3 coletada
    H_CT_CARGO_X, H_CT_CARGO_Z,                         // 705, 706
    H_CT_CARGO_SX, H_CT_CARGO_SY, H_CT_CARGO_ON,        // 707..709 projeção na tela (0..1) e visibilidade
    H_CT_BOMBS,           // 710 (4C) Inchados gerados por ninhos até agora
    H_SIZE = 720

// EventType — depois de EV_WAVE = 25:
    EV_CONTRACT = 40,     // a = 1 sem reserva/sem munição, 2 último pente, 3 carga encontrada (4D), 4 carga entregue (4D), 5 Inchado gerado por ninho (4C; b = ninho)

// ResultIndex — substituir "R_SCORE, R_SIZE" por:
    R_SCORE,
    R_ALERTS = 24,        // zumbis alertados pelo jogador (sem ninhos/ondas)
    R_CARGO,              // 25 (4D) 1 = carga coletada
    R_RESERVE_PCT,        // 26 reserva de munição restante, centésimos de pente (-1 = ilimitada)
    R_BOMBS,              // 27 (4C) Inchados gerados por ninhos
    R_SIZE = 32
```

**`NativeProtocol.kt` (espelho):**

```kotlin
// object Cfg — depois de EXPLOSIVES = 27 (e antes de COUNT):
    const val CONTRACT_ID = 40
    const val AMMO_CLIPS = 41
    const val CHEST_AMMO = 42
    const val VISION_MUL = 43
    const val AGGRESSION = 44
    const val RED_SKY = 45
    const val GUARD_MUL = 46
    const val NEST_BOMB = 47
    const val CARGO = 48
    const val COUNT = 56        // era 28

// object Hud — antes de SIZE:
    const val CT_ID = 700
    const val CT_RESERVE_ROUNDS = 701
    const val CT_RESERVE_CLIPS = 702
    const val CT_ALERTS = 703
    const val CT_CARGO_STATE = 704
    const val CT_CARGO_X = 705
    const val CT_CARGO_Z = 706
    const val CT_CARGO_SX = 707
    const val CT_CARGO_SY = 708
    const val CT_CARGO_ON = 709
    const val CT_BOMBS = 710
    const val SIZE = 720        // era 660

// object Ev:
    const val CONTRACT = 40

// object Res (SIZE era 15):
    const val ALERTS = 24
    const val CARGO = 25
    const val RESERVE_PCT = 26
    const val BOMBS = 27
    const val SIZE = 32
```

**Aplicação no array de configuração** (novo arquivo `engine/ContractConfig.kt`, fora de `MainActivity` para encolher o diff do arquivo quente):

```kotlin
package com.fabiosilva.ecosdeeldra.engine

import com.fabiosilva.ecosdeeldra.data.EngineRules
import com.fabiosilva.ecosdeeldra.data.ExpeditionPlan

/** Escreve o bloco de contrato no FloatArray de startMission (valores neutros = 0). */
fun FloatArray.applyContract(plan: ExpeditionPlan, rules: EngineRules) {
    this[Cfg.CONTRACT_ID] = plan.contract.code.toFloat()
    this[Cfg.AMMO_CLIPS] = rules.ammoClips
    this[Cfg.CHEST_AMMO] = rules.chestAmmoPct.toFloat()
    this[Cfg.VISION_MUL] = rules.visionMul
    this[Cfg.AGGRESSION] = rules.aggression
    this[Cfg.RED_SKY] = if (rules.redSky) 1f else 0f
    this[Cfg.GUARD_MUL] = rules.guardMul
    this[Cfg.NEST_BOMB] = rules.nestBomb
    this[Cfg.CARGO] = if (rules.cargo) 1f else 0f
}
```

### 5.2 `MissionConfig` e JNI

`game.h` (struct `MissionConfig`, depois de `int explosives = 0;`):

```cpp
    // Contrato de expedição (etapa 4). Valores padrão = expedição padrão (PvP também usa o padrão).
    int contractId = 0;         // ContractId do Kotlin
    float ammoClips = -1.0f;    // reserva em pentes; < 0 = ilimitada
    int chestAmmoPct = 0;       // reserva ganha por baú (centésimos de pente)
    float visionMul = 1.0f;     // x alcance da névoa
    float aggression = 1.0f;    // x percepção/persistência dos zumbis
    bool redSky = false;
    float guardMul = 1.0f;      // x guardas dos pontos de interesse
    float nestBomb = 0.0f;      // 4C
    bool cargo = false;         // 4D
```

`jni_bridge.cpp` (`startMission`, depois de `cfg.explosives = ...`):

```cpp
    cfg.contractId = static_cast<int>(c[C_CONTRACT_ID]);
    cfg.ammoClips = c[C_AMMO_CLIPS] > 0 ? c[C_AMMO_CLIPS] : -1.0f;
    cfg.chestAmmoPct = static_cast<int>(c[C_CHEST_AMMO]);
    cfg.visionMul = c[C_VISION_MUL] > 0 ? c[C_VISION_MUL] : 1.0f;
    cfg.aggression = c[C_AGGRESSION] > 0 ? c[C_AGGRESSION] : 1.0f;
    cfg.redSky = c[C_RED_SKY] > 0.5f;
    cfg.guardMul = c[C_GUARD_MUL] > 0 ? c[C_GUARD_MUL] : 1.0f;
    cfg.nestBomb = c[C_NEST_BOMB];
    cfg.cargo = c[C_CARGO] > 0.5f;
```

(O padrão "0 = neutro" é o mesmo já usado em `reloadSpeed`, `speedMul` etc., e protege chamadas antigas/arrays zerados.)

### 5.3 Código novo: `game/contract_rules.h` (puro) e `game/game_contract.cpp`

`contract_rules.h` não inclui nada do jogo (só `<algorithm>`/`<cmath>`), para poder ser compilado num teste de host (seção 10.5):

```cpp
// Regras puras dos contratos de expedição (sem dependência do motor).
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
    if (missing == 0 || reservePct == 0) return {current, reservePct};
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

}  // namespace zz::contractrules
```

`game.h`: `#include "contract_rules.h"` junto dos outros includes; `struct Zombie` ganha `bool counted = false;  // já entrou na contagem de alertas do contrato`; antes de `class Game`: `Theme applyContractTheme(Theme t, const MissionConfig& c);`; na seção privada, depois dos métodos de sobrevivência:

```cpp
    // Contrato de expedição (game_contract.cpp)
    void startContractState();
    void updateContract(float dt);
    void alertZombie(Zombie& z);              // único lugar que vira alerted=true por percepção/ruído
    bool ammoLimited() const { return mode_ == Mode::Mission && reservePct_ >= 0; }
    bool canReloadNow(int magazine) const;
    void completeReload(int magazine);
    void notifyOutOfAmmo();
    void grantReserve(int pct);
    void publishContractHud(std::array<float, proto::H_SIZE>& h) const;
```

e nos membros (perto de "Sobrevivência"):

```cpp
    // Contrato de expedição
    int reservePct_ = -1;          // reserva de munição em centésimos de pente; < 0 = ilimitada
    int alerts_ = 0;               // zumbis alertados pelo jogador (sem ninhos nem ondas)
    float noAmmoTimer_ = 0;
    float contractLogTimer_ = 0;
```

`game_contract.cpp` completo (adicionar `game/game_contract.cpp` à lista de `add_library` em `CMakeLists.txt`, logo após `game/game.cpp`):

```cpp
// Contratos de expedição (etapa 4): céu/visão, reserva de munição, contagem de alertas e HUD do contrato.
#include <algorithm>
#include <cmath>

#include "../core/log.h"
#include "game.h"

namespace zz {

using namespace proto;

namespace {
constexpr int kMaxReservePct = 900;  // teto da reserva: 9 pentes
}  // namespace

// Roda em startMission ANTES de renderer_.setTheme()/generateBlockTextures(). Só mexe no céu/névoa:
// o terreno da ilha não lê theme.sky, então a mesma semente gera a mesma ilha em qualquer contrato.
Theme applyContractTheme(Theme t, const MissionConfig& c) {
    if (c.redSky) {
        t.sky.zenith = {0.16f, 0.015f, 0.03f};
        t.sky.horizon = {0.55f, 0.07f, 0.05f};
        t.sky.fog = {0.34f, 0.04f, 0.04f};
        t.sky.sunColor = {1.0f, 0.22f, 0.15f};
        t.sky.ambient = {0.42f, 0.17f, 0.17f};
        t.sky.sunIntensity = 0.40f;
        t.sky.night = 1.0f;  // estrelas, lanterna do jogador, +3 zumbis e mais Corredores (já existentes)
    }
    if (c.visionMul > 0.0f && c.visionMul != 1.0f) {
        t.sky.fogEnd = std::max(34.0f, t.sky.fogEnd * c.visionMul);
        t.sky.fogStart = std::min(t.sky.fogStart, t.sky.fogEnd * 0.3f);
    }
    return t;
}

void Game::startContractState() {
    alerts_ = 0;
    noAmmoTimer_ = 0;
    contractLogTimer_ = 0;
    const bool limited = mode_ == Mode::Mission && !survival() && config_.ammoClips > 0.0f;
    reservePct_ = limited ? std::min(kMaxReservePct, static_cast<int>(std::lround(config_.ammoClips * 100.0f))) : -1;
    LOGI("Contrato %d: reserva=%d%% visao=%.2f agressao=%.2f guardas=%.2f ceuVermelho=%d bomba=%.2f carga=%d",
         config_.contractId, reservePct_, config_.visionMul, config_.aggression, config_.guardMul,
         config_.redSky ? 1 : 0, config_.nestBomb, config_.cargo ? 1 : 0);
}

void Game::updateContract(float dt) {
    noAmmoTimer_ = std::max(0.0f, noAmmoTimer_ - dt);
    if (config_.contractId == 0) return;
    contractLogTimer_ -= dt;
    if (contractLogTimer_ <= 0.0f) {  // diagnóstico por logcat (seção 10.6)
        contractLogTimer_ = 5.0f;
        LOGI("Contrato %d: t=%.0fs alertas=%d reserva=%d%% pente=%d zumbis=%zu", config_.contractId, missionTime_, alerts_, reservePct_, ammo_, zombies_.size());
    }
}

// Único ponto que marca um zumbi como alertado por percepção, tiro, grito ou dano.
// Zumbis gerados já alertados (ninhos, ondas, chamados do Gritador) não passam por aqui e não contam.
void Game::alertZombie(Zombie& z) {
    if (z.alerted) return;
    z.alerted = true;
    z.lostSight = 0;
    if (!z.counted && z.nest < 0 && mode_ == Mode::Mission && !survival()) {
        z.counted = true;
        ++alerts_;
        if (config_.contractId != 0 && alerts_ <= 40) LOGI("Alerta #%d (tipo %d) a %.1f m", alerts_, static_cast<int>(z.kind), distXZ(z.body.position, player_.position));
    }
}

bool Game::canReloadNow(int magazine) const {
    return contractrules::canReload(ammo_, magazine, ammoLimited() ? reservePct_ : -1);
}

void Game::completeReload(int magazine) {
    const int limit = ammoLimited() ? reservePct_ : -1;
    const contractrules::ReloadResult r = contractrules::reload(ammo_, magazine, limit);
    ammo_ = r.ammo;
    if (limit < 0) return;
    const int before = reservePct_;
    reservePct_ = r.reservePct;
    if (reservePct_ == 0 && before > 0) emit(EV_CONTRACT, 1);                                // sem reserva
    else if (reservePct_ > 0 && reservePct_ <= 100 && before > 100) emit(EV_CONTRACT, 2);  // último pente
}

void Game::notifyOutOfAmmo() {
    if (noAmmoTimer_ > 0.0f) return;
    noAmmoTimer_ = 4.0f;
    emit(EV_CONTRACT, 1);
}

void Game::grantReserve(int pct) {
    if (reservePct_ < 0 || pct <= 0) return;
    reservePct_ = std::min(kMaxReservePct, reservePct_ + pct);
}

void Game::publishContractHud(std::array<float, H_SIZE>& h) const {
    h[H_CT_ID] = static_cast<float>(config_.contractId);
    h[H_CT_ALERTS] = static_cast<float>(alerts_);
    const bool limited = reservePct_ >= 0;
    h[H_CT_RESERVE_CLIPS] = limited ? reservePct_ / 100.0f : -1.0f;
    h[H_CT_RESERVE_ROUNDS] = limited ? static_cast<float>(contractrules::reserveRounds(reservePct_, magazineFor(weaponId()))) : -1.0f;
}

}  // namespace zz
```

### 5.4 Pontos exatos de alteração nos arquivos existentes

Cada item é pequeno e **neutro quando o contrato é "padrão"** (reserva ilimitada, `aggression = 1`, `redSky = false`, `guardMul = 1`): a expedição padrão continua idêntica ao jogo de hoje.

**N1 — `game_mission.cpp`, `Game::startMission` (L14-98).**
```cpp
// L19 — antes:
theme_ = applySkyVariant(themeForStage(config.stage), config.seed, config.stage >= 3);
// depois:
theme_ = applyContractTheme(applySkyVariant(themeForStage(config.stage), config.seed, config.stage >= 3), config);
// L78 — logo depois de "startSurvivalState();":
startContractState();
```
(`config_ = config;` já foi atribuído em L16.)

**N2 — `game_mission.cpp`, `Game::updatePois` (L173-187), escala de guardas sem mudar os números aleatórios:**
```cpp
int guards;
const bool guarded = !survival();
if (p->role == ROLE_NEST) guards = 4 + std::min(4, config_.stage / 3);
else if (p->role == ROLE_PORTAL) guards = 3 + std::min(3, config_.stage / 4);
else guards = prng.rangeInt(2, 4) + std::min(2, config_.stage / 5);
const int keep = contractrules::scaledGuards(guards, config_.guardMul);   // NOVO
for (int i = 0; i < (guarded ? guards : 0); ++i) {
    float ang = prng.range(0, kTwoPi), dist = prng.range(1.5f, p->radius + 2.0f);
    if (i >= keep) continue;                                              // NOVO: depois dos sorteios, para o prng seguir igual
    ... (resto igual)
```
O `prng` consome exatamente os mesmos números que hoje; os baús do POI (sorteados depois do laço com o mesmo `prng`) ficam idênticos entre contratos.

**N3 — `game_mission.cpp`, `Game::updateChests` (L306):**
```cpp
// antes:
ammo_ = std::max(ammo_, magazineFor(weaponId()));
// depois:
if (ammoLimited()) grantReserve(config_.chestAmmoPct);
else ammo_ = std::max(ammo_, magazineFor(weaponId()));
```

**N4 — `game.cpp`, `Game::updateWeapon` (L854-871 e L933-941), três edições:**
```cpp
// (a) recarga manual (L860) — acrescentar "&& canReloadNow(magazine)":
if (in.reloads > 0 && ammo_ < magazine && reloadTimer_ <= 0 && meleeTime_ < 0 && !throwAiming_ && throwAnimTime_ < 0 && canReloadNow(magazine)) startReload();
// (b) fim da recarga (L867-870) — "ammo_ = magazine;" vira:
if (reloadTimer_ <= 0) {
    reloadTimer_ = 0;
    completeReload(magazine);
}
// (c) gatilho com pente vazio (L934-940) — a chamada incondicional a startReload() vira:
if (trigger && ammo_ == 0 && reloadTimer_ <= 0 && meleeTime_ < 0) {
    if (emptyClick_ <= 0) {
        emitSound(SND_EMPTY, player_.position, 0.8f);  // clique de arma vazia
        emptyClick_ = 0.35f;
    }
    if (canReloadNow(magazine)) startReload();
    else notifyOutOfAmmo();
}
```
Sem reserva e sem munição o jogador ainda tem **faca** (`meleeHit`, 55 x poder, recarga 0,55 s), granadas, molotovs e fumaça: nunca há travamento total.

**N5 — `game.cpp`, caixa de suprimentos por anúncio (`updatePickups`, L1277-1283):** completa o pente atual, mas **não** repõe a reserva nem enche os pentes das outras armas quando a reserva é limitada:
```cpp
ammo_ = magazineFor(weaponId());
if (!ammoLimited()) {
    for (int w = IT_RIFLE; w <= IT_LAUNCHER; ++w) clipsInit_[w] = false;
    clipsInit_[weaponId()] = true;
}
clips_[weaponId()] = ammo_;
reloadTimer_ = 0;
```
`revive()` (L199, `ammo_ = config_.magazine`) fica como está (documentado: reviver é uma decisão já existente de anúncio, ver cap. 7).

**N6 — `game.cpp`, `Game::updateMission`:** depois do bloco `if (survival()) {...}` (L405-408) chamar `updateContract(dt);`.

**N7 — `game.cpp`, `Game::finishResult` (L459-479):** acrescentar
```cpp
result_[R_ALERTS] = static_cast<float>(alerts_);
result_[R_RESERVE_PCT] = static_cast<float>(reservePct_);
```
(`R_CARGO`/`R_BOMBS` ficam 0 até 4C/4D; o `result_.fill(0)` de `startMission` já zera.)

**N8 — `game.cpp`, `Game::publishHud` (L2390-2636):** antes do `std::lock_guard` final: `if (mode_ == Mode::Mission) publishContractHud(h);`.

**N9 — `game.cpp`, `Game::startPvp` (L2124):** depois de `config_ = MissionConfig{};` acrescentar `reservePct_ = -1; alerts_ = 0;` (o PvP não pode herdar reserva da missão anterior; `ammoLimited()` também exige `Mode::Mission`, cinto e suspensório).

**N10 — `game_zombies.cpp`, percepção (L290-309) e os pontos de alerta:**
```cpp
z.perceptionTimer -= dt;
if (z.perceptionTimer <= 0) {
    z.perceptionTimer = 0.25f + static_cast<float>(index % 5) * 0.02f;
    // Noite vermelha: o escuro só atrapalha o jogador (os zumbis enxergam como de dia) e a atenção cresce com `aggression`.
    float sight = (theme_.sky.night > 0.5f && !config_.redSky) ? 14.0f : 21.0f;
    if (isRanged(z.kind)) sight += 6.0f;
    if (crouching_) sight *= 0.6f;  // agachado: furtivo
    sight *= config_.aggression;
    const float notice = (crouching_ ? 3.5f : 6.5f) * config_.aggression;
    z.canSee = dist < sight + 10.0f && clearLineOfSight(pos + Vec3(0, 1.5f * z.scale, 0), player_.position + Vec3(0, 1.5f, 0));
    if (!z.alerted && !dead_ && ((z.canSee && dist < sight) || dist < notice)) {
        alertZombie(z);                                   // era: z.alerted = true; z.lostSight = 0;
        emitSound(SND_GROAN, pos, 0.8f);
        z.groanTimer = rng_.range(4.0f, 8.0f);
        if (z.kind == ZombieKind::Screamer) z.abilityTimer = 0;
    }
    if (z.alerted) {
        if (z.canSee || dist < 12.0f) z.lostSight = 0;
        else z.lostSight += 0.25f;
        if (z.lostSight > 10.0f * config_.aggression && dist > 40.0f && z.nest < 0) z.alerted = false;
    }
}
```
Demais pontos (todos trocam a atribuição por `alertZombie`, mantendo as linhas `lostSight = 0` que já existiam quando o alerta pode ser redundante):
| Linha | Antes | Depois |
|---|---|---|
| L236 (`alertNear`) | `z.alerted = true; z.lostSight = 0;` | `alertZombie(z);` |
| L558 (grito) | `o.alerted = true; o.lostSight = 0;` | `alertZombie(o); o.lostSight = 0;` |
| L733 (molotov do jogador) | `z.alerted = true;` | `alertZombie(z);` |
| L880 (`damageZombie`) | `z.alerted = true; z.lostSight = 0;` | `alertZombie(z); z.lostSight = 0;` |
Não mexer: L177 (`spawnZombie` com `alerted=true`: ninhos/ondas/chamados) e `game_survival.cpp` L109 (teleporte de ondas).

**N11 — `game_items.cpp`, `addItem` (L286-289):** arma repetida deixa de virar munição grátis quando a reserva é limitada:
```cpp
if (isWeaponItem(id) && countItem(id) > 0) {
    if (!ammoLimited()) {
        clipsInit_[id] = false;
        if (weaponId() == id) ammo_ = magazineFor(id);
    }
    return 0;
}
```

**N12 — `game.cpp`, log periódico de FPS (L317):** sem mudança (o diagnóstico de contrato tem o seu próprio log em `updateContract`).

### 5.5 O que cada contrato muda de verdade no motor, e o custo

| Contrato | Decisão que o jogador passa a tomar | Mudanças Kotlin | Mudanças nativas | Linhas nativas ≈ | Risco |
|---|---|---|---|---|---|
| Noite vermelha | Avançar às cegas e confiar na bússola, ou esperar/usar fumaça; agachar para não ser notado a 27 m; extrair cedo | `EngineRules` + chip HUD + recompensa | `applyContractTheme` (N1), percepção (N10) | 60 | baixo (só tema e dois multiplicadores) |
| Recuperação silenciosa | Gastar ou poupar munição; faca e granadas no lugar do fuzil; contornar guardas para não somar alertas; abrir baús para repor | idem + `ContractRun.alerts` | reserva (N3, N4, N5, N11 + `game_contract.cpp`), alertas (N10), guardas (N2), HUD/resultado (N7, N8) | 160 | médio (toca o fluxo de tiro; calibrar) |
| Resistência | (nenhuma nova; só a mesma preparação/código) | plano `SURVIVAL` | nenhuma | 0 | baixo |
| Ninho instável (4C) | Aproximar-se de ninhos com granada/explosivo de longe, recuar, extrair cedo | módulo + `ExpeditionLog.modules` | `spawnZombie(forceKind)`, `updateNests`, cor do núcleo | 45 | baixo |
| Caixa perdida (4D) | Desvio de exploração antes de extrair; revelar a carga ao destruir o 1º ninho | objetivo CARGO | entidade `Cargo`, colocação, render, HUD/mapa | 220 | médio |

### 5.6 4C — Ninho instável (diferido; ponto de entrada já reservado)

- `game.h`: `struct Nest` ganha `int spawnCount = 0;`; `void spawnZombie(const Vec3& pos, bool alerted, int nest = -1, bool forceElite = false, int forceKind = -1);`.
- `game_zombies.cpp` `spawnZombie` (L170): `ZombieKind kind = forceKind >= 0 ? static_cast<ZombieKind>(forceKind) : rollKind();`.
- `game_mission.cpp` `updateNests` (L225-235), no ramo que gera zumbi:
```cpp
const bool bomb = config_.nestBomb > 0.0f && rng_.chance(config_.nestBomb);
spawnZombie(pos, true, static_cast<int>(i), false, bomb ? static_cast<int>(ZombieKind::Exploder) : -1);
if (bomb) { ++bombs_; emit(EV_CONTRACT, 5, static_cast<int>(i)); }
```
  (O Inchado já existe com IA de pavio e explosão de 4,6 m; `minStage` é ignorado quando forçado. Respeita `kMaxNestMobs = 7`.)
- `game.cpp` `buildScene` (L1895): cor do núcleo `config_.nestBomb > 0 ? Vec4(1.0f, 0.45f, 0.1f, 1) : Vec4(0.62f, 0.16f, 0.78f, 1)` (laranja instável comunica o contrato).
- Resultado: `R_BOMBS`; módulo concedido por `Contracts.reward` quando `run.nestsDestroyed >= run.nestsTotal` e vitória; `ExpeditionLog.modules["nucleo_instavel"]++`; a etapa 6 consome o módulo.
- Ao ligar: `engineReady = true` em `Contracts.all`, preencher `Contracts.card` (texto já descrito), teste novo `unstableNestAwardsModuleOnlyWhenAllNestsFall`.

### 5.7 4D — Caixa perdida (diferido)

- Colocação **determinística** (mesma semente → mesma caixa): em `startContractState` (4D), com `Rng crng(config_.seed ^ 0xCA46E5u)`, varrer `island_->poiAtCell(id)` (48x48 células) e escolher entre os POIs `ROLE_NONE` com `stamp(p).decor.chests` não vazio, a 450-1400 m da largada e a mais de 250 m de ninhos/portal. Usar a posição de um baú já validado pela decoração do POI (garante que está dentro do mundo e acessível). `IslandPoi.center.y` já é o chão nivelado, então não depende de o chunk estar carregado.
- Estado: `struct Cargo { Vec3 pos; bool placed=false, revealed=false, collected=false; }`.
- Coleta em `updateContract`: `dist < 1.8` → `collected`, `emit(EV_CONTRACT, 3)`, partículas/som de baú.
- Revelação: ao destruir o primeiro ninho (`damageNest` → `EV_NEST_DESTROYED`) a carga fica `revealed` e aparece como marcador dourado "CARGA" na bússola e no `MapOverlay` (`H_CT_CARGO_*`); antes disso só a dica de distância no briefing.
- Resultado: `R_CARGO`; `Contracts.reward` paga `objectiveMax` se coletada **antes** de a vitória acontecer.
- Render: caixa laranja + feixe vertical (padrão dos pickups); sem entidade de colisão.

### 5.8 Determinismo, desempenho, compatibilidade

- Mesma `(fase, semente)` => mesma ilha, mesmos POIs, mesmos guardas (posições), mesmos baús do POI, qualquer que seja o contrato (N2 preserva o `prng`). O comportamento instantâneo (quem alerta quando, loot de zumbis) usa `rng_` e depende do jogador: **o código reproduz ilha e regras, não a partida quadro a quadro** (a spec pede exatamente isso: "jogar a mesma ilha sob as mesmas regras").
- Custo por quadro: contador `alerts_` e `updateContract` são O(1); `publishContractHud` grava 4 floats. `H_SIZE` 660→720 acrescenta 240 bytes por cópia de HUD.
- ABIs: tudo em C++17 puro, sem novas dependências; compilar `arm64-v8a` (e conferir `armeabi-v7a` uma vez no fim, a spec exige as duas).

## 6. Fluxo do app, telas e MainActivity

### 6.1 Fluxo

```
Home JOGAR ─────────┐
Fases JOGAR ────────┼─> showExpeditionPrep(CAMPAIGN, fase) ─PARTIR─> startExpedition(plano) ─[intersticial no 5º toque]─> launchMission
Prep: aba SOBREVIV. ┘                 │ (antes da 1ª vitória: parte direto, sem tela)
                                      └─ MOCHILA ─> showInventory(0){ volta à preparação com o mesmo plano }
Derrota "TENTAR DE NOVO" ─> startExpedition(plan.retry(), contar=false)         (mesma ilha só se veio de código)
Vitória "PRÓXIMA FASE"   ─> showExpeditionPrep(CAMPAIGN, fase+1)               (preseleciona o último contrato)
Sobrevivência: tela de ranking (SurvivalScreen) ─SOBREVIVER─> showExpeditionPrep(SURVIVAL)
```

### 6.2 `MainActivity.kt` — trechos exatos (arquivo quente)

Os números são da árvore de trabalho de 18/10. Importações novas: `ContractRun`, `Contracts`, `ExpeditionKind`, `ExpeditionPlan`, `ExpeditionRules` (de `data`), `applyContract` (de `engine`), `ExpeditionPrepScreen`, `ExpeditionResultOverlay` (de `ui`); saem `SurvivalResultOverlay` só se for trocado (ver 6.5).

**M1 — campos (junto de `survivalWave`, L101):**
```kotlin
/** Expedição escolhida/em curso: tipo, fase, ilha (semente) e contrato. */
var plan: ExpeditionPlan = ExpeditionPlan.standard(1)
    private set
```

**M2 — abrir a preparação (junto de `showStages`/`showSurvival`, L381-408):**
```kotlin
fun showExpeditionPrep(
    kind: ExpeditionKind = ExpeditionKind.CAMPAIGN,
    stage: Int = progress.highestUnlockedStage(),
    initial: ExpeditionPlan? = null,
    back: () -> Unit = { showHome() },
) {
    if (kind == ExpeditionKind.CAMPAIGN && initial == null && !Contracts.prepUnlocked(progress)) {
        startExpedition(ExpeditionPlan.standard(stage))      // novo jogador: parte direto
        return
    }
    closeOverlay()
    hud.visibility = View.GONE
    audio.setMusic(GameAudio.Music.MENU)
    val look = progress.lookCode
    engine { NativeBridge.enterLobby(look, 0) }
    showScreen(ExpeditionPrepScreen(this, kind, stage, initial, back))
}
```

**M3 — lançamento (substitui `startStage`/`startCampaignPlay`/`beginStage`, L465-496):**
```kotlin
/** PARTIR: fixa o plano e lança a missão. [countPlayTap] só vale na campanha (intersticial no 5º toque). */
fun startExpedition(chosen: ExpeditionPlan, countPlayTap: Boolean = true) {
    if (stageLaunchPending) return
    plan = chosen
    progress.expeditions.remember(chosen)
    survivalMode = chosen.kind == ExpeditionKind.SURVIVAL
    survivalWave = 0
    missionStage = chosen.stage
    launchWithAdGate(countPlayTap && !survivalMode)
}

private fun launchWithAdGate(countPlayTap: Boolean) {
    val showPlayAd = countPlayTap && Rules.registerCampaignPlayTap(progress)
    save()
    val launch = {
        stageLaunchPending = false
        launchMission()
    }
    // ... (o resto é exatamente o corpo atual de beginStage, a partir de "if (!showPlayAd || ...")
}
```
Chamadores a trocar: `HomeScreen.kt` L270 `app.startCampaignPlay(current.number)` -> `app.showExpeditionPrep(ExpeditionKind.CAMPAIGN, current.number)`; `StagesScreen.kt` L74 `app.startCampaignPlay(selected)` -> `app.showExpeditionPrep(ExpeditionKind.CAMPAIGN, selected, back = { app.showStages() })`; `SurvivalScreen.kt` L86 `app.startSurvival()` -> `app.showExpeditionPrep(ExpeditionKind.SURVIVAL, back = { app.showSurvival() })`. `startStage`, `startCampaignPlay` e `startSurvival` deixam de existir (o compilador acusa todos os usos).

**M4 — `launchMission` (L498-565):**
```kotlin
val stage = Campaign.stage(missionStage)
val p = progress
val rules = Contracts.engineRules(plan.contract, stage.number)
val baseDiff = if (survivalMode) Campaign.survivalDifficulty(p) else Campaign.difficulty(/* ... exatamente como hoje ... */)
val diff = rules.applyTo(baseDiff)                       // NOVO (substitui o uso direto de baseDiff)
...
config[Cfg.SEED] = plan.seed.toFloat()                   // era: Random.nextInt(1, 16_000_000).toFloat()
...
config[Cfg.EXPLOSIVES] = p.upgrade("explosives").toFloat()
config.applyContract(plan, rules)                        // NOVO, logo depois de EXPLOSIVES
```
Rótulo da tela de carregamento (L553): usar o construtor de 5 argumentos para anexar o contrato:
```kotlin
val contractTag = if (plan.contract == ContractId.NONE) "" else " • " + Contracts.get(plan.contract).name.uppercase()
loading = (if (survivalMode)
    LoadingOverlay(this, "SOBREVIVÊNCIA • ONDAS INFINITAS$contractTag", "Ilha aleatória", Palette.gold, stage.number)
else
    LoadingOverlay(this, "ZONA ${stage.zone.index + 1} • ${stage.zone.name.uppercase()}$contractTag", "Fase ${stage.number} · ${stage.title}", stage.zone.color, stage.number)
).also { showOverlay(it) }
```

**M5 — `onMissionReady` (L617-639):** depois do `when`, `Contracts.briefing(plan)?.let { hud.tip(it) }` (substitui a dica padrão da campanha só quando há contrato).

**M6 — `onGameEvent` (L641-763), novo ramo antes de `Ev.PORTAL_OPEN`:**
```kotlin
Ev.CONTRACT -> when (a) {
    1 -> hud.message("SEM MUNIÇÃO! USE A FACA E AS GRANADAS", Palette.red)
    2 -> hud.message("ÚLTIMO PENTE!", Palette.gold)
    // 3, 4 (carga) e 5 (Inchado de ninho) entram em 4C/4D
}
```
No ramo `Ev.CHEST` (L662), quando `hud` indicar reserva limitada, acrescentar " +½ PENTE" é opcional (o chip já mostra a reserva).

**M7 — leitura do resultado do contrato (junto de `readResult`, L766):**
```kotlin
private fun readContractRun(): ContractRun {
    val r = FloatArray(Res.SIZE)
    NativeBridge.copyResult(r)
    return ContractRun(
        alerts = r[Res.ALERTS].toInt(),
        cargoCollected = r[Res.CARGO] > 0.5f,
        nestsDestroyed = r[Res.NESTS].toInt(),
        nestsTotal = Campaign.stage(missionStage).crystalTotal,
    )
}
```

**M8 — `finishMission` (L794-809):** trocar `Rules.applyResult(progress, stage, result)` por
```kotlin
val summary = ExpeditionRules.applyCampaign(progress, plan, stage, result, readContractRun(), today())
...
showOverlay(ExpeditionResultOverlay(this, plan, stage, result, summary, unlocked))
```
**M9 — `recordDefeat` (L837-846):** `Rules.applyResult(...)` -> `ExpeditionRules.applyCampaign(progress, plan, Campaign.stage(missionStage), result, readContractRun(), today())` (derrota: contrato paga zero; o histórico registra DEFEAT).

**M10 — `showDefeat` (L811-818):** passar ao `DefeatOverlay` o plano e uma linha de resumo: `DefeatOverlay(this, stage, plan, summaryLine = "Ninhos ${n}/3 • ☠ ${kills} • ${mm:ss}", canRevive, canSkip)` com os valores de `readResult(false)`.

**M11 — retry e saídas (L849-881, L1002-1005):**
```kotlin
// abandonMission(retry):
if (survivalMode) { recordSurvival(); endMissionUi(); maybeInterstitial { if (retry) startExpedition(plan.retry(), countPlayTap = false) else showSurvival() }; return }
recordDefeat(); endMissionUi()
maybeInterstitial { if (retry) startExpedition(plan.retry(), countPlayTap = false) else showStages() }

// skipStageWithAd: onde hoje chama startStage(missionStage + 1):
showExpeditionPrep(ExpeditionKind.CAMPAIGN, missionStage + 1, back = { showStages() })

// leaveAfterVictory(next):
if (next && progress.isStageCompleted(missionStage)) showExpeditionPrep(ExpeditionKind.CAMPAIGN, missionStage + 1, back = { showStages() }) else showStages()

// leaveSurvival(again):
maybeInterstitial { if (again) startExpedition(plan.retry(), countPlayTap = false) else showSurvival() }
```

**M12 — sobrevivência (L942-1000):** remover `startSurvival()`; `recordSurvival()` usa `ExpeditionRules.applySurvival(progress, plan, name, today(), result, ContractRun(alerts = 0))` e devolve `SurvivalExpeditionSummary`; `finishSurvival()` passa `plan` e o resumo a `SurvivalResultOverlay` (6.5).

**M13 — `pauseSubtitle` (L1007):** acrescentar `· ${Contracts.get(plan.contract).name}` quando `plan.contract != NONE`.

**M14 — `onBack` (L255-267):** novo ramo antes de `currentScreen !is HomeScreen -> showHome()`: `currentScreen is ExpeditionPrepScreen -> (currentScreen as ExpeditionPrepScreen).goBack()`.

**M15 — `doubleRewardWithAd(amount, onDone)` (L892):** assinatura inalterada; os chamadores passam `summary.doublable` (base), nunca `summary.total`.

### 6.3 `ui/ExpeditionPrepScreen.kt` (nova)

Construtor: `class ExpeditionPrepScreen(app: MainActivity, kind: ExpeditionKind, stage: Int, initial: ExpeditionPlan?, private val back: () -> Unit) : FrameLayout(app)`; `fun goBack() = back()`. Estilo e helpers: `Ui.panel/row/column/text/title/button`, `Header.build`, `PanelDrawable`, `BlockDrawable`, `ItemIconView` (já usado em `InventoryScreen`), mesma moldura de `StagesScreen`/`SurvivalScreen` (fundo `Color.argb(170, 6, 10, 20)`, `Ui.blockTouches(this)`).

Layout (paisagem; rola em telas baixas):

```
Header: "PREPARAR EXPEDIÇÃO" / "ESCOLHA O CONTRATO • ASSUMA O RISCO"            [X] volta
┌ abas ──────────────────────────────────────────────────────────────────────────────┐
│ [ CAMPANHA • FASE 7 ]  [ SOBREVIVÊNCIA ]                         RANKING (Survival) │
├ esquerda (peso 1) ───────────────────┬ direita (peso 1,3, ScrollView) ──────────────┤
│ Cartão da missão:                    │ CONTRATO ESCOLHIDO: nome colorido + frase     │
│  ZONA 2 • DESERTO  (cor da zona)     │ CUSTO / RISCO       (linhas com ▸ vermelho)   │
│  Fase 7 · Oásis Infectado            │ RECOMPENSA          (linhas com ◆ ciano)      │
│  ILHA 5,1 km • 3 NINHOS • PORTAL     │ OBJETIVO OPCIONAL   (★ dourado, se houver)    │
│  Horda ~N • vida deles M             │ EQUIPAMENTO: 6 ItemIconView da barra rápida   │
│ Lista de contratos (cartões):        │   + nome da arma + dica de loadoutAdvice      │
│  ● Expedição padrão        ●○○       │   [MOCHILA]                                    │
│  ● Recuperação silenciosa  ●●○ ◆…    │ ILHA: ZZ-80W0-Y4G0-100RS                       │
│  ○ Noite vermelha  (bloqueado F3)    │   [COPIAR] [NOVA ILHA] [CÓDIGO…]              │
│                                      │ Histórico deste contrato: "Vencido 3x • último │
│                                      │   ◆ +52 em 12/10"                              │
├──────────────────────────────────────┴───────────────────────────────────────────────┤
│ [ PARTIR ▶ ]  (botão grande, Ui.pulse)   "Sem anúncios: contrato grátis, bônus não dobra" │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

Estado e comportamento:
- `plan` local (`var`). Ao abrir: se `initial != null` usa-o; senão campanha = `ExpeditionPlan.standard(stage).copy(contract = Contracts.defaultContract(CAMPAIGN, p))`; sobrevivência = `ExpeditionPlan.survival(Contracts.defaultContract(SURVIVAL, p))`.
- Trocar de aba recria o plano do outro tipo (a semente da aba oculta é sorteada na hora; o bioma da sobrevivência mostra o nome do bioma sorteado).
- Selecionar contrato bloqueado: toque mostra `lockReason` (`Popup`/mensagem) e não seleciona. Selecionar um contrato: `plan = plan.copy(contract = id)` (semente e fase **não mudam**), atualiza detalhes, código e botão.
- `NOVA ILHA`: `plan = plan.copy(seed = Contracts.randomSeed(), sharedSeed = false)` (e na sobrevivência também re-sorteia o bioma).
- `COPIAR`: `ClipboardManager.setPrimaryClip(ClipData.newPlainText("Zombie Zone", plan.code))` + `Toast` "Código copiado". Código `null` (fase > 1023): bloco do código oculto.
- `CÓDIGO…`: abre `CodeEntryOverlay` (a mesma classe no arquivo): `EditText` monoespaçado (`inputType = TYPE_CLASS_TEXT or TYPE_TEXT_FLAG_CAP_CHARACTERS or TYPE_TEXT_FLAG_NO_SUGGESTIONS`, `imeOptions = IME_FLAG_NO_EXTRACT_UI or IME_ACTION_DONE` para não ocupar a tela inteira em paisagem), botões `COLAR` (lê `primaryClip`, passa por `ExpeditionCode.extract`), `CANCELAR`, `USAR`. `USAR` => `ExpeditionCode.decode` -> erro: `ExpeditionCode.message` sob o campo; ok: `Contracts.check(plan, p)` -> se `!ok` mostra `message`; se ok: troca de aba se necessário e `plan = decoded` (com `sharedSeed = true`). Se o IME atrapalhar o modo imersivo, `COLAR` sozinho já resolve (fallback documentado).
- `MOCHILA`: `app.showInventory(0) { app.showExpeditionPrep(plan.kind, plan.stage, plan, back) }` (volta com o mesmo plano).
- `PARTIR`: `app.startExpedition(plan)`; nenhuma chamada de anúncio nesta tela.
- Número de "horda" e "vida" do cartão: mesmos cálculos de `StagesScreen.select()` (L97-98) aplicando `Contracts.engineRules(...).applyTo(diff)` para o contrato selecionado (o jogador vê a dificuldade real).

### 6.4 `ui/ExpeditionResultOverlay.kt` (nova) e `DefeatOverlay`

`ExpeditionResultOverlay(app, plan, stage, result: MissionResult, summary: ExpeditionSummary, unlocked: List<Cosmetic>)` — painel de ~680 dp (mesma moldura de `VictoryOverlay`, L211-291, que passa a ser código morto: removê-la no mesmo commit). Conteúdo:

1. **Cabeçalho:** "EXPEDIÇÃO CONCLUÍDA" + título "VITÓRIA!" (ou "EXTRAÍDO" quando `contract != NONE`), à direita o **chip do contrato** (cor + nome) e o código `ZZ-…` com botão `COPIAR`.
2. **Objetivos (lista com ✓/✗)**, nesta ordem: `Ninhos destruídos n/3`, `Cristais n/3`, `Extraído pelo portal`; as 3 estrelas (rótulos atuais de `VictoryOverlay`); **linha do contrato**: Silenciosa => `Silêncio: ${alerts} zumbis alertados => ◆ +${objective}`; Noite vermelha => `Noite vermelha concluída`; 4C/4D => módulo/carga.
3. **Recompensa em duas colunas:** `EXPEDIÇÃO` (as seis linhas atuais: primeira vitória, estrelas, cristais, abates, baús, exploração, só quando > 0) e `CONTRATO` (`Ao extrair`, `+${runPct}% do saque`, `Objetivo`, `Módulo` se houver); `TOTAL ◆ x` em destaque.
4. **Resumo** (a linha atual de tempo/abates/precisão/distância) e `NOVO VISUAL` quando houver.
5. **Botões:** `DOBRAR ◆ ${summary.doublable}` (anúncio; texto auxiliar pequeno *"o bônus do contrato não é dobrado"*), `MAPA`, `PRÓXIMA FASE ▶`. O callback usa `app.doubleRewardWithAd(summary.doublable)` e atualiza só o texto do TOTAL somando `doublable`.

`DefeatOverlay(app, stage, plan, summaryLine, canRevive, canSkip)` (assinatura nova; `Overlays.kt` L293): acrescentar o chip do contrato, `summaryLine` e, quando `plan.contract != NONE`, *"O contrato só paga na extração."*. Nenhum botão novo.

### 6.5 Sobrevivência como variante de expedição (mudança mínima)

- **Entrada:** `SurvivalScreen` mantém ranking, recordes e MOCHILA; o botão `SOBREVIVER ▶` chama `app.showExpeditionPrep(SURVIVAL, back = { app.showSurvival() })`. Na preparação, a aba SOBREVIVÊNCIA mostra os contratos `Resistência` (padrão) e `Noite vermelha`; o cartão da missão mostra o bioma sorteado e as regras da onda (texto atual de `SurvivalScreen`). Isso também devolve à sobrevivência um acesso a partir da Home (hoje inexistente).
- **Resultado:** `SurvivalResultOverlay` ganha dois parâmetros opcionais `plan: ExpeditionPlan? = null, contract: ContractReward = ContractReward.NONE`; quando há contrato mostra o chip, a linha `CONTRATO ◆ +x` e o código. `DOBRAR` continua usando `summary.reward` (já é só a base). `SurvivalDefeatOverlay` não muda.
- **Ranking:** inalterado (`SurvivalRecord`); o contrato é registrado no histórico de expedições, não no ranking, para não misturar condições de jogo.
- **Não muda:** ondas, pontuação, dificuldade (`Campaign.survivalDifficulty`), anúncio de reviver.

### 6.6 Chip do contrato no HUD (`HudView.kt`)

Um único método novo, chamado uma vez em `onDraw` logo depois de `drawObjective`/`drawSurvivalTop` (L492), **desenhado só quando `hud[Hud.CT_ID] > 0.5f && island`**:

```kotlin
private fun drawContractChip(canvas: Canvas, w: Float) {
    val id = ContractId.fromCode(hud[Hud.CT_ID].toInt()) ?: return
    if (id == ContractId.NONE || !island) return
    val def = Contracts.get(id)
    val parts = mutableListOf(def.name.uppercase())
    if (id == ContractId.SILENT_RECOVERY) parts.add("ALERTAS ${hud[Hud.CT_ALERTS].toInt()}")
    val clips = hud[Hud.CT_RESERVE_CLIPS]
    if (clips >= 0f) parts.add("RESERVA ${hud[Hud.CT_RESERVE_ROUNDS].toInt()}")
    // desenhar uma pílula (p/rect/textPaint) com def.color, centralizada em x = w/2
}
```
Posição: **abaixo da faixa superior e fora da região de mira**; a etapa 1 (HUD) está redesenhando essa faixa, então a coordenada vertical final deve seguir o layout dela (sugestão: logo abaixo da bússola). Reserva baixa (`<= 100` centésimos, ou seja `CT_RESERVE_CLIPS <= 1f`): pílula em `Palette.gold`; zerada: `Palette.red`. A reserva de balas também pode aparecer ao lado do número de munição do botão RECARREGAR (`drawReload`, L1526): `"$ammo/${reserve}"` quando `CT_RESERVE_ROUNDS >= 0`.

### 6.7 Textos auxiliares

README (a área de documentação reescreve; texto sugerido): *"Contratos: antes de cada expedição você escolhe um contrato, que muda as regras em troca de recompensa maior. Contratos nunca custam diamantes nem anúncios, e o bônus do contrato não é dobrado por vídeo. Cada ilha tem um código (ZZ-XXXX-XXXX-XXXXX): quem digitar o mesmo código joga a mesma ilha sob as mesmas regras."*

## 7. Recompensa e anúncios (contrato nunca é monetizado)

### 7.1 Onde há anúncio no fluxo e o que muda

| Ponto do fluxo | Hoje | Depois desta etapa |
|---|---|---|
| Escolher, liberar ou iniciar um contrato | n/a | **Sem anúncio, sem custo em diamantes.** A tela de preparação não tem botão de vídeo. |
| Intersticial no 5º toque em JOGAR (`Rules.registerCampaignPlayTap`) | em `startCampaignPlay` | contado em **PARTIR** na campanha (`startExpedition(countPlayTap=true)`); retries e sobrevivência não contam, como hoje |
| DOBRAR recompensa da vitória | dobra `summary.total` | dobra **só a parte base** (`summary.doublable`); o bônus do contrato nunca |
| DOBRAR na sobrevivência | dobra `summary.reward` | igual (já é a base) |
| Reviver com anúncio (1x) | `reviveWithAd` | igual; o contrato continua pagando se a extração acontecer (ver decisão D1) |
| Pular fase com anúncio (após 2 derrotas) | `skipStageWithAd` | igual; depois abre a **preparação** da fase seguinte |
| SUPRIMENTOS por anúncio | enche pente, cura e dá itens | com reserva limitada: enche **só o pente atual**; não repõe a reserva nem os pentes das outras armas (N5) |
| Intersticial pós-partida (`maybeInterstitial`) | igual | igual |

A dificuldade **nunca** depende de receita: a preparação e o motor só leem fase, contrato e melhorias do jogador. (O código de dificuldade adaptativa por receita vem da limpeza de outra área; esta etapa não o toca nem o reintroduz.)

### 7.2 Fórmulas e ordem de grandeza (para calibrar)

`contrato = extração + runPct% x (abates + baús + exploração) + objetivo`, somente em vitória. Exemplos com os valores iniciais (saque típico sem contrato = 40 a 80 ◆ por saída repetida):

| Situação | Extração | % do saque (saque 70) | Objetivo | Bônus total |
|---|---|---|---|---|
| Noite vermelha, fase 10 | 25+2x10 = **45** | 50% = **35** | — | **◆ 80** |
| Noite vermelha, fase 30 | 25+2x30 = **85** | 50% = 35 | — | ◆ 120 |
| Silenciosa, fase 10, 8 alertas | 20+10 = **30** | 25% = 17 | 60-3x8 = **36** | **◆ 83** |
| Silenciosa, fase 10, 0 alertas | 30 | 17 | 60 | ◆ 107 |
| Silenciosa, fase 10, 20+ alertas | 30 | 17 | 0 | ◆ 47 |
| Sobrevivência + Noite vermelha (recompensa de ondas 120) | 0 | 50% = 60 | — | ◆ 60 |

Salvaguardas contra inflação: tetos de extração (`80`/`125`), uso de `min(fase, 60)`, nada de primeira vitória/estrelas/cristais no contrato (essas já são pagas pelo progresso), contrato só paga na extração. Se, ao calibrar, o contrato virar o único jeito racional de jogar, reduza `extractionBase` antes de mexer no motor.

### 7.3 Decisão que só o dono do produto pode tomar (D1)

*Política de anúncios em expedição com contrato.* Padrão implementado: **(a)** DOBRAR só dobra a base; **(b)** reviver e pular fase por anúncio continuam como estão e **não** anulam o bônus de contrato; **(c)** o intersticial do 5º toque vale também quando o toque inicia um contrato; **(d)** o SUPRIMENTO por anúncio não repõe a reserva. Se o dono preferir "contrato sem nenhum anúncio ao redor" (bloquear (b) e (c)), são duas linhas em `startExpedition` e em `DefeatOverlay.canRevive`.

## 8. Persistência

- `Progress.kt` (dentro de `class Progress`, perto de `val stats = Stats()`): `val expeditions = ExpeditionLog()`.
- `SaveStore.kt`:
  - `encode(p)`, junto de `put("tutorialSeen", p.tutorialSeen)`: `put("expeditions", p.expeditions.encode())`
  - `decode(o)`, junto de `p.tutorialSeen = o.optBoolean("tutorialSeen")`: `p.expeditions.load(o.optString("expeditions", ""))`
- **Sem bump de `version`** e sem migração: campo ausente = log vazio com `lastCampaign=NONE`, `lastSurvival=ENDURANCE`. Se a área de "save recuperável" tiver introduzido versão/backup, apenas acrescente essas duas linhas no mesmo lugar; `ExpeditionLog.load` nunca lança, então não pode causar o "JSON inválido derruba o save".
- Persistido: último contrato por modo, contadores `plays`/`wins` por id, módulos (4C), 40 registros mais recentes. **Não persistido:** plano em curso (vive em `MainActivity.plan`), aba e seleção da preparação, semente da tela.
- Todo acesso ao log ocorre na thread de UI (`MainActivity`), como `Progress`.

## 9. Passos de implementação (cada um deixa o APK jogável)

Convenção de build (memória do projeto): um Gradle por vez, sempre `--no-daemon --max-workers=1`, ABI única na iteração, **sem emulador**; instalar no aparelho por `adb`.

| # | Passo | Arquivos | Verificação | Por que o APK continua jogável |
|---|---|---|---|---|
| 0 | Linha de base | — | `.\gradlew.bat --no-daemon --max-workers=1 :app:testDebugUnitTest` verde antes de começar | — |
| 1 | **Modelo Kotlin puro + testes** | `data/Contracts.kt`, `ExpeditionCode.kt`, `ExpeditionLog.kt`, `ExpeditionRules.kt`; 1 linha em `Progress.kt`; 2 em `SaveStore.kt`; testes de 10.1-10.3 | `testDebugUnitTest` | Nada é chamado ainda |
| 2 | **Protocolo e motor neutro** | `protocol.h`, `NativeProtocol.kt`, `jni_bridge.cpp`, `game.h`, `contract_rules.h`, `game_contract.cpp`, `CMakeLists.txt`; N1-N11 com contrato neutro; `engine/ContractConfig.kt` | `ContractProtocolParityTest`; `assembleDebug` arm64; jogar uma fase **igual a antes** (reserva ilimitada, névoa original) | Todos os valores novos são neutros por padrão |
| 3 | **Fluxo e telas** (só `NONE` e `ENDURANCE` selecionáveis) | `ui/ExpeditionPrepScreen.kt`, `ui/ExpeditionResultOverlay.kt`, M1-M15, call-sites (Home/Fases/Sobrevivência), `DefeatOverlay`, `SurvivalResultOverlay` | Jogar campanha e sobrevivência pela preparação; copiar/colar código D1 (aceita o código, mas a lista só mostra contratos disponíveis; D1 deve dizer "indisponível" até o passo 5); histórico no save | Preparação + resultado novos, regras de jogo idênticas |
| 4 | **Noite vermelha** (`engineReady = true`) | `Contracts.all`, `applyContractTheme` já pronto, N10, chip HUD, `Contracts.briefing` | Cenário D2 e D4 no aparelho | Só uma opção a mais |
| 5 | **Recuperação silenciosa** (`engineReady = true`) | N2-N5, N7-N8, N11, chip com reserva e alertas, `Ev.CONTRACT` | Cenário D1; `contract_rules_test` (host, opcional) | Idem |
| 6 | **Fechamento** | textos finais, README (seção 6.7), calibração da seção 7.2, versão/changelog, build `armeabi-v7a` uma vez | Checklist do capítulo 11 | — |
| 7 | **(diferido)** 4C e 4D | seções 5.6 e 5.7 | testes novos listados lá | Contratos novos entram escondidos até `engineReady = true` |

Observação de ordem: se o prazo apertar, o passo 5 pode ser movido para depois do 6 sem quebrar nada (o contrato `SILENT_RECOVERY` fica `engineReady = false` e o código dele aparece como "indisponível nesta versão").

## 10. Testes

### 10.1 `ExpeditionCodeTest` (Kotlin, `app/src/test/java/com/fabiosilva/ecosdeeldra/`)
- `goldenVectorsAreStable` — os seis primeiros vetores do cap. 4.3 (`ExpeditionPlan(...).code == "ZZ-..."`) e o decode inverso devolve o mesmo plano com `sharedSeed = true`.
- `roundTripKeepsEveryField` — laço sobre `ExpeditionKind` x todos os `ContractId` compatíveis x fases `{1, 2, 7, 20, 137, 1023}` x sementes `{1, 4242, 123456, 15_999_999, 16_777_215}`.
- `singleCharacterSubstitutionIsAlwaysRejected` — para três códigos, trocar cada uma das 13 posições por cada um dos outros 31 símbolos: `decode` nunca devolve `Ok`.
- `normalizationAcceptsLowercaseSeparatorsAndConfusables` — `zz-80w0-y4g0-100rs`, com espaços, com quebra de linha, sem prefixo `ZZ-`, com `O/I/L` no lugar de `0/1/1`; e `extract("Joga essa ilha: ZZ-80W0-Y4G0-100RS!")`.
- `lengthAlphabetAndChecksumHaveReadableProblems` — vazio -> `EMPTY`; 12 símbolos -> `LENGTH`; `U` -> `ALPHABET`; último símbolo trocado -> `CHECKSUM`; `message(...)` nunca é vazio.
- `invalidFieldsAreRejectedAfterTheChecksum` — montar códigos válidos em CRC com: versão 2, `mods != 0`, fase 0, semente 0, contrato 6/7/255, sobrevivência com contrato `SILENT_RECOVERY`, campanha com `ENDURANCE`. Para isso o teste expõe um helper `encodeRaw(version, kind, stage, seed, contract, mods)` **somente de teste** (copiar a rotina `crc12`).
- `encodeRefusesWhatCannotFit` — fase 1024 e semente 0 devolvem `null`.
- `seedSurvivesTheFloatRoundTrip` — para sementes até 16_777_215: `seed.toFloat().toInt() == seed` (garante que o `Cfg.SEED` não corrompe).

### 10.2 `ContractsTest`
- `catalogCoversEveryIdAndKindsAreConsistent` — todo `ContractId` tem `ContractDef`; `NONE` só campanha; `ENDURANCE` só sobrevivência; `RED_NIGHT` ambos.
- `cardNumbersComeFromTheSameTuningTheEngineReceives` — para `SILENT_RECOVERY`, `RED_NIGHT` (fases 2 e 40): cada número do `engineRules` (`55%`, `50%`, `3,2`/`7,0`, `45%`, `35%`, `10%`/`15%`) aparece nas strings de `card(...)`.
- `neutralContractsLeaveTheDifficultyUntouched` — `NONE`/`ENDURANCE`: `applyTo(d) == d`, `EngineRules()` neutro.
- `redNightHardensZombiesButKeepsPopulation` — velocidade x1,10, dano x1,15, população igual (a noite do motor já soma +3).
- `silentRecoveryScalesAmmoWithStageAndCapsIt` — `ammoClips(2) == 3.2f`, `ammoClips(40) == 7f`, `ammoClips(90) == 7f`; `zombieCount` reduz mas nunca abaixo de 3.
- `rewardOnlyOnVictory` — derrota => `ContractReward.NONE` em todos os contratos.
- `silenceBonusDropsThreePerAlertAndClamps` — alertas 0/8/20/40 => 60/36/0/0.
- `survivalRewardHasNoExtractionAndScalesWithRunBase` — `RED_NIGHT` na sobrevivência: `extraction == 0`, `runBonus == base*50/100`.
- `survivalCardMatchesRules` — roda `Rules.applySurvivalResult` com onda 3 e 10 abates e compara com a fórmula citada no texto da `Resistência` (8 por onda vencida, 1 a cada 2 abates, +30 no recorde >= 60 s).
- `doublingNeverIncludesTheContractBonus` — `ExpeditionSummary.doublable == base.total` e `SurvivalExpeditionSummary.doublable == base.reward`.
- `checkRejectsLockedStageEngineNotReadyAndWrongKind` — jogador novo + código de fase 60 => `!ok` e mensagem cita a fase 59; `UNSTABLE_NEST`/`LOST_CRATE` => "ainda não disponível"; contrato bloqueado pela progressão; caso feliz `ok`.
- `unlocksFollowProgress` — `prepUnlocked` falso para `Progress()` novo, verdadeiro após a primeira vitória; `options()` esconde `engineReady=false` e marca `locked`.
- `retryKeepsTheIslandOnlyForSharedCodes` — `plan.retry(Random(1))`: sem `sharedSeed` muda a semente (dentro de 1..15_999_999); com `sharedSeed` devolve o mesmo plano.
- `loadoutAdviceUsesTheInventory` — os três avisos de 3.4.

### 10.3 `ExpeditionLogTest` e `ExpeditionRulesTest`
- `ExpeditionLogTest.encodeDecodeRoundTrips` (registros, contadores, módulos, últimos contratos), `garbageAndUnknownFieldsAreIgnored` (texto vazio, `v9|...`, registro com contrato 99, números inválidos, `~` faltando) sem exceção, `keepsOnlyTheLatestFortyRecords`, `countsPlaysAndWinsPerContract`, `rememberStoresLastContractPerKind`.
- `ExpeditionRulesTest.campaignVictoryCreditsBaseAndContract` — `Progress` novo, fase 2, `win(...)` como em `RulesTest`; `p.diamonds == base.total + contract.total`; registro `VICTORY` no log.
- `campaignDefeatKeepsBaseRewardsAndPaysNoContract` — `MissionResult(false, ...)`: `contract.total == 0`, `p.diamonds == base.total`, registro `DEFEAT`, `p.stages[n].fails` incrementado (efeito de `applyResult` preservado).
- `survivalContractAddsPercentOfWaveReward`, `stageProgressionIsUnaffectedByTheContract` (estrelas/primeira vitória idênticas com e sem contrato).
- Nenhum teste existente muda: `RulesTest`/`InventoryTest` continuam passando (`MissionResult` e `Rules.*` intactos).

### 10.4 `ContractProtocolParityTest` (C++ <-> Kotlin) e configuração
- Lê `src/main/cpp/game/protocol.h` (diretório de trabalho do teste = módulo `app`), remove comentários `//`, extrai o corpo de `enum ConfigIndex`, `HudIndex`, `EventType`, `ResultIndex` e calcula os valores respeitando `= N` explícitos e a numeração sequencial. Asserções: `Cfg.CONTRACT_ID..CARGO` e `Cfg.COUNT`, `Hud.CT_ID..CT_BOMBS` e `Hud.SIZE`, `Ev.CONTRACT`, `Res.ALERTS..BOMBS` e `Res.SIZE` batem com `C_*`, `H_*`, `EV_*`, `R_*`. Pega o erro mais caro desta área (índice trocado entre linguagens) sem aparelho.
- `engineConfigWritesTheContractBlock` — `FloatArray(Cfg.COUNT).applyContract(plan, rules)` para `RED_NIGHT` e `SILENT_RECOVERY` (fase 2): confere cada posição, inclusive `AMMO_CLIPS == 3.2f` e `RED_SKY == 1f`; para `NONE`: todas as posições do bloco são 0 exceto `CONTRACT_ID`.

### 10.5 Teste de host do `contract_rules.h` (opcional, só se houver `g++`/`clang++` no PC)
`app/src/test/cpp/contract_rules_test.cpp`, sem dependências, compilado à mão: `g++ -std=c++17 -I app/src/main/cpp/game app/src/test/cpp/contract_rules_test.cpp -o contract_rules_test && contract_rules_test`.
```cpp
#include <cassert>
#include "contract_rules.h"
using namespace zz::contractrules;
int main() {
    assert(reload(0, 30, -1).ammo == 30 && reload(0, 30, -1).reservePct == -1);      // ilimitada
    assert(reload(0, 30, 320).ammo == 30 && reload(0, 30, 320).reservePct == 220);   // recarga completa custa 100
    assert(reload(29, 30, 100).ammo == 30 && reload(29, 30, 100).reservePct == 96);  // 1 bala custa 4% (arredonda para cima)
    assert(reload(10, 30, 50).ammo == 25 && reload(10, 30, 50).reservePct == 0);     // reserva parcial
    assert(reload(0, 5, 3).ammo == 0 && reload(0, 5, 3).reservePct == 0);            // poeira
    assert(!canReload(0, 30, 0) && !canReload(30, 30, 100) && canReload(0, 30, -1));
    assert(reserveRounds(220, 30) == 66 && reserveRounds(-1, 30) == -1);
    assert(scaledGuards(4, 0.5f) == 2 && scaledGuards(2, 0.5f) == 1 && scaledGuards(6, 1.0f) == 6);
}
```
Se não houver compilador de host, estes mesmos casos viram a lista de verificação manual do cenário D1.

### 10.6 Cenários de diagnóstico reproduzíveis no aparelho (sem emulador)
Pré-requisito: `adb install -r` do APK debug; `adb logcat -s ZombieZoneNative` (tag do `core/log.h`). O jogador de teste precisa ter liberado a fase 3 (ou usar um save de teste). Use os códigos do cap. 4.3.

| Cenário | Passos | Resultado esperado |
|---|---|---|
| **D1 reserva e silêncio** — `ZZ-8080-Y4G0-1011S` (fase 2, silenciosa) | Preparação > `CÓDIGO…` > colar > PARTIR. Disparar até esvaziar e recarregar até acabar. Chegar a menos de 6,5 m (3,5 m agachado) de um zumbi parado. Abrir um baú. | Log de início `Contrato 1: reserva=320% ... guardas=0.50`; a cada recarga completa a reserva cai 100 (`320 -> 220 -> 120 -> 20 -> 0`, a última recarga é parcial); eventos `ÚLTIMO PENTE!` (ao cruzar 100) e `SEM MUNIÇÃO!`; o gatilho vazio sem reserva só dá o clique e a faca ainda funciona; `Alerta #1 (tipo ...) a x m` no log; baú soma +50; chip do HUD mostra `ALERTAS n` e `RESERVA m`. |
| **D2 noite vermelha** — `ZZ-80C0-Y4G0-300CK` (fase 3) | Colar, PARTIR, tirar captura (`adb exec-out screencap -p`), andar de frente a um zumbi parado em pé e depois agachado. | Log `ceuVermelho=1 visao=0.45 agressao=1.35`; céu e névoa vermelhos, horizonte visível a poucas dezenas de metros; primeiro `Alerta` a ~28 m em pé (21 x 1,35) e ~17 m agachado (contra 21/12,6 na expedição padrão); extração paga `extração + 50%` do saque no resultado. |
| **D3 resistência** — `ZZ-C040-0FT0-50355` | Colar na aba SOBREVIVÊNCIA, jogar 2 ondas e cair. | Ranking registra a partida como sempre; resultado mostra "Resistência" e o código; `adb shell run-as com.fabiosilva.ecosdeeldra cat shared_prefs/zombie_zone_native.xml` contém `expeditions` com o registro. |
| **D4 variante** — `ZZ-C0R0-00RR-301G3` | Sobrevivência com Noite vermelha. | Céu vermelho nas ondas; bônus `+50%` da recompensa de ondas na linha CONTRATO; DOBRAR dobra só a base. |
| **D5 determinismo** | Iniciar D1 duas vezes (fechando a missão no meio). | Os logs `Missão 2: ilha 123456, ... rota N m` têm o **mesmo** `rota` e número de ninhos; o ponto de largada é o mesmo; trocar o contrato do mesmo código-base (D1 vs `ZZ-8080-Y4G0-3...` gerado no app) mantém a ilha. |
| **D6 neutralidade** | Expedição padrão antes/depois do passo 2. | Mesmo `FPS`, sem `Contrato` além de `Contrato 0: reserva=-1% ...`; recarga infinita; nada visual muda. |

## 11. Critérios de aceitação

Mapeados nos critérios do spec:
1. **"Um contrato altera decisões reais … e comunica custo e recompensa antes do início."** Preparação mostra CUSTO/RISCO, RECOMPENSA e OBJETIVO derivados dos mesmos números do motor (teste `cardNumbersComeFromTheSameTuningTheEngineReceives`); Noite vermelha muda visão e percepção (D2) e Silenciosa muda munição e furtividade (D1).
2. **"Uma seed/contrato reproduzível … pode ser compartilhado sem serviço remoto."** Código `ZZ-…` decodificável offline, ilha idêntica (D5), teste de ida e volta e vetores dourados. (Cartão de imagem fica para a etapa 5.)
3. **"O jogo continua compilando para arm64-v8a e armeabi-v7a, testes passam."** `assembleDebug` nas duas ABIs no passo 6; suíte completa verde (incluindo `RulesTest`, `InventoryTest`, `PvpTest`, `LookTest`).
4. **Contrato não monetizado:** nenhum botão de vídeo na preparação; DOBRAR exclui o contrato; teste `doublingNeverIncludesTheContractBonus`.
5. **Sobrevivência como expedição:** pelo menos o fluxo de preparação, plano/código, histórico e resultado com contrato; ranking intacto; acesso à sobrevivência restabelecido a partir da Home.
6. **Sem regressão da expedição padrão:** D6; PvP intacto (`reservePct_` e `alerts_` zerados em `startPvp`).
7. **Persistência:** reabrir o app preserva último contrato por modo e histórico; save antigo carrega sem erro.
8. **Nada de dificuldade por receita; IDs reais do AdMob inalterados.**
9. **Novo jogador:** antes da primeira vitória, JOGAR parte direto sem tela de contratos.

## 12. Riscos e mitigações

| # | Risco | Mitigação |
|---|---|---|
| R1 | Colisão de edições em arquivos quentes (`MainActivity`, `protocol.h`, `game.h`, `NativeProtocol.kt`, `HudView`, `SaveStore`) | Arquivos novos para quase tudo (`game_contract.cpp`, `contract_rules.h`, `ContractConfig.kt`, as quatro de `data`, as duas telas); faixas de índice reservadas (5.1); tabela de trechos exatos (cap. 13). |
| R2 | Dois codecs de código divergentes (esta área e a etapa 5) | Formato fixado igual ao de `scratchpad/ref_code.py`; `ExpeditionCode.kt` pertence a esta etapa; a etapa 5 só importa. O orquestrador deve apontar um dono único. |
| R3 | Reserva de munição quebra o ritmo (frustração, soft-lock) | Faca/granadas/molotov/fumaça sempre disponíveis; baús dão +½ pente; log de `Contrato` a cada 5 s para calibrar; valores só em `Contracts.engineRules`; se necessário subir `ammoClips` base de 3,0 para 4,0. |
| R4 | Meta de silêncio impossível porque danificar um ninho alerta os guardas num raio de 35 m | Guardas pela metade + penalidade contínua (−3 por alerta) em vez de degraus; zumbis gerados por ninhos não contam; calibrar `objectivePenalty` pelo log `Alerta #n`. |
| R5 | `Hud.SIZE`/`Res.SIZE`/`Cfg.COUNT` mudam e algum `FloatArray` fixo fica pequeno | Todas as alocações usam `Hud.SIZE`/`Res.SIZE`/`Cfg.COUNT`; o teste de paridade falha se C++ e Kotlin divergirem. |
| R6 | Código de fase alta daria primeira vitória/estrelas a quem não chegou lá | `Contracts.check` recusa fases acima de `highestUnlockedStage()`; o teste cobre. |
| R7 | Dessincronia texto x regra (ex.: "45%" no cartão e 0,55 no motor) | O cartão é **gerado** de `engineRules`/`RewardTuning`; teste de consistência. |
| R8 | `EditText` + modo imersivo/IME em paisagem | `IME_FLAG_NO_EXTRACT_UI`, `COLAR` como caminho principal, `enterImmersive()` já é chamado em `onWindowFocusChanged`; se o teclado atrapalhar, deixar só `COLAR` (decisão local do implementador, não bloqueia). |
| R9 | O código fixa ilha e regras, não as melhorias do jogador: comparar resultados é desigual | Está dito na própria tela do código ("mesma ilha e regras; suas melhorias contam"). Normalizar atributos em desafios é uma etapa futura, fora de escopo. |
| R10 | `Campaign.difficulty` muda de assinatura na limpeza da dificuldade por receita | Esta etapa só usa o `Difficulty` já calculado por `launchMission` e `copy()`; nenhum parâmetro novo é passado a `Campaign`. |
| R11 | Chip do HUD sobrepõe a faixa superior em redesenho | Uma função isolada com posição única; seguir o layout da etapa 1 ao integrar. |
| R12 | Efeito colateral em PvP | `startPvp` zera `reservePct_`/`alerts_`; `ammoLimited()` exige `Mode::Mission`; `MissionConfig{}` padrão é neutro. |
| R13 | Build de 4 GB de RAM | Um arquivo `.cpp` novo (compilação única, pequena); `contract_rules.h` é header-only. |
| R14 | Acentos em scripts de edição no Windows | Fazer edições com acentos pelo `Edit`/`Write` ou por arquivo `.py` (memória do projeto: não usar heredoc). |

## 13. Arquivos e trechos (hot files)

**Criar:** `app/src/main/java/com/fabiosilva/ecosdeeldra/data/{Contracts,ExpeditionCode,ExpeditionLog,ExpeditionRules}.kt`; `.../engine/ContractConfig.kt`; `.../ui/ExpeditionPrepScreen.kt` (inclui `CodeEntryOverlay`); `.../ui/ExpeditionResultOverlay.kt`; `app/src/main/cpp/game/contract_rules.h`; `app/src/main/cpp/game/game_contract.cpp`; testes `ExpeditionCodeTest`, `ContractsTest`, `ExpeditionLogTest`, `ExpeditionRulesTest`, `ContractProtocolParityTest`; opcional `app/src/test/cpp/contract_rules_test.cpp`.

**Editar (quentes marcados com ★):**

| Arquivo | Trechos desta área |
|---|---|
| ★ `MainActivity.kt` | M1-M15 (cap. 6.2): campo `plan`; `showExpeditionPrep`; `startExpedition`/`launchWithAdGate` (substituem `startStage`/`startCampaignPlay`/`beginStage`/`startSurvival`); `launchMission` (diff, semente, `applyContract`, rótulo); `onMissionReady`; `onGameEvent` (`Ev.CONTRACT`); `readContractRun`; `finishMission`; `recordDefeat`; `showDefeat`; `abandonMission`/`skipStageWithAd`/`leaveAfterVictory`/`leaveSurvival`; `recordSurvival`/`finishSurvival`; `pauseSubtitle`; `onBack` |
| ★ `protocol.h` | blocos de `ConfigIndex`, `HudIndex`, `EventType`, `ResultIndex` (5.1) |
| ★ `NativeProtocol.kt` | `Cfg`, `Hud`, `Ev`, `Res` (5.1) |
| ★ `game.h` | `#include "contract_rules.h"`; campos novos em `MissionConfig`; `Zombie::counted`; declaração de `applyContractTheme`; métodos e membros de contrato; (4C) `Nest::spawnCount` e parâmetro `forceKind` de `spawnZombie` |
| ★ `game.cpp` | `updateWeapon` (3 edições), supply pickup, `updateMission`, `finishResult`, `publishHud`, `startPvp` (N4-N9) |
| `game_mission.cpp` | `startMission` (tema + `startContractState`), `updatePois` (guardas), `updateChests` (N1-N3); (4C) `updateNests` |
| `game_zombies.cpp` | percepção e 4 trocas para `alertZombie` (N10) |
| `game_items.cpp` | `addItem` (N11) |
| `jni_bridge.cpp` | leitura dos campos novos (5.2) |
| `CMakeLists.txt` | `game/game_contract.cpp` |
| ★ `HudView.kt` | `drawContractChip` + 1 chamada em `onDraw`; reserva ao lado do botão RECARREGAR (`drawReload`); (4D) marcador na bússola |
| `Progress.kt` | `val expeditions = ExpeditionLog()` |
| ★ `SaveStore.kt` | 2 linhas (cap. 8) |
| `HomeScreen.kt` L270, `StagesScreen.kt` L74, `SurvivalScreen.kt` L86 | 1 chamada cada (6.2 M3) |
| `Overlays.kt` | `DefeatOverlay` (assinatura); remover `VictoryOverlay` ao fim |
| `SurvivalScreen.kt` | `SurvivalResultOverlay` com `plan`/`contract` opcionais |

## 14. Coordenação com as outras áreas

- **HUD/tutorial (etapas 1-2):** o chip do contrato e a reserva usam a mesma disciplina de "um aviso por vez" (nenhum `hud.message` novo fora dos dois eventos de munição); a posição do chip deve ser decidida junto da faixa superior segmentada. Objetivo textual do README/HUD/tutorial continua o mesmo ("destrua ninhos, recolha cristais, extraia"): contratos acrescentam regras, não mudam o objetivo.
- **Limpeza de dificuldade por anúncio:** independente; só garantir que `launchMission` continue produzindo um `Difficulty` e que nenhum teste novo dependa de `estimatedAdRevenueMicros`.
- **Save recuperável:** só as duas linhas do cap. 8; `ExpeditionLog.load` é à prova de falhas.
- **Desempenho (etapa 3):** nada aqui toca streaming; a escala de `viewRadius_`/gráficos continua independente da névoa do contrato.
- **Etapa 5 (ficha e compartilhamento):** consome `ExpeditionCode`, `ExpeditionRecord`, `Contracts.shareText(plan)` e `ExpeditionLog.records`; precisa de `R_ALERTS` (já exposto) e, para o cartão, do nome do contrato. Esta etapa **não** gera imagem nem abre o seletor de compartilhamento.
- **Etapa 6 (recursos e deployables):** a reserva em pentes é o gancho para o "kit de campo" converter recursos em munição limitada (`grantReserve`); os módulos de 4C são colecionáveis em `ExpeditionLog.modules`.
- **Etapa 7:** a extração de `ExpeditionFlow` para fora de `MainActivity` é natural depois desta etapa (M1-M15 já formam um bloco coeso).

## 15. Notas de ambiente

- Windows, PowerShell/Git Bash; PC com 4 GB: **um Gradle por vez**, `--no-daemon --max-workers=1`, `-Pandroid.injected.build.abi=arm64-v8a` na iteração; nunca iniciar emulador; testes de jogo no aparelho do usuário por `adb`.
- Não trocar IDs reais do AdMob nem criar variante debug com IDs de teste (o aparelho já é dispositivo de teste em `ADMOB_TEST_DEVICES`). Isso sobrepõe a frase do spec sobre IDs de teste em debug; esta etapa não mexe em anúncios além do descrito no cap. 7.
- Interface e comentários em português (pt-BR); formatar números nos textos de UI com vírgula decimal sem depender do locale da JVM.
- Ao escrever arquivos com acentos por script, usar arquivo `.py`/`Write`/`Edit`; evitar heredoc do bash.
- Comandos úteis:
  - `.\gradlew.bat --no-daemon --max-workers=1 :app:testDebugUnitTest`
  - `.\gradlew.bat --no-daemon --max-workers=1 testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.ExpeditionCodeTest"`
  - `.\gradlew.bat --no-daemon --max-workers=1 -Pandroid.injected.build.abi=arm64-v8a :app:assembleDebug`
  - `adb install -r app\build\outputs\apk\debug\app-debug.apk` e `adb logcat -s ZombieZoneNative`

