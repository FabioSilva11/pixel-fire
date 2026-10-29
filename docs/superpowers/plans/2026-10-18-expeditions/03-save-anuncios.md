# 03 — Save recuperável, configuração de anúncios e limpeza da dificuldade por receita

Etapa 2 do spec `docs/superpowers/specs/2026-10-18-zombie-zone-expeditions-design.md`
(parte "Confiança e continuidade"). Este documento é o contrato para o agente implementador:
arquivos exatos, assinaturas, código-chave, ordem dos passos, testes e critérios de aceitação.
Todos os números de linha abaixo se referem ao working tree de 2026-10-18 (com o merge não
commitado); confirme com `Grep` antes de editar, porque outras áreas mexem nos mesmos arquivos.

Convenções do projeto que valem aqui: textos e comentários em português; no máximo UM Gradle por vez,
sempre `--no-daemon --max-workers=1`; nunca emulador (testar no aparelho do usuário via adb); nunca
trocar IDs de anúncio por IDs de teste.

---

## 0. Resumo das decisões

| Tema | Decisão |
|---|---|
| Armazenamento | Sai do `SharedPreferences` (chave `progress`) e vai para arquivos em `filesDir/save/` com escrita atômica, backups rotativos e quarentena. O prefs antigo é lido uma única vez (migração) e **nunca apagado**. |
| JSON inválido | Jamais vira `Progress()` em silêncio. Ordem: primário -> gravação interrompida (`.tmp`) -> `bak1` -> `bak2`; o arquivo ruim é copiado para quarentena antes de ser sobrescrito; com um backup válido o jogo abre com aviso; sem nenhum válido, **tela de recuperação** e o disco fica intocado até o usuário escolher. |
| Lógica pura | Novo pacote `data.save` sem `android.*` (só `java.io` + `org.json`), testável na JVM. A parte com `Context` fica numa fachada fina: `data/SaveStore.kt`. |
| `org.json` nos testes | O `android.jar` de testes só tem stubs ("not mocked"). Solução: `testImplementation("org.json:json:20250517")` (implementação real; vai antes do `android.jar` no classpath) + teste-sentinela. **Não** usar `isReturnDefaultValues = true`. |
| Versão de esquema | `"version": 5` (a chave já existia; valia 4 e nunca era lida). Passos de migração em `SaveMigrations`. Campos aditivos NÃO incrementam a versão (ver 3.7). |
| Anúncios | IDs reais e `ADMOB_TEST_DEVICES` permanecem **exatamente como estão** em `app/build.gradle.kts`. A frase do spec "variantes debug usarão IDs de teste" é **DESCARTADA** (desvio deliberado, seção 2). Teste-sentinela impede regressão. |
| Dificuldade por receita | Removida por inteiro: `Campaign.adRevenueDifficultyTier`/`AD_REVENUE_*`, parâmetros de `difficulty`, `Progress.adRevenueMicros/Currency`, `RewardedAds.onRevenueEstimated`, bloco em `MainActivity`, texto em `SettingsScreen`, teste antigo. Saves antigos com esses campos continuam abrindo (campos ignorados e não regravados). O acúmulo ILRD também é removido (era só insumo da dificuldade). |
| Intersticial do 5º toque | Apenas **relatado** (seção 8); nada é removido. |
| Bug achado de passagem | `MainActivity.launchMission` deixou de passar o nível de "Pente estendido" à dificuldade desde o commit `f92ad28`; a limpeza volta a `Campaign.difficulty(stage.number, p)` (seção 6). |

Pré-requisito para o orquestrador/implementador: o commit `dcc3e30` (HEAD) **contém marcadores de
conflito** (`<<<<<<< HEAD` em `MainActivity.kt:156`) e uma segunda `Campaign.stage()` que referencia
`stages` inexistente; **HEAD não compila**. O working tree corrige isso. Não rode `git checkout`,
`git stash` nem `git restore` sobre esses arquivos; preserve o working tree (commitar antes de
começar é o ideal, decisão do orquestrador).

---

## 1. Estado atual (o que foi lido)

### 1.1 Save (`data/SaveStore.kt`, 220 linhas)

- Persistência: `getSharedPreferences("zombie_zone_native")`, chave `progress`, uma string JSON.
- `load()` (linhas 11-14): `runCatching { ...decode(JSONObject(raw)) }.getOrElse { Progress() }`.
  Qualquer exceção (JSON truncado, `stages.getJSONObject(key)` em valor que não é objeto na linha 129,
  `arr.getJSONObject(i)` em `daily` na linha 181) vira **jogo novo em silêncio**.
- `MainActivity.onCreate` (linhas 189-191) chama `Rules.onAppOpen` e `store.save(progress)`
  imediatamente: o JSON corrompido é sobrescrito pelo jogo novo no mesmo arranque. **Perda irreversível.**
  É exatamente o que o spec proíbe.
- `save()` usa `apply()`: assíncrono, sem sinal de falha. O XML do próprio SharedPreferences, se
  corromper, também é lido como vazio pela plataforma (reset silencioso fora do nosso controle).
- `"version"` é gravado (4; o comentário da classe diz "versão 3") e **nunca lido**.
- Nada impede NaN/Infinity em `bestTime` (`put(Double.NaN)` lança `JSONException` dentro de `save()`).
- `allowBackup="false"` no manifesto: desinstalar = perder tudo (coerente com "sem cloud save"; só
  documentar).
- Quem chama o store: `MainActivity` linhas 145-146 (criação/carga), 163 (bloco ILRD, será removido),
  191, 213, 224 (`onPause`), 274 (`fun save()` público usado por ~20 telas via `app.save()`).
  Nenhuma outra classe usa SharedPreferences.

### 1.2 Testes e `org.json`

`app/build.gradle.kts` só tem `testImplementation("junit:junit:4.13.2")`. Hoje nenhum teste toca
`JSONObject`. `SaveStore` não é testável na JVM (usa `Context`). No cache do Gradle desta máquina existem
`org.json:json` 20240303, 20250517 e 20260814 (jars; só o 20260814 tem `.pom` em cache). A primeira
resolução de 20250517 precisa de rede (alguns KB do Maven Central, repositório já declarado em
`settings.gradle.kts`); se estiver offline, usar `20260814`.

### 1.3 Anúncios

- `app/build.gradle.kts` linhas 20-23 (em `defaultConfig`, valem para debug e release):
  - `admobAppId` = `ca-app-pub-6598765502914364~3458194932`
  - `ADMOB_REWARDED_ID` = `ca-app-pub-6598765502914364/5788844807`
  - `ADMOB_INTERSTITIAL_ID` = `ca-app-pub-6598765502914364/6741641853`
  - `ADMOB_TEST_DEVICES` = `E812F36934272DE8BFD5D96B7E64BE83` (aparelho do usuário)
  Não há `buildTypes.debug`. O `BuildConfig` gerado em `app/build/.../debug/BuildConfig.java` confirma
  os mesmos valores. O manifesto usa `${admobAppId}` (sem ID literal) e o merge inclui `AD_ID`.
- `AdConsent.kt` (81 linhas): UMP -> `requestConsentInfoUpdate` -> `loadAndShowConsentFormIfRequired` ->
  só então `MobileAds.initialize` (em thread). `setRequestConfiguration` (test devices + classificação
  `MAX_AD_CONTENT_RATING_T`) é aplicado **antes** de `initialize`. `RewardedAds.load` e
  `InterstitialAds.load` exigem `AdConsent.ready`. Botão "PRIVACIDADE DOS ANÚNCIOS" nos Ajustes quando
  `privacyOptionsRequired`. Está conforme o guia de início rápido do UMP.
- `RewardedAds.kt` registra `OnPaidEventListener` (ILRD) -> `onRevenueEstimated` (linhas 24-25, 35-37).
- `README.md` linha 84 diz "Os IDs de AdMob são **de teste**": **está errado** hoje (são reais; o
  aparelho do usuário é que está cadastrado como dispositivo de teste).

### 1.4 Dificuldade por receita (código a remover)

- `Campaign.kt` 123-170: `AD_REVENUE_MICROS_PER_LEVEL`, `MAX_AD_REVENUE_DIFFICULTY_LEVEL`,
  `adRevenueDifficultyTier`, parâmetros `estimatedAdRevenueMicros` e `adRevenueDifficultyEnabled`
  (default `true` no working tree!), multiplicadores `healthMultiplier/damageMultiplier/speedMultiplier`
  e `+ adTier` em `zombieCount`.
- `Progress.kt` 101-103: `adRevenueMicros`, `adRevenueCurrency`.
- `SaveStore.kt` 80-81 e 189-190.
- `MainActivity.kt` 153-166 (acúmulo) e 503-507 (uso em `launchMission`).
- `SettingsScreen.kt` 63-76 (texto "Dificuldade adaptativa ativa automaticamente..."), imports das linhas 14-15.
- `RulesTest.kt` 191-206 (`adRevenueDifficultyIsOptInAndBounded`).
- `README.md` linha 81.

### 1.5 O que o merge não commitado já mudou (resumo; detalhes na seção 8)

Comparando HEAD (`dcc3e30`) com o working tree: marcadores de conflito resolvidos; `StoryOverlay` e
`startStage(..., skipStory)` removidos; **bônus de combate por anúncio** (`boostReady`, botões em
`StagesScreen`/`SurvivalScreen`) removido; **dificuldade por receita trocada de opt-in (padrão
desligado, com chave em Ajustes) para automática (padrão ligado, sem chave)** - o oposto do spec;
**intersticial a cada 5º toque em JOGAR** adicionado.

---

## 2. Desvios deliberados do spec (registro)

1. **IDs de teste em debug: DESCARTADO por decisão do usuário.** O spec diz "Variantes debug usarão
   IDs de teste de anúncios; IDs reais ficam apenas em release". O usuário proibiu trocar os IDs reais
   e o aparelho dele já está registrado em `ADMOB_TEST_DEVICES`, o que faz o AdMob entregar anúncios de
   teste nesse aparelho com os IDs reais (evita tráfego inválido). Portanto:
   - não criar `buildTypes.debug` com IDs de teste, nem flavors, nem `local.properties` que troque IDs;
   - `ADMOB_TEST_DEVICES` fica como está; **não** alterar `app/build.gradle.kts` nas linhas 18-23;
   - o risco que o spec queria cobrir (APK debug em outro aparelho gerando impressões reais na conta) é
     mitigado por **procedimento**: ao instalar qualquer APK em outro aparelho, copiar o hash que o SDK
     imprime no Logcat ("Use RequestConfiguration.Builder().setTestDeviceIds(...)") para
     `ADMOB_TEST_DEVICES` (lista separada por vírgula; `AdConsent` já faz `split(',')`);
   - o teste-sentinela `AdConfigTest` (seção 9) trava qualquer regressão para IDs de demonstração do
     Google (`3940256099942544`) ou para um bloco `debug { ... ADMOB ... }`.
   O dono da documentação deve registrar este desvio no spec (parágrafo "Confiança e continuidade") e
   corrigir `README.md:84`.
2. **"Dificuldade nunca definida por receita"**: cumprido removendo o código (nenhum desvio).
3. Nenhum outro desvio. O 5º toque é relatado, não alterado (decisão do usuário).

---

## 3. Arquitetura do save recuperável

### 3.1 Arquivos no aparelho

Diretório: `File(context.filesDir, "save")` (interno e privado). Nomes:

| Arquivo | Conteúdo / quando existe |
|---|---|
| `save.json` | Primário. |
| `save.json.tmp` | Gravação em andamento (fsync feito antes do rename). Sobra só se o processo morreu entre `writeTmp` e `promoteTmp`. |
| `save.json.bak1`, `save.json.bak2` | Backups rotativos (bak1 = mais novo). |
| `save.json.bak1.part` etc. | Temporário de cópia atômica de backup (some sozinho). |
| `save.corrupt-<crc32>.json` | Cópia do primário/backup ilegível (quarentena, dedup por CRC, máx. 5). |
| `save.partial-<crc32>.json` | Original de um save que abriu com seções reiniciadas (máx. 5). |
| `save.pre-v<N>.json` | Original de um save de esquema N < atual, guardado **uma vez** antes de migrar (nunca sobrescrito). |
| `save.legacy-prefs.json` | Cópia literal da string do SharedPreferences no momento da migração (uma vez). |
| `save.newer-v<N>.json` | Original de um save de esquema N > atual (aberto em modo tolerante). |
| `save.unreadable-<ts>-<nome>` | Arquivo que não pôde ser lido (erro de E/S) e foi apenas renomeado para não ser destruído. |

### 3.2 Formato (JSON)

Mesmos nomes de campo do `SaveStore` atual (compatível com builds anteriores), com estas diferenças:
`"version": 5`; novo `"writtenBy"` (VERSION_NAME do app, só diagnóstico); sem `adRevenueMicros`,
`adRevenueCurrency` nem `settings.adRevenueDifficulty`; chaves de `stages` e `upgrades` ordenadas
(saída determinística); valores float sanitizados (NaN/Infinity viram 0).
Chaves de topo desconhecidas (de versões futuras) são preservadas em `Progress.saveExtras` e regravadas.

### 3.3 Algoritmo de carga (`SaveRepository.load()`)

```text
1. P = avaliar(save.json)          // Missing | Unreadable(IOException) | Invalid(motivo) | Valid
2. Se P é Valid -> Loaded(origem=PRIMARY). FIM.   (backups não são nem lidos)
3. Avaliar, nesta ordem: save.json.tmp, bak1, bak2. O primeiro Valid vence:
     Loaded(origem=INTERRUPTED_WRITE | BACKUP_1 | BACKUP_2) + aviso RestoredFromBackup
     (sem aviso quando P é Missing e a origem é o .tmp: foi só uma gravação interrompida).
     O primário ruim NÃO é tocado agora: será copiado para quarentena na próxima gravação.
4. Se P, tmp, bak1 e bak2 estão todos Missing:
     lê o SharedPreferences antigo (uma vez):
       vazio            -> Fresh (instalação nova)
       válido           -> keepOnce(save.legacy-prefs.json); persist(...) imediato; Loaded(LEGACY_PREFS)
       inválido         -> keepOnce(save.legacy-prefs.json); NeedsRecovery
5. Caso contrário (há arquivos, nenhum válido): NeedsRecovery(relatório) e `recoveryPending = true`.
   NENHUMA escrita acontece; toda gravação posterior retorna Blocked até o usuário escolher.
```

Seções do JSON: contêineres com tipo errado (`stages` que não é objeto, `heroes` que não é array...)
e escalares críticos com tipo errado (`diamonds`, `stars`, níveis de upgrade) tornam o arquivo
**Invalid** (vai para os backups). Entradas soltas malformadas em listas leves (`survival`, `daily`)
são descartadas com aviso `SectionsReset` e o original é guardado em `save.partial-*.json`.

### 3.4 Algoritmo de gravação (`SaveRepository.persist(text)`)

```text
0. Se recoveryPending -> Blocked (não escreve nada).
1. Autoverificação: SaveCodec.decode(text) tem que dar Ok; senão Failed (nunca grava o que o app
   não consegue ler de volta; protege de bug do encoder).
2. writeTmp: save.json.tmp <- bytes UTF-8, flush, fd.sync().
3. Se o primário existe e NÃO é sabidamente válido (veio de fallback/restauração/novo jogo):
   preservar: texto -> quarentena (save.corrupt-<crc>.json); ilegível -> renomear (setAside).
4. Se o primário é sabidamente válido: rotação de backups (não fatal; erro só é logado):
     devida se (primeira gravação da sessão) OU (>= 15 min desde a última) OU forceRotate,
     e só se o primário difere de bak1 (byte a byte).
     bak1 válido  -> bak2 := bak1 (rename);  bak1 inválido -> quarentena e bak2 fica intacto;
     bak1 := cópia atômica do primário (.part + fsync + rename, mtime preservado).
5. promoteTmp: rename(save.json.tmp, save.json)  (atômico no Linux/Android).
6. primaryKnownValid = true.
```

Janela de falha: morrer entre 2 e 5 deixa `save.json` antigo íntegro + `.tmp` completo. A carga
prefere o primário válido (perde no máximo a última gravação, ~nada em prática). Documentado e testado.

Regra do rename: `File.renameTo` em Linux/Android substitui o destino atomicamente. No Windows (só nos
testes JVM) falha se o destino existe; `SaveFiles.moveOver` faz `delete + renameTo` **somente** quando
`os.name` começa com "Windows". Em Android nunca apaga o destino.

### 3.5 Recuperação explícita

- `NeedsRecovery(RecoveryReport)`: lista de candidatos (rótulo, arquivo, INVALID/UNREADABLE, motivo,
  tamanho, data), `readErrors` e `restorable` (backups/snapshots que **são** válidos, ex.: `pre-v4`).
- A `RecoveryScreen` oferece: **TENTAR DE NOVO**, **RESTAURAR <snapshot>** (um botão por item de
  `restorable`), **COMEÇAR JOGO NOVO** (com confirmação; nada é apagado), **COPIAR ARQUIVOS PARA
  SUPORTE** (copia `filesDir/save/*` para `getExternalFilesDir("save-recovery")/save-<data>/`) e
  **FECHAR O JOGO** (`finish()`, disco intocado). O botão Voltar do sistema = `finish()`.
- Erro de E/S (`readErrors`) não é tratado como corrupção: o texto pede para liberar espaço e tentar de
  novo; "novo jogo" só renomeia (`setAside`) os arquivos ilegíveis, nunca os apaga.
- `restore(BackupInfo)` e `startNewGame()` desbloqueiam as gravações só depois de gravar com sucesso.
  `restore` força rotação (o primário atual, se válido, vai para `bak1`): restaurar é reversível.

### 3.6 Migração do SharedPreferences (usuários atuais, incluindo o aparelho do dono)

Primeira execução do APK novo: não há `save/` -> lê `zombie_zone_native/progress` -> decodifica (v4 ->
migra em memória para v5) -> copia a string original para `save.legacy-prefs.json` -> grava o primário
-> o prefs antigo **permanece intocado** (remover só numa versão futura, depois de 2 releases). Se o
usuário instalar uma build antiga depois, ela lê o prefs (defasado); isso é limitação aceita e deve
ser avisada ao dono (não instalar builds antigas por cima depois da migração).

### 3.7 Protocolo para outras áreas adicionarem campos ao save

1. Declare o campo em `Progress`/`Settings` **com default**.
2. `SaveCodec.encode`: grave com o nome do campo; se for chave de topo, acrescente a `KNOWN_KEYS`
   (o teste `knownKeysCoverEveryEncodedKey` falha se esquecer).
3. `SaveCodec.decodeBody`: leia com `opt*` + default (leve) ou `strict*` (se perder o dado for grave).
4. Mudança **aditiva** (campo novo com default): NÃO incrementa `SCHEMA_VERSION`. Mudança destrutiva
   (renomear, mudar semântica/unidade, remover): incrementa `SCHEMA_VERSION`, adiciona o passo em
   `SaveMigrations.steps` e um fixture de teste do formato anterior. Chave aposentada vai para
   `REMOVED_KEYS`; nunca reutilize o nome com outro significado.
5. Acrescente o campo ao teste `fullProgressRoundTrips`.

---

## 4. Código novo (pacote `com.fabiosilva.ecosdeeldra.data.save`)

Todos em `app/src/main/java/com/fabiosilva/ecosdeeldra/data/save/`. Sem `android.*`.

### 4.1 `SaveModels.kt`

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Progress
import java.io.File

/** De onde veio o progresso carregado. */
enum class Origin { PRIMARY, INTERRUPTED_WRITE, BACKUP_1, BACKUP_2, LEGACY_PREFS }

data class SaveSummary(val diamonds: Int, val completedStages: Int, val stars: Int, val heroName: String, val playMinutes: Long)

/** Um arquivo válido que o usuário pode restaurar (backup, snapshot de migração, save antigo). */
data class BackupInfo(val label: String, val file: File, val modifiedAt: Long, val summary: SaveSummary)

enum class CandidateStatus { INVALID, UNREADABLE }

data class RecoveryCandidate(
    val label: String,
    val fileName: String,
    val status: CandidateStatus,
    val detail: String,
    val sizeBytes: Long,
    val modifiedAt: Long,
)

data class RecoveryReport(
    val candidates: List<RecoveryCandidate>,
    /** Algum arquivo não pôde ser LIDO (E/S): "tentar de novo" é a ação principal. */
    val readErrors: Boolean,
    /** Itens válidos que o usuário pode restaurar mesmo com o primário/backups ilegíveis. */
    val restorable: List<BackupInfo>,
)

/** Avisos não bloqueantes mostrados depois da tela inicial. */
sealed interface SaveNotice {
    data class RestoredFromBackup(val origin: Origin, val savedAt: Long, val summary: SaveSummary) : SaveNotice
    data class SectionsReset(val sections: List<String>) : SaveNotice
    data class NewerSchema(val found: Int) : SaveNotice
}

sealed interface LoadResult {
    /** Instalação nova: nada no disco nem no formato antigo. Único caso em que "jogo novo" é automático. */
    data object Fresh : LoadResult
    data class Loaded(val progress: Progress, val origin: Origin, val notices: List<SaveNotice>) : LoadResult
    /** Há arquivos, mas nenhum utilizável. O disco não foi tocado; gravações ficam bloqueadas. */
    data class NeedsRecovery(val report: RecoveryReport) : LoadResult
}

sealed interface PersistResult {
    data object Written : PersistResult
    /** Recuperação pendente: nada foi escrito (de propósito). */
    data object Blocked : PersistResult
    data class Failed(val error: Throwable) : PersistResult
}
```

### 4.2 `SaveCodec.kt`

Porta o `encode`/`decode` atuais de `SaveStore.kt` (mantenha os nomes de campo idênticos). O corpo de
cada seção é o código de `SaveStore.decode` linhas 109-213, com as mudanças marcadas `// MUDOU`.

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Cosmetics
import com.fabiosilva.ecosdeeldra.data.DailyMission
import com.fabiosilva.ecosdeeldra.data.Hero
import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.Look
import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.Rules
import com.fabiosilva.ecosdeeldra.data.StageRecord
import com.fabiosilva.ecosdeeldra.data.SurvivalRecord
import com.fabiosilva.ecosdeeldra.data.Upgrades
import org.json.JSONArray
import org.json.JSONObject

class SaveFormatException(message: String) : Exception(message)

sealed interface DecodeOutcome {
    data class Ok(val progress: Progress, val schemaFound: Int, val warnings: List<String>) : DecodeOutcome {
        val isNewerThanApp: Boolean get() = schemaFound > SaveCodec.SCHEMA_VERSION
    }
    data class Invalid(val reason: String) : DecodeOutcome
}

/** Conversão Progress <-> JSON. Lógica pura: não conhece arquivos nem Android. */
object SaveCodec {
    const val SCHEMA_VERSION = 5

    /** Chaves de topo que o app conhece. O resto é preservado em Progress.saveExtras. */
    internal val KNOWN_KEYS: Set<String> = setOf(
        "version", "writtenBy", "diamonds", "ownedSkins", "selectedHero", "heroes", "stages",
        "vitality", "power", "reload", "upgrades", "inventory", "survival", "supplyAdsDay", "supplyAdsToday",
        "stats", "lastLoginDay", "calendarClaims", "lastCalendarClaim", "dailyDay", "daily", "dailyBonusClaimed",
        "adsDay", "adsToday", "chestReadyAt", "tutorialSeen", "playTapsSinceAd", "missionsSinceInterstitial",
        "lastAdAt", "settings",
    )

    /** Aposentadas: ignoradas na leitura e nunca mais gravadas (nem como extras). */
    internal val REMOVED_KEYS: Set<String> = setOf("adRevenueMicros", "adRevenueCurrency")
    internal val REMOVED_SETTINGS_KEYS: Set<String> = setOf("adRevenueDifficulty")

    private fun Float.finiteOr(default: Float = 0f): Float = if (isFinite()) this else default

    fun encode(p: Progress, writtenBy: String = ""): String {
        val o = JSONObject()
        o.put("version", SCHEMA_VERSION)                                   // MUDOU: 5
        if (writtenBy.isNotEmpty()) o.put("writtenBy", writtenBy)           // NOVO
        o.put("diamonds", p.diamonds)
        o.put("ownedSkins", JSONArray(p.ownedCosmetics.toList()))           // nome antigo mantido
        o.put("selectedHero", p.selectedHero)
        o.put("heroes", JSONArray().apply { p.heroes.forEach { put(JSONObject().put("name", it.name).put("look", it.look)) } })
        o.put("stages", JSONObject().apply {
            for ((n, r) in p.stages.toSortedMap()) {                        // MUDOU: ordenado
                put(n.toString(), JSONObject().apply {
                    put("completed", r.completed)
                    put("stars", r.stars)
                    put("bestTime", r.bestTime.finiteOr().toDouble())       // MUDOU: sem NaN
                    put("crystalMask", r.crystalMask)
                    put("fails", r.fails)
                    put("skipped", r.skipped)
                })
            }
        })
        o.put("vitality", p.vitality)
        o.put("power", p.power)
        o.put("reload", p.reload)
        o.put("upgrades", JSONObject().apply { p.upgrades.toSortedMap().forEach { (k, v) -> put(k, v) } })
        o.put("inventory", p.inventory.encode())
        o.put("survival", JSONArray().apply {
            p.survivalRecords.forEach {
                put(JSONObject().put("name", it.name).put("seconds", it.seconds).put("wave", it.wave).put("kills", it.kills).put("score", it.score).put("date", it.date))
            }
        })
        o.put("supplyAdsDay", p.supplyAdsDay)
        o.put("supplyAdsToday", p.supplyAdsToday)
        o.put("stats", JSONObject().apply {
            // mesmas 18 chaves de SaveStore.encode linhas 51-68, sem alteração
            put("kills", p.stats.kills); put("headshots", p.stats.headshots); put("wins", p.stats.wins)
            put("crystals", p.stats.crystals); put("playSeconds", p.stats.playSeconds); put("adsWatched", p.stats.adsWatched)
            put("loginDays", p.stats.loginDays); put("bestCombo", p.stats.bestCombo); put("pvpKills", p.stats.pvpKills)
            put("pvpWins", p.stats.pvpWins); put("pvpMatches", p.stats.pvpMatches); put("chests", p.stats.chests)
            put("nests", p.stats.nests); put("distance", p.stats.distance); put("losses", p.stats.losses)
            put("survivalRuns", p.stats.survivalRuns); put("bestWave", p.stats.bestWave)
            put("bestSurvivalSeconds", p.stats.bestSurvivalSeconds)
        })
        o.put("lastLoginDay", p.lastLoginDay)
        o.put("calendarClaims", p.calendarClaims)
        o.put("lastCalendarClaim", p.lastCalendarClaim)
        o.put("dailyDay", p.dailyDay)
        o.put("daily", JSONArray().apply {
            p.daily.forEach { put(JSONObject().put("id", it.id).put("progress", it.progress).put("claimed", it.claimed)) }
        })
        o.put("dailyBonusClaimed", p.dailyBonusClaimed)
        o.put("adsDay", p.adsDay)
        o.put("adsToday", p.adsToday)
        // REMOVIDO: adRevenueMicros / adRevenueCurrency
        o.put("chestReadyAt", p.chestReadyAt)
        o.put("tutorialSeen", p.tutorialSeen)
        o.put("playTapsSinceAd", p.playTapsSinceAd)
        o.put("missionsSinceInterstitial", p.missionsSinceInterstitial)
        o.put("lastAdAt", p.lastAdAt)
        o.put("settings", JSONObject().apply {
            val s = p.settings
            put("sensitivity", s.sensitivity.finiteOr(1f).toDouble())
            put("musicVolume", s.musicVolume.finiteOr(0.6f).toDouble())
            put("sfxVolume", s.sfxVolume.finiteOr(1f).toDouble())
            put("aimAssist", s.aimAssist)
            put("autoFire", s.autoFire)
            put("invertY", s.invertY)
            put("vibration", s.vibration)
            put("quality", s.quality)
            put("reminders", s.reminders)
            put("showFps", s.showFps)
            put("leftHanded", s.leftHanded)
            put("playerName", s.playerName)
            put("cinematic", s.cinematic)
            put("hudLayout", s.hudLayout)
            put("hudOpacity", s.hudOpacity.finiteOr(0.85f).toDouble())
        })
        // Campos de versões futuras: devolvidos como vieram.
        p.saveExtras.forEach { (k, raw) ->
            if (k !in KNOWN_KEYS && k !in REMOVED_KEYS && !o.has(k)) {
                try { o.put(k, JSONArray(raw).get(0)) } catch (e: Exception) { /* extra ilegível: descarta */ }
            }
        }
        return o.toString()
    }

    fun decode(raw: String): DecodeOutcome {
        val o = try {
            JSONObject(raw.removePrefix("﻿").trim())
        } catch (e: Exception) {
            return DecodeOutcome.Invalid("JSON inválido: ${e.message.orEmpty().take(100)}")
        }
        val found = SaveMigrations.detectSchema(o)
        if (found < 0) return DecodeOutcome.Invalid("formato não reconhecido (sem 'version' nem campos do jogo)")
        if (found <= SCHEMA_VERSION) SaveMigrations.migrate(o, found)   // esquema futuro: leitura tolerante, sem migrar
        val warnings = mutableListOf<String>()
        return try {
            DecodeOutcome.Ok(decodeBody(o, warnings), found, warnings)
        } catch (e: SaveFormatException) {
            DecodeOutcome.Invalid(e.message ?: "seção crítica inválida")
        } catch (e: Exception) {
            DecodeOutcome.Invalid("erro inesperado: ${e.javaClass.simpleName}")
        }
    }

    fun summarize(p: Progress) = SaveSummary(p.diamonds, p.completedStages(), p.totalStars(), p.currentHero.name, p.stats.playSeconds / 60)

    // ------------------------------------------------------------------ leitura

    private fun JSONObject.strictInt(key: String, default: Int): Int {
        if (isNull(key)) return default
        val v = get(key)
        if (v !is Number) throw SaveFormatException("'$key' deveria ser número")
        return v.toLong().coerceIn(Int.MIN_VALUE.toLong(), Int.MAX_VALUE.toLong()).toInt()
    }

    private fun JSONObject.strictObject(key: String): JSONObject? {
        if (isNull(key)) return null
        return get(key) as? JSONObject ?: throw SaveFormatException("'$key' deveria ser objeto")
    }

    private fun JSONObject.strictArray(key: String): JSONArray? {
        if (isNull(key)) return null
        return get(key) as? JSONArray ?: throw SaveFormatException("'$key' deveria ser lista")
    }

    private fun JSONObject.strictString(key: String): String {
        if (isNull(key)) return ""
        return get(key) as? String ?: throw SaveFormatException("'$key' deveria ser texto")
    }

    private fun finiteFloat(d: Double): Float = if (d.isNaN() || d.isInfinite()) 0f else d.toFloat().coerceAtLeast(0f)

    private fun decodeBody(o: JSONObject, warnings: MutableList<String>): Progress {
        val p = Progress()

        // ---- críticas: formato errado => save inválido (cai para os backups; nunca zera em silêncio) ----
        p.diamonds = o.strictInt("diamonds", 0).coerceAtLeast(0)
        o.strictArray("ownedSkins")?.let { arr ->
            for (i in 0 until arr.length()) arr.optInt(i, -1).takeIf { it in Cosmetics.all.indices }?.let { p.ownedCosmetics.add(it) }
        }
        p.ownedCosmetics.add(0)
        o.strictArray("heroes")?.let { arr ->
            val list = (0 until arr.length()).mapNotNull { i ->
                val h = arr.optJSONObject(i) ?: return@mapNotNull null
                Hero(Rules.sanitizeName(h.optString("name", "Herói")), h.optInt("look", Look().encode()))
            }
            if (arr.length() > 0 && list.isEmpty()) throw SaveFormatException("lista de heróis ilegível")   // MUDOU
            if (list.isNotEmpty()) {
                p.heroes.clear()
                p.heroes.addAll(list.take(Rules.MAX_HEROES))
            }
        }
        p.selectedHero = o.strictInt("selectedHero", 0).coerceIn(0, p.heroes.size - 1)
        o.strictObject("stages")?.let { stages ->
            for (key in stages.keys()) {
                val n = key.toIntOrNull() ?: continue
                if (n < 1) continue
                val r = stages.strictObject(key) ?: throw SaveFormatException("fase $n ilegível")            // MUDOU
                p.stages[n] = StageRecord(
                    completed = r.optBoolean("completed"),
                    stars = r.strictInt("stars", 0).coerceIn(0, 3),
                    bestTime = finiteFloat(r.optDouble("bestTime", 0.0)),
                    crystalMask = r.strictInt("crystalMask", 0) and 0b111,
                    fails = r.strictInt("fails", 0).coerceAtLeast(0),
                    skipped = r.optBoolean("skipped"),
                )
            }
        }
        p.vitality = o.strictInt("vitality", 0).coerceIn(0, 60)
        p.power = o.strictInt("power", 0).coerceIn(0, 60)
        p.reload = o.strictInt("reload", 0).coerceIn(0, 60)
        o.strictObject("upgrades")?.let { u ->
            for (key in u.keys()) if (Upgrades.get(key) != null) p.upgrades[key] = u.strictInt(key, 0).coerceIn(0, 60)
        }
        if (o.has("inventory")) p.inventory.copyFrom(Inventory.decode(o.strictString("inventory")))

        // ---- leves: entradas ruins são descartadas e viram aviso; contêiner com tipo errado ainda é crítico ----
        o.strictArray("survival")?.let { arr ->
            var dropped = 0
            for (i in 0 until arr.length()) {
                val r = arr.optJSONObject(i)
                if (r == null) { dropped++; continue }
                p.survivalRecords.add(SurvivalRecord(r.optString("name", "Jogador"), r.optInt("seconds"), r.optInt("wave"), r.optInt("kills"), r.optInt("score"), r.optString("date", "")))
            }
            if (dropped > 0) warnings.add("ranking de sobrevivência")
        }
        p.supplyAdsDay = o.optString("supplyAdsDay", "")
        p.supplyAdsToday = o.optInt("supplyAdsToday").coerceAtLeast(0)
        o.strictObject("stats")?.let { s ->
            fun n(key: String) = s.optInt(key).coerceAtLeast(0)
            p.stats.kills = n("kills"); p.stats.headshots = n("headshots"); p.stats.wins = n("wins")
            p.stats.crystals = n("crystals"); p.stats.playSeconds = s.optLong("playSeconds").coerceAtLeast(0L)
            p.stats.adsWatched = n("adsWatched"); p.stats.loginDays = n("loginDays"); p.stats.bestCombo = n("bestCombo")
            p.stats.pvpKills = n("pvpKills"); p.stats.pvpWins = n("pvpWins"); p.stats.pvpMatches = n("pvpMatches")
            p.stats.chests = n("chests"); p.stats.nests = n("nests"); p.stats.distance = s.optLong("distance").coerceAtLeast(0L)
            p.stats.losses = n("losses"); p.stats.survivalRuns = n("survivalRuns"); p.stats.bestWave = n("bestWave")
            p.stats.bestSurvivalSeconds = n("bestSurvivalSeconds")
        }
        p.lastLoginDay = o.optString("lastLoginDay", "")
        p.calendarClaims = o.optInt("calendarClaims").coerceAtLeast(0)
        p.lastCalendarClaim = o.optString("lastCalendarClaim", "")
        p.dailyDay = o.optString("dailyDay", "")
        o.strictArray("daily")?.let { arr ->
            var dropped = 0
            for (i in 0 until arr.length()) {
                val d = arr.optJSONObject(i)
                if (d == null) { dropped++; continue }                                                       // MUDOU: era getJSONObject
                val id = d.optString("id")
                if (Rules.DAILY_POOL.any { it.id == id }) p.daily.add(DailyMission(id, d.optInt("progress").coerceAtLeast(0), d.optBoolean("claimed")))
            }
            if (dropped > 0) { p.daily.clear(); p.dailyDay = ""; warnings.add("missões diárias") }           // refreshDaily recria
        }
        p.dailyBonusClaimed = o.optBoolean("dailyBonusClaimed")
        p.adsDay = o.optString("adsDay", "")
        p.adsToday = o.optInt("adsToday").coerceAtLeast(0)
        // REMOVIDO: adRevenueMicros / adRevenueCurrency (qualquer valor presente é ignorado)
        p.chestReadyAt = o.optLong("chestReadyAt")
        p.tutorialSeen = o.optBoolean("tutorialSeen")
        p.playTapsSinceAd = o.optInt("playTapsSinceAd").coerceIn(0, Rules.CAMPAIGN_PLAY_AD_EVERY - 1)
        p.missionsSinceInterstitial = o.optInt("missionsSinceInterstitial").coerceAtLeast(0)
        p.lastAdAt = o.optLong("lastAdAt")
        o.strictObject("settings")?.let { s ->
            val t = p.settings
            t.sensitivity = s.optDouble("sensitivity", 1.0).toFloat().coerceIn(0.3f, 2.5f)
            t.musicVolume = s.optDouble("musicVolume", 0.6).toFloat().coerceIn(0f, 1f)
            t.sfxVolume = s.optDouble("sfxVolume", 1.0).toFloat().coerceIn(0f, 1f)
            t.aimAssist = s.optBoolean("aimAssist", true)
            t.autoFire = s.optBoolean("autoFire", true)
            t.invertY = s.optBoolean("invertY", false)
            t.vibration = s.optBoolean("vibration", true)
            t.quality = s.optInt("quality", 1).coerceIn(0, 2)
            t.reminders = s.optBoolean("reminders", true)
            t.showFps = s.optBoolean("showFps", false)
            t.leftHanded = s.optBoolean("leftHanded", false)
            t.playerName = s.optString("playerName", "")
            t.cinematic = s.optBoolean("cinematic", false)
            t.hudLayout = s.optString("hudLayout", "")
            t.hudOpacity = s.optDouble("hudOpacity", 0.85).toFloat().coerceIn(0.3f, 1f)
        }

        // Campos desconhecidos (versões futuras): guardados verbatim, sem tipo (lista de 1 elemento preserva string x número).
        for (key in o.keys()) {
            if (key !in KNOWN_KEYS && key !in REMOVED_KEYS) p.saveExtras[key] = JSONArray().put(o.get(key)).toString()
        }
        return p
    }
}
```

Observações para quem for portar: `NaN` de `optDouble` não ocorre em JSON válido, mas `finiteFloat`
cobre. `isNull(key)` é `true` para chave ausente ou `null` nas duas implementações de `org.json`.
`catch (e: Exception)` não usa `_` (sublinhado em catch exige Kotlin mais novo).

### 4.3 `SaveMigrations.kt`

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import org.json.JSONObject

/** Passos de migração do JSON bruto entre versões de esquema. Cada passo é puro e idempotente. */
internal object SaveMigrations {
    /** Campos que existiam antes do campo "version" ser lido (versões <= 3 não tinham marcador confiável). */
    private val LEGACY_MARKERS = listOf("diamonds", "stages", "heroes", "stats", "ownedSkins", "inventory")

    /** @return versão do esquema, 0 para legado sem "version", -1 se o JSON não parece um save do jogo. */
    fun detectSchema(o: JSONObject): Int {
        if (o.has("version")) {
            val v = o.opt("version")
            return if (v is Number && v.toInt() >= 0) v.toInt() else -1
        }
        return if (LEGACY_MARKERS.any { o.has(it) }) 0 else -1
    }

    /** Chave = versão de origem; o passo leva o JSON de `from` para `from + 1`. Ausente = identidade. */
    private val steps: Map<Int, (JSONObject) -> Unit> = mapOf(
        4 to { o: JSONObject -> v4ToV5(o) },
    )

    fun migrate(o: JSONObject, from: Int): Int {
        var v = from
        while (v < SaveCodec.SCHEMA_VERSION) {
            steps[v]?.invoke(o)
            v++
        }
        o.put("version", v)
        return v
    }

    /** v4 -> v5: aposenta o estado de receita de anúncios (a dificuldade nunca depende de anúncios). */
    private fun v4ToV5(o: JSONObject) {
        SaveCodec.REMOVED_KEYS.forEach { o.remove(it) }
        o.optJSONObject("settings")?.let { s -> SaveCodec.REMOVED_SETTINGS_KEYS.forEach { s.remove(it) } }
    }
}
```

### 4.4 `SaveFiles.kt` (E/S de arquivos; só `java.io`)

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.CRC32

sealed interface FileRead {
    data object Missing : FileRead
    data class Text(val text: String) : FileRead
    data class TooBig(val bytes: Long) : FileRead
    data class Failed(val error: Throwable) : FileRead
}

class SaveFiles(val dir: File) {
    val primary = File(dir, "save.json")
    val tmp = File(dir, "save.json.tmp")
    val bak1 = File(dir, "save.json.bak1")
    val bak2 = File(dir, "save.json.bak2")

    /** Nunca lança: ausência, tamanho absurdo e erro de E/S são resultados distintos. */
    fun read(f: File): FileRead = try {
        when {
            !f.exists() -> FileRead.Missing
            f.isFile && f.length() > MAX_BYTES -> FileRead.TooBig(f.length())
            else -> FileRead.Text(f.readBytes().toString(Charsets.UTF_8))   // diretório no lugar do arquivo cai em Failed
        }
    } catch (e: Exception) {
        FileRead.Failed(e)
    }

    fun writeTmp(text: String) {
        dir.mkdirs()
        FileOutputStream(tmp).use { out ->
            out.write(text.toByteArray(Charsets.UTF_8))
            out.flush()
            out.fd.sync()
        }
    }

    fun promoteTmp() = moveOver(tmp, primary)

    /** rename(2) substitui o destino de forma atômica no Android/Linux; o desvio para Windows serve só aos testes JVM. */
    fun moveOver(from: File, to: File) {
        if (from.renameTo(to)) return
        if (IS_WINDOWS && to.exists() && to.delete() && from.renameTo(to)) return
        throw IOException("não foi possível mover ${from.name} para ${to.name}")
    }

    /** Cópia que nunca deixa um destino pela metade: grava em .part, sincroniza e só então renomeia. */
    fun copyAtomic(src: File, dst: File) {
        val part = File(dst.path + ".part")
        src.inputStream().use { input ->
            FileOutputStream(part).use { out ->
                input.copyTo(out)
                out.flush()
                out.fd.sync()
            }
        }
        part.setLastModified(src.lastModified())
        moveOver(part, dst)
    }

    /** Guarda uma cópia do texto ruim (dedup por CRC, no máximo 5 por prefixo). Melhor esforço: null se falhar. */
    fun quarantine(text: String, prefix: String): File? = try {
        dir.mkdirs()
        val f = File(dir, "save.$prefix-${crc(text)}.json")
        if (!f.exists()) {
            f.writeText(text, Charsets.UTF_8)
            prune("save.$prefix-", QUARANTINE_KEEP)
        }
        f
    } catch (e: Exception) {
        null
    }

    /** Grava o arquivo só se ainda não existir (snapshots de migração). Melhor esforço. */
    fun keepOnce(name: String, text: String): File? = try {
        dir.mkdirs()
        val f = File(dir, name)
        if (!f.exists()) f.writeText(text, Charsets.UTF_8)
        f
    } catch (e: Exception) {
        null
    }

    /** Arquivo ilegível (E/S): em vez de apagar, só renomeia. Lança se não conseguir. */
    fun setAside(f: File, tag: String, nowMs: Long): File {
        val dst = File(dir, "save.$tag-$nowMs-${f.name}")
        if (!f.renameTo(dst)) throw IOException("não foi possível separar ${f.name}")
        return dst
    }

    fun copyAllTo(target: File): Int {
        target.mkdirs()
        var n = 0
        dir.listFiles()?.filter { it.isFile }?.forEach { it.copyTo(File(target, it.name), overwrite = true); n++ }
        return n
    }

    private fun prune(prefix: String, keep: Int) {
        val all = dir.listFiles { f -> f.name.startsWith(prefix) } ?: return
        all.sortedByDescending { it.lastModified() }.drop(keep).forEach { it.delete() }
    }

    private fun crc(text: String): String {
        val c = CRC32()
        c.update(text.toByteArray(Charsets.UTF_8))
        return c.value.toString(16).padStart(8, '0')
    }

    companion object {
        const val MAX_BYTES = 4L * 1024 * 1024
        const val QUARANTINE_KEEP = 5
        private val IS_WINDOWS = System.getProperty("os.name").orEmpty().startsWith("Windows")
    }
}
```

### 4.5 `SaveRepository.kt` (orquestração; sem Android)

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Progress
import java.io.File
import java.io.IOException

internal sealed interface Eval {
    data object Missing : Eval
    data class Unreadable(val error: Throwable) : Eval
    data class Invalid(val reason: String) : Eval
    data class Valid(val ok: DecodeOutcome.Ok, val text: String) : Eval
}

class SaveRepository(
    private val files: SaveFiles,
    /** Leitor do SharedPreferences antigo (injetado pela fachada Android; nos testes é uma lambda). */
    private val legacy: () -> String? = { null },
    private val writtenBy: String = "",
    private val clock: () -> Long = { System.currentTimeMillis() },
    private val log: (String) -> Unit = {},
) {
    @Volatile private var blocked = false
    private var primaryKnownValid = false
    private var sessionChecked = false
    private var lastRotationAt = 0L

    /** true enquanto a recuperação explícita não foi resolvida: toda gravação é recusada. */
    val recoveryPending: Boolean get() = blocked

    // ------------------------------------------------------------------ carga

    @Synchronized
    fun load(): LoadResult {
        blocked = false
        primaryKnownValid = false
        sessionChecked = false

        val primary = eval(files.primary)
        if (primary is Eval.Valid) return accept(primary, files.primary, Origin.PRIMARY, primary)

        val tmp = eval(files.tmp)
        val bak1 = eval(files.bak1)
        val bak2 = eval(files.bak2)
        val chain = listOf(
            Triple(Origin.INTERRUPTED_WRITE, files.tmp, tmp),
            Triple(Origin.BACKUP_1, files.bak1, bak1),
            Triple(Origin.BACKUP_2, files.bak2, bak2),
        )
        for ((origin, file, ev) in chain) {
            if (ev is Eval.Valid) return accept(ev, file, origin, primary)
        }
        if (primary is Eval.Missing && tmp is Eval.Missing && bak1 is Eval.Missing && bak2 is Eval.Missing) return loadLegacyOrFresh()

        blocked = true
        return LoadResult.NeedsRecovery(
            report(
                listOf(
                    Triple("Save principal", files.primary, primary),
                    Triple("Gravação interrompida", files.tmp, tmp),
                    Triple("Cópia de segurança 1", files.bak1, bak1),
                    Triple("Cópia de segurança 2", files.bak2, bak2),
                ),
            ),
        )
    }

    private fun accept(v: Eval.Valid, file: File, origin: Origin, primary: Eval): LoadResult.Loaded {
        val notices = mutableListOf<SaveNotice>()
        val found = v.ok.schemaFound
        if (found > SaveCodec.SCHEMA_VERSION) {
            files.keepOnce("save.newer-v$found.json", v.text)
            notices += SaveNotice.NewerSchema(found)
        } else if (found < SaveCodec.SCHEMA_VERSION) {
            files.keepOnce("save.pre-v$found.json", v.text)            // snapshot antes de migrar; nunca sobrescrito
        }
        if (v.ok.warnings.isNotEmpty()) {
            files.quarantine(v.text, "partial")
            notices += SaveNotice.SectionsReset(v.ok.warnings)
        }
        if (origin == Origin.PRIMARY) {
            primaryKnownValid = true
        } else if (!(origin == Origin.INTERRUPTED_WRITE && primary is Eval.Missing)) {
            notices += SaveNotice.RestoredFromBackup(origin, file.lastModified(), SaveCodec.summarize(v.ok.progress))
        }
        return LoadResult.Loaded(v.ok.progress, origin, notices)
    }

    private fun loadLegacyOrFresh(): LoadResult {
        val raw = try { legacy() } catch (e: Exception) { null }
        if (raw.isNullOrBlank()) return LoadResult.Fresh
        files.keepOnce("save.legacy-prefs.json", raw)
        return when (val d = SaveCodec.decode(raw)) {
            is DecodeOutcome.Ok -> {
                val progress = d.progress
                val r = persist(SaveCodec.encode(progress, writtenBy))           // cria o primário já no formato novo
                if (r is PersistResult.Failed) log("migração do prefs não gravou: ${r.error.message}")
                LoadResult.Loaded(progress, Origin.LEGACY_PREFS, emptyList())
            }
            is DecodeOutcome.Invalid -> {
                blocked = true
                LoadResult.NeedsRecovery(
                    RecoveryReport(
                        listOf(RecoveryCandidate("Save antigo (preferências)", "save.legacy-prefs.json", CandidateStatus.INVALID, d.reason, raw.length.toLong(), 0L)),
                        readErrors = false,
                        restorable = emptyList(),
                    ),
                )
            }
        }
    }

    private fun eval(f: File): Eval = when (val r = files.read(f)) {
        FileRead.Missing -> Eval.Missing
        is FileRead.Failed -> Eval.Unreadable(r.error)
        is FileRead.TooBig -> Eval.Invalid("arquivo grande demais (${r.bytes} bytes)")
        is FileRead.Text -> when (val d = SaveCodec.decode(r.text)) {
            is DecodeOutcome.Ok -> Eval.Valid(d, r.text)
            is DecodeOutcome.Invalid -> Eval.Invalid(d.reason)
        }
    }

    private fun report(parts: List<Triple<String, File, Eval>>): RecoveryReport {
        val candidates = parts.mapNotNull { (label, f, ev) ->
            when (ev) {
                Eval.Missing, is Eval.Valid -> null
                is Eval.Unreadable -> RecoveryCandidate(label, f.name, CandidateStatus.UNREADABLE, ev.error.message ?: ev.error.javaClass.simpleName, f.length(), f.lastModified())
                is Eval.Invalid -> RecoveryCandidate(label, f.name, CandidateStatus.INVALID, ev.reason, f.length(), f.lastModified())
            }
        }
        return RecoveryReport(candidates, candidates.any { it.status == CandidateStatus.UNREADABLE }, listRestorable())
    }

    // ------------------------------------------------------------------ gravação

    @Synchronized
    fun persist(text: String, forceRotate: Boolean = false): PersistResult {
        if (blocked) return PersistResult.Blocked
        return try {
            val check = SaveCodec.decode(text)
            if (check is DecodeOutcome.Invalid) throw SaveFormatException("o app gerou um save ilegível: ${check.reason}")
            files.writeTmp(text)
            if (primaryKnownValid) {
                try { maybeRotate(forceRotate) } catch (e: Exception) { log("backup rotativo falhou: ${e.message}") }
            } else {
                preserveCurrentPrimary()
            }
            files.promoteTmp()
            primaryKnownValid = true
            PersistResult.Written
        } catch (e: Exception) {
            PersistResult.Failed(e)
        }
    }

    /** Nunca sobrescreve um arquivo que não sabemos ser válido sem antes guardá-lo. */
    private fun preserveCurrentPrimary() {
        if (!files.primary.exists()) return
        when (val r = files.read(files.primary)) {
            is FileRead.Text -> files.quarantine(r.text, "corrupt")
            FileRead.Missing -> Unit
            is FileRead.TooBig, is FileRead.Failed -> files.setAside(files.primary, "unreadable", clock())
        }
    }

    private fun maybeRotate(force: Boolean) {
        val now = clock()
        val due = force || !sessionChecked || now - lastRotationAt >= BACKUP_MIN_INTERVAL_MS
        if (!due || !files.primary.exists()) return
        sessionChecked = true
        lastRotationAt = now
        val current = files.primary.readBytes()
        val b1 = if (files.bak1.exists()) files.bak1.readBytes() else null
        if (b1 != null && b1.contentEquals(current)) return
        rotate()
    }

    private fun rotate() {
        when (val r = files.read(files.bak1)) {
            is FileRead.Text -> {
                if (SaveCodec.decode(r.text) is DecodeOutcome.Ok) files.moveOver(files.bak1, files.bak2)   // bak2 := bak1 (válido)
                else files.quarantine(r.text, "corrupt")                                                    // bak1 ruim: bak2 fica intacto
            }
            FileRead.Missing -> Unit
            is FileRead.TooBig, is FileRead.Failed -> return                                                // não destrói o que não consegue ler
        }
        files.copyAtomic(files.primary, files.bak1)
    }

    // ------------------------------------------------------------------ recuperação

    /** Itens válidos restauráveis: bak1, bak2, snapshots pre-v*, save antigo do SharedPreferences. */
    @Synchronized
    fun listRestorable(): List<BackupInfo> {
        val slots = mutableListOf("Cópia de segurança 1" to files.bak1, "Cópia de segurança 2" to files.bak2)
        files.dir.listFiles { f -> f.name.startsWith("save.pre-v") && f.name.endsWith(".json") }
            ?.sortedByDescending { it.name }
            ?.forEach { slots += "Antes da atualização do formato (${it.name.removePrefix("save.pre-").removeSuffix(".json")})" to it }
        slots += "Save antigo (antes da migração)" to File(files.dir, "save.legacy-prefs.json")
        return slots.mapNotNull { (label, f) ->
            val ev = eval(f)
            if (ev is Eval.Valid) BackupInfo(label, f, f.lastModified(), SaveCodec.summarize(ev.ok.progress)) else null
        }
    }

    /** Escolha explícita do usuário: começar do zero. Nada é apagado (o ruim vai para quarentena/renomeado). */
    @Synchronized
    fun startNewGame(): Progress {
        val fresh = Progress()
        blocked = false
        primaryKnownValid = false
        files.tmp.delete()
        val r = persist(SaveCodec.encode(fresh, writtenBy))
        if (r is PersistResult.Failed) { blocked = true; throw r.error }
        return fresh
    }

    /** Restaura um item de [listRestorable]. Se o primário atual é válido, ele vira bak1 (reversível). */
    @Synchronized
    fun restore(info: BackupInfo): Progress {
        val text = (files.read(info.file) as? FileRead.Text)?.text ?: throw IOException("não foi possível ler ${info.file.name}")
        val ok = SaveCodec.decode(text) as? DecodeOutcome.Ok ?: throw SaveFormatException("a cópia escolhida não é válida")
        val wasBlocked = blocked
        blocked = false
        val r = persist(SaveCodec.encode(ok.progress, writtenBy), forceRotate = true)
        if (r is PersistResult.Failed) { blocked = wasBlocked; throw r.error }
        return ok.progress
    }

    /** Copia todos os arquivos do save para [target] (suporte). @return quantidade copiada. */
    @Synchronized
    fun exportTo(target: File): Int = files.copyAllTo(target)

    companion object {
        /** Intervalo mínimo entre rotações de backup dentro de uma sessão (a 1ª gravação da sessão sempre roda). */
        const val BACKUP_MIN_INTERVAL_MS = 15 * 60_000L
    }
}
```

Notas de revisão do código acima (para o implementador não "corrigir" por engano):
- `restore()` em estado bloqueado: `primaryKnownValid` é `false`, então `persist` preserva o primário
  ruim em vez de rotacionar. Fora do bloqueio (Ajustes), `primaryKnownValid` é `true` e `forceRotate`
  copia o primário atual para `bak1`.
- `load()` é chamado de novo pela "TENTAR DE NOVO"; ele zera `blocked` no início e o recoloca se ainda
  não houver nada utilizável.
- `Eval` é `internal` de propósito (evita erro de "exposição" de tipo privado de arquivo).

### 4.6 `SaveMessages.kt` (textos pt-BR dos avisos; sem Android)

```kotlin
package com.fabiosilva.ecosdeeldra.data.save

import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object SaveMessages {
    private fun date(ms: Long): String = SimpleDateFormat("dd/MM 'às' HH:mm", Locale.forLanguageTag("pt-BR")).format(Date(ms))

    /** (título, mensagem) ou null se o aviso não deve aparecer ao jogador. */
    fun describe(n: SaveNotice): Pair<String, String>? = when (n) {
        is SaveNotice.RestoredFromBackup -> "Save recuperado" to
            "O arquivo principal do seu save estava danificado ou ausente. Restauramos a cópia de segurança de ${date(n.savedAt)} " +
            "(${n.summary.diamonds} diamantes, ${n.summary.completedStages} fases vencidas). O arquivo danificado foi guardado, sem alterações, na pasta do jogo."
        is SaveNotice.SectionsReset -> "Parte do save foi reiniciada" to
            "Estas partes estavam ilegíveis e voltaram ao início: ${n.sections.joinToString(", ")}. O resto do progresso foi mantido e o original ficou guardado."
        is SaveNotice.NewerSchema -> "Save de uma versão mais nova" to
            "Este save foi criado por uma versão mais nova do jogo (formato ${n.found}). Ele foi aberto e o original foi guardado; atualize o jogo para não perder dados novos."
    }
}
```

---

## 5. Mudanças em arquivos existentes

### 5.1 `app/build.gradle.kts` (linha 70, bloco `dependencies`)

```kotlin
    testImplementation("junit:junit:4.13.2")
    // Implementação real de org.json para testes JVM (o android.jar só traz stubs que lançam "not mocked").
    testImplementation("org.json:json:20250517")
```

Não tocar mais nada neste arquivo (principalmente linhas 18-23: IDs de anúncio). Não habilitar
`unitTests.isReturnDefaultValues` (devolveria `null`/0 do stub e esconderia erros). Bump de
`versionCode`/`versionName`: não faz parte desta área; combinar um bump único para a etapa 2 com o dono
da documentação/versão.

### 5.2 `data/Progress.kt`

- Remover (linhas 101-103): o comentário "Valor ILRD estimado...", `var adRevenueMicros = 0L` e
  `var adRevenueCurrency = ""`.
- Acrescentar na classe `Progress` (perto de `settings`):

```kotlin
    /** Campos de versões futuras do save, devolvidos intactos ao gravar (chave -> "[valor]" em JSON). Só o SaveCodec usa. */
    val saveExtras: MutableMap<String, String> = mutableMapOf()
```

### 5.3 `data/SaveStore.kt` (reescrever como fachada; mantém o nome da classe)

```kotlin
package com.fabiosilva.ecosdeeldra.data

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.fabiosilva.ecosdeeldra.BuildConfig
import com.fabiosilva.ecosdeeldra.data.save.BackupInfo
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.PersistResult
import com.fabiosilva.ecosdeeldra.data.save.SaveCodec
import com.fabiosilva.ecosdeeldra.data.save.SaveFiles
import com.fabiosilva.ecosdeeldra.data.save.SaveRepository
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

/**
 * Fachada Android do save: arquivos em filesDir/save (ver data/save/SaveRepository).
 * O JSON é montado na thread chamadora (foto consistente do Progress) e gravado numa thread única,
 * juntando pedidos seguidos. onPause usa [saveNow] (síncrono, com limite de 2 s).
 */
class SaveStore(context: Context) {
    private val app = context.applicationContext
    private val repo = SaveRepository(
        files = SaveFiles(File(app.filesDir, "save")),
        legacy = { app.getSharedPreferences(LEGACY_PREFS, Context.MODE_PRIVATE).getString(LEGACY_KEY, null) },
        writtenBy = BuildConfig.VERSION_NAME,
        log = { Log.w(TAG, it) },
    )
    private val io = Executors.newSingleThreadExecutor { r -> Thread(r, "zz-save").apply { isDaemon = true } }
    private val pending = AtomicReference<String?>(null)
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var failureReported = false

    /** Chamado na thread principal, uma vez por sequência de falhas de gravação. */
    var onSaveFailed: ((Throwable) -> Unit)? = null

    val recoveryPending: Boolean get() = repo.recoveryPending

    fun load(): LoadResult = repo.load()

    fun save(p: Progress) {
        if (repo.recoveryPending) return
        if (!snapshot(p)) return
        io.execute { flushPending() }
    }

    /** Grava já (onPause). Espera até 2 s; se estourar, a gravação segue na thread de E/S. */
    fun saveNow(p: Progress) {
        if (repo.recoveryPending) return
        if (!snapshot(p)) return
        try { io.submit { flushPending() }.get(2, TimeUnit.SECONDS) } catch (e: Exception) { Log.w(TAG, "saveNow demorou: ${e.message}") }
    }

    private fun snapshot(p: Progress): Boolean = try {
        pending.set(SaveCodec.encode(p, BuildConfig.VERSION_NAME))
        true
    } catch (e: Exception) {
        reportFailure(e)
        false
    }

    private fun flushPending() {
        val text = pending.getAndSet(null) ?: return
        when (val r = repo.persist(text)) {
            PersistResult.Written -> failureReported = false
            PersistResult.Blocked -> Unit
            is PersistResult.Failed -> reportFailure(r.error)
        }
    }

    private fun reportFailure(e: Throwable) {
        Log.w(TAG, "falha ao gravar o save", e)
        if (failureReported) return
        failureReported = true
        main.post { onSaveFailed?.invoke(e) }
    }

    // ---- recuperação (RecoveryScreen / Ajustes) ----
    fun startNewGame(): Progress = repo.startNewGame()
    fun restorable(): List<BackupInfo> = repo.listRestorable()
    fun restore(b: BackupInfo): Progress = repo.restore(b)

    /** Copia os arquivos do save para uma pasta legível por adb/gerenciador de arquivos. @return a pasta ou null. */
    fun exportForSupport(): File? {
        val base = app.getExternalFilesDir("save-recovery") ?: return null
        val target = File(base, "save-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()))
        return if (repo.exportTo(target) > 0) target else null
    }

    private companion object {
        const val TAG = "SaveStore"
        const val LEGACY_PREFS = "zombie_zone_native"
        const val LEGACY_KEY = "progress"
    }
}
```

Todo o conteúdo antigo de `encode`/`decode` sai deste arquivo (vai para `SaveCodec`). O comentário de
classe antigo ("versão 3 = jogo nativo") some.

### 5.4 `MainActivity.kt` (arquivo quente; edições cirúrgicas, na ordem do arquivo)

1. Imports: acrescentar
   `import com.fabiosilva.ecosdeeldra.data.save.LoadResult`,
   `import com.fabiosilva.ecosdeeldra.data.save.Origin`,
   `import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport`,
   `import com.fabiosilva.ecosdeeldra.data.save.SaveMessages`,
   `import com.fabiosilva.ecosdeeldra.data.save.SaveNotice`,
   `import com.fabiosilva.ecosdeeldra.ui.RecoveryScreen`.
2. Linha 80: `private lateinit var store: SaveStore` ->
   `lateinit var store: SaveStore` + `private set` na linha seguinte (a `RecoveryScreen` usa `app.store`).
3. `onCreate`, trecho 145-166 (criação, carga e bloco ILRD). Substituir por:

```kotlin
        store = SaveStore(this)
        store.onSaveFailed = {
            Popup.show(this, "Não foi possível salvar", "O armazenamento do aparelho parece cheio ou indisponível. Libere espaço para não perder o progresso.", "OK")
        }
        val loaded = store.load()
        // Em recuperação o Progress é provisório: store.recoveryPending bloqueia qualquer gravação até o usuário escolher.
        progress = when (loaded) {
            is LoadResult.Loaded -> loaded.progress
            is LoadResult.Fresh -> Progress()
            is LoadResult.NeedsRecovery -> Progress()
        }
        audio = GameAudio(this)
        audio.sfxVolume = progress.settings.sfxVolume
        audio.musicVolume = progress.settings.musicVolume
        Ui.clickSound = { audio.playUi("click", 0.6f) }
        ads = RewardedAds(this)
        ads.onStateChanged = { runOnUiThread { homeScreen?.refresh() } }
        interstitials = InterstitialAds(this)
```
   (o bloco `ads.onRevenueEstimated = {...}` das linhas 153-166 é **apagado**).
4. `onCreate`, trecho 189-199 (do `val newDay = ...` ao `postDelayed`) vira:

```kotlin
        AdConsent.gather(this) {
            ads.load()
            interstitials.load()
        }
        if (loaded is LoadResult.NeedsRecovery) {
            showRecovery(loaded.report)
            return
        }
        startSession((loaded as? LoadResult.Loaded)?.notices.orEmpty())
    }

    /** Tudo que o arranque fazia depois de carregar o save; também roda ao sair da tela de recuperação. */
    private fun startSession(notices: List<SaveNotice>) {
        val newDay = Rules.onAppOpen(progress, today())
        Rules.refreshCosmeticUnlocks(progress)
        store.save(progress)
        showHome()
        showSaveNotices(notices)
        if (Rules.canClaimCalendar(progress, today())) {
            surface.postDelayed({ showDailyReward() }, if (newDay) 900L else 600L)
        }
    }

    private fun showRecovery(report: RecoveryReport) {
        hud.visibility = View.GONE
        showScreen(RecoveryScreen(this, report))
    }

    /** Chamado pela RecoveryScreen quando o usuário resolveu (restaurou, tentou de novo com sucesso ou começou novo). */
    fun onRecoveryResolved(newProgress: Progress, notices: List<SaveNotice> = emptyList()) {
        progress = newProgress
        applySettings()      // volume, qualidade e HUD vêm do Progress real; também grava
        startSession(notices)
    }

    private fun showSaveNotices(notices: List<SaveNotice>) {
        val texts = notices.mapNotNull { SaveMessages.describe(it) }
        fun next(i: Int) {
            if (i >= texts.size) return
            val (title, body) = texts[i]
            Popup.show(this, title, body, "OK") { next(i + 1) }
        }
        next(0)
    }
```
   Atenção: `onCreate` precisa continuar chamando, antes disso, `NativeBridge.init`, `setContentView`,
   `enterImmersive`, `engine { NativeBridge.setGraphics(...) }` e o `onBackPressedDispatcher` exatamente
   como hoje (linhas 169-187); só o miolo acima muda. `Origin` só é necessário se o IDE reclamar de
   import não usado: remova-o se não usar.
5. `onResume` (linha 212): `if (!store.recoveryPending && Rules.onAppOpen(progress, today())) { ... }`.
6. `onPause` (linhas 224-225): trocar por

```kotlin
        store.saveNow(progress)
        if (!store.recoveryPending) Reminders.schedule(this, progress.chestReadyAt, progress.settings.reminders)
```
7. `onBack` (linha 255): primeira linha do `when`: `store.recoveryPending -> finish()`.
8. `fun save()` (273-275): corpo `store.save(progress)` permanece (agora assíncrono e ignorado em recuperação).
9. `launchMission` (linhas 501-508): ver seção 6 (volta a `Campaign.difficulty(stage.number, p)`).

### 5.5 `ui/RecoveryScreen.kt` (novo)

Tela cheia opaca, uma `ScrollView` (paisagem estreita 640x360 dp precisa rolar), duas colunas.

```kotlin
package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.graphics.Color
import android.widget.FrameLayout
import android.widget.ScrollView
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.data.save.BackupInfo
import com.fabiosilva.ecosdeeldra.data.save.CandidateStatus
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Mostrada no arranque quando nenhum arquivo de save é utilizável. Nada é gravado até o usuário escolher. */
@SuppressLint("ViewConstructor")
class RecoveryScreen(private val app: MainActivity, private var report: RecoveryReport) : FrameLayout(app) {
    init {
        setBackgroundColor(Palette.bg)
        Ui.blockTouches(this)
        render()
    }

    private fun render() {
        removeAllViews()
        val left = Ui.column(app)
        left.addView(Ui.text(app, "PROTEÇÃO DO PROGRESSO", 10f, Palette.accent, Fonts.title))
        left.addView(Ui.title(app, "NÃO FOI POSSÍVEL ABRIR O SAVE", 22f, Palette.red))
        left.addView(Ui.text(app, if (report.readErrors) READ_TEXT else CORRUPT_TEXT, 13f).apply {
            setLineSpacing(0f, 1.2f)
            setPadding(0, app.dpi(8), 0, app.dpi(10))
        })
        report.candidates.forEach { c ->
            val state = if (c.status == CandidateStatus.UNREADABLE) "não foi possível ler" else "ilegível"
            left.addView(Ui.text(app, "• ${c.label} (${c.fileName}): $state — ${c.detail}", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(3)) })
        }

        val right = Ui.column(app)
        right.addView(action("TENTAR DE NOVO", Palette.lime, Palette.dark) { retry() })
        report.restorable.forEach { b ->
            right.addView(action("RESTAURAR: ${b.label.uppercase()}", Palette.cyan, Palette.dark) { restore(b) })
            right.addView(Ui.text(app, "${b.summary.diamonds} diamantes • ${b.summary.completedStages} fases • ${date(b.modifiedAt)}", 10f, Palette.muted).apply { setPadding(0, 0, 0, app.dpi(8)) })
        }
        right.addView(action("COMEÇAR JOGO NOVO", Palette.gold, Palette.dark) { confirmNewGame() })
        right.addView(action("COPIAR ARQUIVOS PARA SUPORTE", Palette.stone, Color.WHITE) { export() })
        right.addView(action("FECHAR O JOGO", Palette.stone, Color.WHITE) { app.finish() })

        val columns = Ui.row(app, android.view.Gravity.TOP)
        columns.addView(left, Ui.lp(0, Ui.WRAP, 1f).apply { marginEnd = app.dpi(16) })
        columns.addView(right, Ui.lp(0, Ui.WRAP, 1f))
        columns.setPadding(app.dpi(24), app.dpi(16), app.dpi(24), app.dpi(16))
        addView(ScrollView(app).apply { addView(columns) }, Ui.frame(Ui.MATCH, Ui.MATCH))
    }

    private fun action(label: String, fill: Int, textColor: Int, onClick: () -> Unit) =
        Ui.button(app, label, fill, textColor, 13f, onClick).also { it.layoutParams = Ui.lp(Ui.MATCH, app.dpi(46)).apply { bottomMargin = app.dpi(8) } }

    private fun retry() = guarded {
        when (val r = app.store.load()) {
            is LoadResult.Loaded -> app.onRecoveryResolved(r.progress, r.notices)
            is LoadResult.Fresh -> app.onRecoveryResolved(app.store.startNewGame())
            is LoadResult.NeedsRecovery -> { report = r.report; render() }
        }
    }

    private fun restore(b: BackupInfo) = guarded { app.onRecoveryResolved(app.store.restore(b)) }

    private fun confirmNewGame() {
        Popup.show(app, "Começar um jogo novo?", "Os arquivos do save atual continuam guardados na pasta do jogo (nada é apagado), mas você começará do zero.", "COMEÇAR NOVO", "VOLTAR") { ok ->
            if (ok) guarded { app.onRecoveryResolved(app.store.startNewGame()) }
        }
    }

    private fun export() {
        val dir = app.store.exportForSupport()
        Popup.show(app, if (dir != null) "Arquivos copiados" else "Não foi possível copiar", dir?.let { "Copiados para:\n${it.absolutePath}" } ?: "Armazenamento externo indisponível.", "OK")
    }

    private fun guarded(block: () -> Unit) {
        try { block() } catch (e: Exception) {
            Popup.show(app, "Não foi possível concluir", e.message ?: "Erro ao gravar o save. Libere espaço e tente de novo.", "OK")
        }
    }

    private fun date(ms: Long) = SimpleDateFormat("dd/MM HH:mm", Locale.forLanguageTag("pt-BR")).format(Date(ms))

    private companion object {
        const val CORRUPT_TEXT = "Não conseguimos ler o save principal nem as cópias de segurança. Seu progresso NÃO foi apagado: os arquivos foram mantidos sem alteração. Escolha abaixo."
        const val READ_TEXT = "O armazenamento não respondeu ao ler o save. Feche outros apps, libere espaço e toque em TENTAR DE NOVO. Nada foi apagado."
    }
}
```

---

## 6. Remoção da dificuldade por receita (passo a passo)

Valores de referência: com tier 0 as fórmulas atuais já são numericamente iguais às de antes do
recurso (multiplicadores 1,0 e `+ 0`). Logo a remoção **não muda a dificuldade-base**.

1. `Campaign.kt`: apagar linhas 123-133 (KDoc, `AD_REVENUE_MICROS_PER_LEVEL`,
   `MAX_AD_REVENUE_DIFFICULTY_LEVEL`, `adRevenueDifficultyTier`) e reescrever `difficulty`:

```kotlin
    fun difficulty(
        stageNumber: Int,
        vitality: Int,
        power: Int,
        reload: Int,
        magazineLevel: Int = 0,
    ): Difficulty {
        val step = stageNumber.coerceAtLeast(1) - 1
        val late = (step - 19).coerceAtLeast(0)
        // Cada zona nova deixa a horda um pouco mais resistente e perigosa.
        val zoneMul = 1f + (step / STAGES_PER_ZONE) * 0.04f
        return Difficulty(
            zombieCount = (8 + min(step, 30) * 0.6f + late / 10).toInt().coerceAtMost(30),
            zombieHealth = (60f + min(step, 19) * 12f + late * 4f) * zoneMul,
            zombieDamage = ((8f + min(step, 40) * 0.6f + (step - 40).coerceAtLeast(0) * 0.25f) * zoneMul).coerceAtMost(60f),
            zombieSpeed = 1f + min(0.45f, step * 0.022f),
            // (playerHealth, magazine, power, reloadSpeed: manter exatamente como estão hoje)
        )
    }

    /** A dificuldade depende só da fase e das melhorias compradas; nunca de anúncios nem de receita. */
    fun difficulty(stageNumber: Int, p: Progress): Difficulty =
        difficulty(stageNumber, p.vitality, p.power, p.reload, p.upgrade("magazine"))
```
   (Ao colar, mantenha as linhas `playerHealth`, `magazine`, `power` e `reloadSpeed` do `Difficulty(...)`
   atuais - 160-164 - sem mexer; o trecho acima só mostra o que muda.)
2. `Progress.kt`: apagar `adRevenueMicros`/`adRevenueCurrency` (e o comentário) - ver 5.2.
3. `SaveStore.kt`: o conteúdo antigo sai por completo (5.3); as chaves não são mais lidas nem gravadas.
4. `MainActivity.kt`: apagar o bloco `ads.onRevenueEstimated` (153-166) - já feito em 5.4 - e trocar o
   `else` de `launchMission` (linhas 503-508) por `Campaign.difficulty(stage.number, p)`:

```kotlin
        val diff = if (survivalMode) Campaign.survivalDifficulty(p) else Campaign.difficulty(stage.number, p)
```
   Isso também corrige o bug de passagem: a chamada longa atual não repassa `magazineLevel`, então a
   melhoria "Pente estendido" (+6% de balas por nível) não vale na campanha desde `f92ad28`
   (`Campaign.difficulty(stage, p)` repassava `p.upgrade("magazine")`). Efeito colateral aceito: a campanha
   volta a respeitar a melhoria que o jogador comprou. Avisar o dono no resumo final.
5. `RewardedAds.kt`: apagar a propriedade `onRevenueEstimated` (linhas 24-25), o `import ...OnPaidEventListener`
   (linha 10) e o bloco `loaded.onPaidEventListener = ...` (linhas 35-37). `onAdLoaded` fica:

```kotlin
            override fun onAdLoaded(loaded: RewardedAd) {
                loading = false
                ad = loaded
                onStateChanged?.invoke()
            }
```
6. `SettingsScreen.kt`: apagar linhas 63-76 (`val revenueText ... setPadding(0, app.dpi(8), 0, app.dpi(8))`),
   mais os imports `Campaign` (linha 14) e `java.util.Locale` (linha 15) que só serviam a esse trecho.
7. `RulesTest.kt`: apagar `adRevenueDifficultyIsOptInAndBounded` (191-206) e acrescentar os testes de
   9.5. Manter `campaignPlayAdIsRequestedEveryFifthTap`.
8. `README.md` (dono: documentação): apagar a linha 81 (dificuldade adaptativa) e reescrever a nota da
   linha 84 (IDs reais; aparelho do dono cadastrado como dispositivo de teste).

**Decisão sobre o acúmulo ILRD (`onRevenueEstimated`)**: REMOVER. Era a única fonte da dificuldade por
receita; não há outro consumidor, é dado de receita por aparelho sem uso de produto, e guardá-lo no save
é coleta sem finalidade. Alternativa rejeitada: manter só como telemetria local/`Log.d` (custa um
listener e dois campos persistentes sem uso). Se um dia houver telemetria, será um desenho novo, explícito.

Saves antigos: `adRevenueMicros`, `adRevenueCurrency` e `settings.adRevenueDifficulty` são ignorados na
leitura (passo `v4ToV5` e `REMOVED_KEYS`) e desaparecem na próxima gravação. O usuário que já acumulou
tier 3-5 volta à dificuldade-base nas próximas fases.

---

## 7. Anúncios: confirmações e achados do consentimento

### 7.1 Confirmado (nada a mudar)

- IDs reais e `ADMOB_TEST_DEVICES` iguais ao que está em 1.3; sem `buildTypes.debug`; BuildConfig gerado confere.
- Ordem correta: UMP -> `setRequestConfiguration` (test devices + classificação T) -> `MobileAds.initialize`.
- Nenhuma requisição de anúncio antes de `AdConsent.ready`.
- Opções de privacidade acessíveis (Ajustes) quando exigidas.

### 7.2 Observações sobre `AdConsent.kt` (todas opcionais; nenhuma bloqueia a etapa)

| # | Achado | Gravidade | Sugestão |
|---|---|---|---|
| 1 | `RewardedAds.load/InterstitialAds.load` só olham `AdConsent.ready` (que fica `true` para sempre depois do 1º `initialize`). Se o usuário revogar o consentimento em "Privacidade dos anúncios", novos carregamentos continuam na mesma execução. | Baixa | Trocar a condição por `AdConsent.ready && AdConsent.canRequestAds(activity)` nos dois `load()` (o método já existe, hoje sem uso). |
| 2 | `initialize()` pode ser chamado 2x no mesmo arranque (uma imediata, outra nos callbacks). Está protegido por `AtomicBoolean`; só a 1ª `onReady` roda enquanto inicializa, o que é suficiente. | Nenhuma | Não mexer. |
| 3 | Sem `ConsentDebugSettings`: não dá para ensaiar o fluxo EEE no aparelho. | Baixa | Só se o dono quiser testar GDPR; usar o mesmo hash de `ADMOB_TEST_DEVICES` com `DEBUG_GEOGRAPHY_EEA` em código temporário, sem commitar. |
| 4 | Se o consentimento for negado e `canRequestAds()` ficar `false`, recursos atrelados a anúncio (reviver, pular fase, +diamantes) mostram "Anúncio indisponível" para sempre. O jogo continua jogável. | Informativa | Decisão de produto futura (fallback sem anúncio); fora desta etapa. |
| 5 | Qualquer APK em outro aparelho (debug ou release) mostra anúncios reais. | Média (processo) | Procedimento descrito em 2 (cadastrar o hash do novo aparelho). Não mudar IDs. |

### 7.3 Teste-sentinela (ver 9.6)

`AdConfigTest` falha se: algum dos 3 IDs reais ou o hash do aparelho sumir de `app/build.gradle.kts`;
aparecer o prefixo de demonstração do Google `3940256099942544`; ou existir `debug { ... ADMOB ... }`.

---

## 8. Relato: o que o merge não commitado já mudou em anúncios e progressão

(Apenas relato; **nada deve ser removido** por esta etapa - decisão do usuário.)

| Mudança no working tree (vs HEAD `dcc3e30`) | Onde | Efeito |
|---|---|---|
| Intersticial **antes de cada 5º toque manual em JOGAR** | `Rules.CAMPAIGN_PLAY_AD_EVERY=5`, `Rules.registerCampaignPlayTap` (`Progress.kt` 160, 352-357); `Progress.playTapsSinceAd` (persistido em `SaveStore`); `MainActivity.startCampaignPlay/beginStage` + `stageLaunchPending` (87, 465-496); chamado por `HomeScreen.kt:270` e `StagesScreen.kt:74`; KDoc de `InterstitialAds`; teste `campaignPlayAdIsRequestedEveryFifthTap` | O anúncio sai quando há anúncio carregado; se não houver, a fase abre normalmente. Retry/"próxima fase" (`startStage`) não contam toque. |
| Dificuldade por receita: de **opt-in** (HEAD: `Settings.adRevenueDifficulty=false`, chave em Ajustes) para **automática** (parâmetro com default `true`, chave removida, texto "ativa automaticamente") | `Campaign.kt`, `Progress.kt`, `SaveStore.kt`, `SettingsScreen.kt`, `MainActivity.kt` | Oposto do spec. Removida nesta etapa (seção 6). |
| Bônus de combate por anúncio (+30% vida/+20% dano) removido | `boostReady`, botões em `StagesScreen`/`SurvivalScreen`; `Cfg.BOOST_*` fixos em 1f | Alinhado ao spec ("sem compra de poder"). Os campos `Cfg.BOOST_*` do protocolo nativo continuam existindo. |
| `StoryOverlay` e `startStage(n, skipStory)` removidos | `MainActivity`, `Overlays.kt` | Alinhado a "tutorial sem tela de texto longa". |
| Marcadores de conflito e `Campaign.stage()` duplicada resolvidos | `MainActivity.kt`, `Campaign.kt` | HEAD não compila; working tree compila. |

### 8.1 O intersticial do 5º toque viola "não ampliar anúncios como mecanismo de retenção"?

- **Literalmente, não**: o texto do spec trata de calendários, baús, moedas e anúncios usados para
  *trazer o jogador de volta*; o intersticial do toque não recompensa nem convida a voltar.
- **Pela intenção do spec, conflita**: a "Intenção" diz que a retenção deve vir de domínio e histórias
  "e não por interrupções, moedas demais ou pressão de anúncios". O anúncio do 5º toque (a) é uma
  **nova colocação** (amplia a exposição), (b) interrompe o botão principal (JOGAR) e (c) **ignora**
  `INTERSTITIAL_GAP_MS` (150 s) e `wins+losses >= 3`: pode vir logo depois de outro intersticial ou de
  um premiado (os dois gravam `lastAdAt`, mas o do toque não o consulta).
- **Impacto estimado** (simulação simples do fluxo, sem o intervalo de 150 s, todas as partidas iniciadas
  por JOGAR e encerradas pelo caminho que chama `maybeInterstitial`): 9 intersticiais em 20 partidas sem o
  recurso contra 11 com ele (cerca de +20%); o toque zera `missionsSinceInterstitial`, o que compensa
  parte do aumento. Para quem usa "tentar de novo"/"próxima fase" quase não conta toque. É uma estimativa
  por leitura do código, **não** foi medido no aparelho.
- **Opções para o usuário** (decidir depois; não implementar agora): (A) manter como está;
  (B) fazer o anúncio do toque passar por `Rules.shouldShowInterstitial` (respeita intervalo e mínimo de
  partidas); (C) remover (`startCampaignPlay` -> `startStage`, apagar `playTapsSinceAd`,
  `registerCampaignPlayTap`, o teste e a chave do save - o codec novo já ignora a chave se ela sumir).
- Esta etapa **preserva** o recurso: o `SaveCodec` continua lendo/gravando `playTapsSinceAd` (com
  `coerceIn(0, CAMPAIGN_PLAY_AD_EVERY - 1)`) e o teste existente continua valendo.

---

## 9. Testes

Pasta: `app/src/test/java/com/fabiosilva/ecosdeeldra/save/` (`package com.fabiosilva.ecosdeeldra.save`).
Todos rodam na JVM, sem Android. Rodar com
`.\gradlew.bat :app:testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.save.*" --no-daemon --max-workers=1`.

### 9.1 `JsonRuntimeSanityTest` (sentinela da dependência)

```kotlin
class JsonRuntimeSanityTest {
    @Test
    fun orgJsonIsTheRealImplementationNotTheAndroidStub() {
        val o = JSONObject("""{"a":1,"b":[true,null,"x"]}""")
        assertEquals(1, o.getInt("a"))
        assertEquals(3, o.getJSONArray("b").length())
        assertTrue(o.getJSONArray("b").isNull(1))
    }
}
```
Se falhar com `Method ... not mocked`, a dependência `org.json:json` não está no classpath de testes.

### 9.2 `SaveFixtures.kt` (utilitários compartilhados)

```kotlin
object SaveFixtures {
    /** Save real no formato v4 (como o app antigo gravava), incluindo os campos de receita que serão ignorados. */
    val V4_JSON: String = """
    {"version":4,"diamonds":1234,"ownedSkins":[0,2,3],"selectedHero":1,
     "heroes":[{"name":"Ana","look":0},{"name":"Bia","look":12345}],
     "stages":{"1":{"completed":true,"stars":3,"bestTime":120.5,"crystalMask":7,"fails":1,"skipped":false},
               "2":{"completed":true,"stars":1,"bestTime":300.0,"crystalMask":3,"fails":0,"skipped":true}},
     "vitality":3,"power":2,"reload":1,"upgrades":{"vitality":3,"power":2,"reload":1,"magazine":4,"inexistente":9},
     "inventory":"1:1,2:1,26:3,28:2,21:2,20:1,${List(18) { "0:0" }.joinToString(",")}|0|-1",
     "survival":[{"name":"Ana","seconds":300,"wave":5,"kills":80,"score":900,"date":"2026-10-01"}],
     "supplyAdsDay":"2026-10-17","supplyAdsToday":2,
     "stats":{"kills":500,"headshots":40,"wins":9,"crystals":20,"playSeconds":7200,"adsWatched":6,"loginDays":4,"bestCombo":7,
              "pvpKills":1,"pvpWins":0,"pvpMatches":1,"chests":3,"nests":9,"distance":15000,"losses":4,"survivalRuns":2,
              "bestWave":5,"bestSurvivalSeconds":300},
     "lastLoginDay":"2026-10-17","calendarClaims":3,"lastCalendarClaim":"2026-10-17","dailyDay":"2026-10-17",
     "daily":[{"id":"kill","progress":10,"claimed":false},{"id":"win","progress":2,"claimed":true},{"id":"nest","progress":0,"claimed":false}],
     "dailyBonusClaimed":false,"adsDay":"2026-10-17","adsToday":1,"adRevenueMicros":9000000,"adRevenueCurrency":"USD",
     "chestReadyAt":1760000000000,"tutorialSeen":true,"playTapsSinceAd":3,"missionsSinceInterstitial":1,"lastAdAt":1760000000000,
     "settings":{"sensitivity":1.4,"musicVolume":0.5,"sfxVolume":1.0,"aimAssist":true,"autoFire":false,"invertY":false,
                 "vibration":true,"quality":2,"reminders":true,"adRevenueDifficulty":true,"showFps":true,"leftHanded":true,
                 "playerName":"Ana","cinematic":false,"hudLayout":"","hudOpacity":0.7}}
    """.trimIndent()

    /** Ordena chaves recursivamente: compara JSON sem depender da ordem do HashMap do org.json real. */
    fun canon(v: Any?): String = when (v) {
        is JSONObject -> v.keys().asSequence().toList().sorted().joinToString(",", "{", "}") { k -> "\"$k\":${canon(v.get(k))}" }
        is JSONArray -> (0 until v.length()).joinToString(",", "[", "]") { canon(v.get(it)) }
        else -> v.toString()
    }

    fun json(diamonds: Int): String = SaveCodec.encode(Progress().apply { this.diamonds = diamonds }, "teste")
}
```

### 9.3 `SaveCodecTest` (nomes e o que afirmam)

- `fullProgressRoundTrips`: monta um `Progress` com TODOS os campos preenchidos (heróis, 3 fases, upgrades,
  inventário, ranking, stats, diárias, settings, `saveExtras`), faz `encode -> decode -> encode` e compara
  `canon(JSONObject(a)) == canon(JSONObject(b))`. Também `assertEquals(SaveCodec.SCHEMA_VERSION, json.getInt("version"))`.
- `legacyV4FixtureDecodesWithEveryValue`: `decode(V4_JSON)` é `Ok` com `schemaFound == 4`; confere diamantes 1234,
  `selectedHero == 1`, nome do herói "Bia", fase 2 `skipped`, `upgrade("magazine") == 4` e que a chave
  `"inexistente"` não entrou, `stats.adsWatched == 6`, `playTapsSinceAd == 3`, `settings.quality == 2`,
  `leftHanded`, inventário com 3 granadas.
- `adRevenueFieldsAreIgnoredAndNeverWrittenAgain`: após decodificar V4, `Progress::class.java.declaredFields` não tem
  campo com "revenue"; o `encode` não contém `adRevenue` (nem `adRevenueDifficulty`); `saveExtras` está vazio.
- `unknownTopLevelKeysSurviveARoundTrip`: JSON com `"futuro":{"a":[1,2]}` e `"texto":"123"`; após `decode -> encode`
  as duas chaves voltam com o mesmo tipo (string continua string).
- `knownKeysCoverEveryEncodedKey`: todas as chaves de topo de `encode(Progress())` estão em `KNOWN_KEYS`.
- `missingFieldsGetDefaults`: `{"version":4}` decodifica para o default (`Progress()`), sem exceção.
- `emptyObjectOrForeignJsonIsInvalid`: `"{}"`, `"[]"`, `"null"`, `""`, `"{\"foo\":1}"` -> `Invalid`.
- `truncatedJsonIsInvalid`: `V4_JSON.dropLast(40)` -> `Invalid`.
- `bomAndWhitespaceAreTolerated`: `"﻿  " + V4_JSON + "\n"` -> `Ok`.
- `wrongContainerTypeIsInvalidNotReset`: `"stages":[]`, `"heroes":{}`, `"stats":"x"`, `"inventory":{}` -> cada um `Invalid`.
- `wrongScalarInCriticalFieldIsInvalid`: `"diamonds":"muitos"` e `"stages":{"1":{"stars":"x"}}` -> `Invalid`.
- `droppedLightEntriesProduceWarnings`: `"survival":[1,{"name":"A"}]` -> `Ok` com `warnings` contendo "ranking de sobrevivência";
  `"daily":[null]` -> warning "missões diárias" e `dailyDay == ""`.
- `nonFiniteFloatsNeverBreakEncode`: `bestTime = Float.NaN` e `settings.sensitivity = Float.POSITIVE_INFINITY`
  não lançam em `encode`; o JSON decodifica com 0 / valor padrão.
- `newerSchemaIsReadLeniently`: `{"version":99, ...V4...}` -> `Ok`, `isNewerThanApp == true`, `schemaFound == 99`.
- `negativeCountersAreClamped`: `"diamonds":-5`, `"stats":{"kills":-1}` -> 0.
- `campaignPlayTapCounterIsClamped`: `playTapsSinceAd: 99` -> `CAMPAIGN_PLAY_AD_EVERY - 1`.

### 9.4 `SaveFilesTest` e `SaveRepositoryTest` (`@get:Rule val tmp = TemporaryFolder()`)

`SaveFilesTest`:
- `writeTmpThenPromoteReplacesPrimaryAndLeavesNoTmp`
- `copyAtomicKeepsModifiedTimeAndLeavesNoPart`
- `quarantineIsDedupedByContentAndCappedAtFive`
- `readReportsMissingTooBigAndIoErrorSeparately` (arquivo 5 MB -> `TooBig`; diretório no lugar do arquivo -> `Failed`)

`SaveRepositoryTest` (helpers: `now` mutável como relógio; `repo(legacy)`; `write(name, text)`; `read(name)`):

```kotlin
    @Test
    fun invalidJsonNeverReplacesProgressSilently() {                    // O teste central do spec
        write("save.json", "{corrompido")
        write("save.json.bak1", "lixo")
        write("save.json.bak2", "")
        val before = dir().listFiles()!!.associate { it.name to it.readText() }

        val r = repo().apply { }.let { rp -> rp.load().also { assertTrue(rp.recoveryPending) }.also { _ ->
            assertEquals(PersistResult.Blocked, rp.persist(json(1)))    // "jogo novo" automático NÃO grava
        } }

        assertTrue(r is LoadResult.NeedsRecovery)
        assertEquals(before, dir().listFiles()!!.associate { it.name to it.readText() })   // disco idêntico
    }
```
(Simplifique a forma ao escrever; o contrato é: `NeedsRecovery`, `recoveryPending`, `Blocked` e **bytes de todos os
arquivos inalterados**, sem arquivos novos.)

Demais casos de `SaveRepositoryTest`:
- `freshInstallLoadsFreshThenFirstPersistCreatesPrimary`
- `persistThenLoadRoundTrips`
- `validPrimaryWinsEvenIfBackupsAreGarbage` (backups nem são lidos)
- `corruptPrimaryFallsBackToNewestValidBackupAndKeepsTheCorruptFile`: primário `"{corrupt"`, bak1 = json(100), bak2 = json(50);
  `load` -> `Loaded(BACKUP_1)` com `RestoredFromBackup`; o primário ainda é `"{corrupt"`; depois de `persist(json(101))`
  existe `save.corrupt-*.json` com o texto ruim, bak1 continua json(100) (sem rotação porque o primário não era válido).
- `missingPrimaryWithValidTmpUsesTmpWithoutNotice` (`INTERRUPTED_WRITE`, `notices` vazio)
- `primaryInvalidAndTmpValidStillWarns`
- `allCorruptThenStartNewGameKeepsEveryBadFile`: após `startNewGame`, `recoveryPending == false`, primário válido (diamantes 0),
  cópias `save.corrupt-*.json` com os textos ruins; um `load` novo devolve `Loaded(PRIMARY)`.
- `ioErrorIsNotTreatedAsCorruption`: `save.json` é um **diretório** -> `NeedsRecovery` com `readErrors == true`;
  `startNewGame` renomeia o diretório para `save.unreadable-*` (continua existindo) e cria o primário.
- `rotationRunsOnFirstSaveOfSessionThenEveryFifteenMinutes`: primário A válido; `load`; `persist(B)` -> bak1 == A;
  `persist(C)` logo em seguida -> bak1 ainda A; `now += 15 min`; `persist(D)` -> bak2 == A e bak1 == C.
- `identicalPrimaryDoesNotRotate` (duas gravações do mesmo texto não mudam bak1/bak2)
- `invalidBak1IsQuarantinedAndDoesNotPushValidBak2Out`
- `persistRefusesTextTheAppCannotReadBack` (`persist("{}")` -> `Failed`; primário inalterado)
- `legacyPrefsAreMigratedOnceAndKeptVerbatim`: `legacy = V4_JSON`; `load` -> `Loaded(LEGACY_PREFS)`; existem `save.json` e
  `save.legacy-prefs.json` (== V4_JSON); novo repo com `legacy` que lança se for chamado -> `Loaded(PRIMARY)`.
- `corruptLegacyPrefsNeedRecoveryAndKeepRawCopy`
- `schemaOlderThanCurrentKeepsPreMigrationSnapshotOnce` (`save.pre-v4.json`, não sobrescrito numa 2ª carga)
- `restoreIsReversible`: A no primário, B em bak1; `restore(B)` -> primário B e bak1 == A.
- `partialSectionsKeepOriginalAndNotifyOnce` (`SectionsReset` + `save.partial-*.json`)
- `newerSchemaKeepsVerbatimCopy` (`save.newer-v99.json`, aviso `NewerSchema`)

`SaveMessagesTest`: `describe` devolve título e texto não vazios para cada subtipo de `SaveNotice`.

### 9.5 Em `RulesTest.kt` (substituem o teste antigo)

```kotlin
    @Test
    fun difficultyIgnoresAnyAdState() {
        val clean = Progress()
        val noisy = Progress().apply {
            stats.adsWatched = 5_000; adsDay = "2026-10-18"; adsToday = 5; supplyAdsToday = 6
            lastAdAt = 123L; playTapsSinceAd = 4; missionsSinceInterstitial = 9
        }
        for (stage in listOf(1, 5, 20, 137)) {
            assertEquals(Campaign.difficulty(stage, clean), Campaign.difficulty(stage, noisy))
        }
        assertEquals(Campaign.survivalDifficulty(clean), Campaign.survivalDifficulty(noisy))
    }

    @Test
    fun difficultyBaselineIsPinned() {   // fórmulas sem multiplicador de anúncio (fase 5, sem melhorias)
        val d = Campaign.difficulty(5, 0, 0, 0)
        assertEquals(10, d.zombieCount)
        assertEquals(108f, d.zombieHealth, 0.001f)
        assertEquals(10.4f, d.zombieDamage, 0.001f)
        assertEquals(1.088f, d.zombieSpeed, 0.001f)
    }

    @Test
    fun progressCampaignAndSettingsCarryNoAdRevenueState() {
        val names = (Progress::class.java.declaredFields + Settings::class.java.declaredFields).map { it.name.lowercase() } +
            Campaign::class.java.declaredMethods.map { it.name.lowercase() } + Campaign::class.java.declaredFields.map { it.name.lowercase() }
        assertTrue(names.none { "revenue" in it || "ilrd" in it })
    }

    @Test
    fun magazineUpgradeReachesTheCampaignDifficulty() {
        val p = Progress().also { it.upgrades["magazine"] = 5 }
        assertTrue(Campaign.difficulty(3, p).magazine > Campaign.difficulty(3, Progress()).magazine)
    }
```
(import `com.fabiosilva.ecosdeeldra.data.Settings`.)

### 9.6 `AdConfigTest` (sentinela de anúncios)

```kotlin
class AdConfigTest {
    private val gradle: String by lazy {
        listOf(File("build.gradle.kts"), File("app/build.gradle.kts")).first { it.exists() }.readText()
    }

    @Test fun realAdMobIdsAndTestDeviceStay() {
        assertTrue(gradle.contains("ca-app-pub-6598765502914364~3458194932"))
        assertTrue(gradle.contains("ca-app-pub-6598765502914364/5788844807"))
        assertTrue(gradle.contains("ca-app-pub-6598765502914364/6741641853"))
        assertTrue(gradle.contains("E812F36934272DE8BFD5D96B7E64BE83"))
    }

    @Test fun googleDemoIdsNeverAppear() = assertFalse(gradle.contains("3940256099942544"))

    @Test fun adIdsAreNotOverriddenPerBuildType() =
        assertFalse(Regex("debug\\s*\\{[^}]*ADMOB", RegexOption.DOT_MATCHES_ALL).containsMatchIn(gradle))
}
```

---

## 10. Passos de implementação (cada um deixa o projeto compilando)

0. **Pré-requisito**: working tree preservado (ver seção 0). Ler `git status`; não reverter nada.
1. **Infra de testes**: `app/build.gradle.kts` (5.1) + `JsonRuntimeSanityTest` + `AdConfigTest`.
   Rodar só esses testes. Se `JsonRuntimeSanityTest` falhar com "not mocked", corrigir o classpath antes de seguir.
2. **Limpar a dificuldade por receita** (seção 6 itens 1, 2, 4, 5, 6, 7) **e**, no `SaveStore.kt` ainda antigo,
   apagar só as 4 linhas de receita (encode 80-81, decode 189-190) para o projeto continuar compilando.
   Rodar `testDebugUnitTest`.
3. **Pacote `data.save`**: `SaveModels`, `SaveCodec`, `SaveMigrations`, `SaveFiles`, `SaveRepository`,
   `SaveMessages` (seção 4), `Progress.saveExtras` (5.2) e todos os testes das seções 9.2-9.4. O app ainda usa o
   `SaveStore` antigo; rodar `--tests "...save.*"` até ficar verde.
4. **Fachada + app**: reescrever `SaveStore.kt` (5.3), `MainActivity` (5.4), `RecoveryScreen` (5.5).
   Compilar com `:app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a --no-daemon --max-workers=1`
   e rodar a suíte inteira (`:app:testDebugUnitTest`).
5. **Entrega para a documentação** (outro agente/área): `README.md` linhas 81 e 84, registro do desvio da seção 2 no
   spec, e (opcional) menção de que o save agora fica em `filesDir/save`.
6. **Verificação no aparelho** (seção 11). Antes do fechamento da etapa, UMA compilação completa sem
   `-Pandroid.injected.build.abi` para confirmar arm64-v8a e armeabi-v7a (critério do spec), sem outro Gradle em paralelo.
7. **Opcional (marcar como feito só se sobrar tempo)**: seção "SAVE" em `SettingsScreen` (status + botão
   "RESTAURAR CÓPIA" abrindo um `Popup` por item de `store.restorable()`). Mexe num arquivo quente; fazer por último e
   em commit separado. Não é critério de aceitação.

Comandos úteis (PowerShell, na raiz do projeto, um por vez):

```powershell
.\gradlew.bat :app:testDebugUnitTest --tests "com.fabiosilva.ecosdeeldra.save.*" --no-daemon --max-workers=1
.\gradlew.bat :app:testDebugUnitTest :app:assembleDebug -Pandroid.injected.build.abi=arm64-v8a --no-daemon --max-workers=1
adb install -r -t app/build/outputs/apk/debug/app-debug.apk
```

---

## 11. Roteiro de verificação no aparelho (APK debug, `run-as` funciona porque é debuggable)

Em Windows, redirecionamento de saída binária do PowerShell recodifica; use `cmd /c` para copiar:
`cmd /c "adb exec-out run-as com.fabiosilva.ecosdeeldra cat files/save/save.json > save.json"`.

1. **Migração**: com a build antiga instalada e progresso existente (anote diamantes e fases), instalar a nova por cima
   (`adb install -r`). Esperado: mesmo progresso; `adb shell run-as com.fabiosilva.ecosdeeldra ls -la files/save`
   mostra `save.json`, `save.legacy-prefs.json`, `save.pre-v4.json` (e `save.json.bak1` a partir da 2ª abertura).
2. **Primário corrompido com backup**: fechar o app (`adb shell am force-stop com.fabiosilva.ecosdeeldra`), então
   `adb shell "run-as com.fabiosilva.ecosdeeldra sh -c 'echo {ruim > files/save/save.json'"`. Abrir: Home normal +
   popup "Save recuperado"; `ls files/save` tem `save.corrupt-*.json` e, depois de jogar um pouco, `save.json` novo.
3. **Tudo corrompido**: repetir o `echo` para `save.json`, `save.json.bak1` e `save.json.bak2` (e `save.json.tmp` se existir).
   Abrir: aparece a `RecoveryScreen`; **Voltar** fecha o app e os três arquivos continuam com `{ruim`.
   Reabrir -> **COMEÇAR JOGO NOVO** -> confirmar: Home com progresso zerado; `save.corrupt-*.json` guardado.
4. **Restaurar**: nova rodada de (3), mas antes copiar `save.pre-v4.json`; na `RecoveryScreen` o botão
   "RESTAURAR: ANTES DA ATUALIZAÇÃO DO FORMATO (V4)" deve aparecer e devolver o progresso antigo.
5. **Sem regressão de jogo**: jogar uma fase inteira, voltar à Home, matar o app pelo recentes, reabrir: progresso mantido.
6. **Dificuldade**: iniciar fase com a melhoria "Pente estendido" comprada: o pente reflete o bônus (bug corrigido);
   Ajustes não mostra mais texto de receita/ILRD.
7. **Anúncios**: no aparelho do dono, intersticial e premiado continuam exibindo anúncios de teste; botão de
   privacidade (se exigido) continua nos Ajustes.

---

## 12. Critérios de aceitação

- [ ] Um `save.json` com JSON inválido **nunca** vira `Progress()` automaticamente: ou abre um backup válido (com aviso
      e o arquivo ruim preservado em `save.corrupt-*.json`), ou mostra a `RecoveryScreen` com o disco intocado.
- [ ] Enquanto a recuperação está pendente, `SaveStore.save/saveNow` não escrevem nada (teste e verificação manual 3).
- [ ] Gravação atômica (tmp + fsync + rename) e backups `bak1/bak2` com a política de rotação da seção 3.4.
- [ ] Saves v4 existentes (incluindo o do aparelho do dono) migram sem perda; o SharedPreferences antigo fica intacto;
      `save.legacy-prefs.json` e `save.pre-v4.json` existem.
- [ ] `Campaign`, `Progress`, `SaveCodec`, `MainActivity`, `RewardedAds` e `SettingsScreen` não têm mais nenhum símbolo de
      receita de anúncios; `RulesTest.difficultyIgnoresAnyAdState` e `progressCampaignAndSettingsCarryNoAdRevenueState` passam.
- [ ] `app/build.gradle.kts` mantém, byte a byte, as linhas 18-23 (IDs reais e `ADMOB_TEST_DEVICES`); `AdConfigTest` passa.
- [ ] A lógica de save roda em testes JVM com `org.json` real (`JsonRuntimeSanityTest` verde).
- [ ] Suíte completa verde; `:app:assembleDebug` arm64-v8a OK; compilação final dos dois ABIs OK.
- [ ] Nada de `android.*` em `data/save/*` (exceto `SaveStore.kt`, que fica em `data/`).
- [ ] `campaignPlayAdIsRequestedEveryFifthTap` e o comportamento do 5º toque inalterados.

---

## 13. Riscos e coordenação

| Risco | Mitigação |
|---|---|
| **Arquivos quentes** (`MainActivity.kt`, `Progress.kt`, `Campaign.kt`, `SettingsScreen.kt`, `RulesTest.kt`, `app/build.gradle.kts`) editados por outras áreas | Esta área deve entrar **primeiro** na etapa 2 (muda a assinatura de `Campaign.difficulty` e remove campos). A área de contratos (etapa 4) deve estender `Campaign.difficulty(stage, p)` já limpa; ninguém deve referenciar `adRevenue*`/`estimatedAdRevenueMicros`. |
| Dependência `org.json` não resolver offline | Usar `20260814` (jar+pom em cache) ou ligar a rede uma vez. O teste-sentinela dá o diagnóstico em segundos. |
| Ordem do classpath faz o stub do `android.jar` vencer | O AGP põe o jar mockável por último; o sentinela detecta. Não usar `isReturnDefaultValues`. |
| `renameTo` não-atômico no Windows | Só afeta testes JVM (desvio guardado por `os.name`); no Android nunca apaga o destino. |
| Usuário instala build antiga depois de migrar | Build antiga lê o prefs defasado. Avisar o dono; o prefs não é apagado de propósito. |
| Backups guardam estado "válido porém errado" (bug lógico) | Limite conhecido. Mitigações: autoverificação antes de gravar, snapshot `pre-vN` por migração, quarentena, `writtenBy` nos arquivos para diagnóstico. |
| `saveNow` bloqueia a UI até 2 s no pior caso | Só em `onPause`; limite de 2 s; gravação segue na thread de E/S. |
| Tela de recuperação em paisagem estreita (640x360 dp) | `ScrollView`; botões 46 dp. Verificar visualmente no aparelho. |
| `applySettings()` em `onRecoveryResolved` reaplica HUD/áudio antes da Home | Ele já é seguro fora de partida (usado nos Ajustes). Conferir que não dispara `NativeBridge` antes de `NativeBridge.init` (init ocorre antes em `onCreate`). |
| Correção do "Pente estendido" altera o equilíbrio da campanha (volta ao comportamento pré-`f92ad28`) | Intencional; registrar no resumo final ao dono. |
| Spec e README ficam contraditórios (IDs de teste, dificuldade adaptativa) | Entrega ao dono de documentação (passo 5). |
| Intersticial do 5º toque pode irritar/limitar receita ou conflitar com política do AdMob (anúncios seguidos) | Apenas relatado (seção 8); decisão do usuário. |

## 14. Fora de escopo (rejeitado de propósito)

- Cloud save, backup do Android (`allowBackup` continua `false`), exportação automática, criptografia do save.
- Checksum/assinatura no JSON (truncamento já falha no parse; adulteração local não é ameaça do produto).
- Novos campos de contrato/expedição/tutorial (outras áreas, seguindo o protocolo 3.7).
- Trocar/parametrizar IDs de anúncio, flavors de debug, `local.properties` para IDs.
- Remover ou alterar o intersticial do 5º toque.
- Apagar o SharedPreferences antigo (só numa versão futura).
