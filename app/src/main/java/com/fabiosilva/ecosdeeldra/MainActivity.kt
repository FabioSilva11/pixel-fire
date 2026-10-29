package com.fabiosilva.ecosdeeldra

import android.graphics.Bitmap
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import android.widget.FrameLayout
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import com.fabiosilva.ecosdeeldra.audio.GameAudio
import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.Items
import com.fabiosilva.ecosdeeldra.data.MatchRules
import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.SaveStore
import com.fabiosilva.ecosdeeldra.data.ZombieKinds
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport
import com.fabiosilva.ecosdeeldra.data.save.SaveMessages
import com.fabiosilva.ecosdeeldra.data.save.SaveNotice
import com.fabiosilva.ecosdeeldra.engine.Cfg
import com.fabiosilva.ecosdeeldra.engine.Ev
import com.fabiosilva.ecosdeeldra.engine.GameSurfaceView
import com.fabiosilva.ecosdeeldra.engine.Hud
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import com.fabiosilva.ecosdeeldra.engine.PerfRun
import com.fabiosilva.ecosdeeldra.engine.PoiKind
import com.fabiosilva.ecosdeeldra.engine.Res
import com.fabiosilva.ecosdeeldra.objective.Objective
import com.fabiosilva.ecosdeeldra.objective.ObjectiveText
import com.fabiosilva.ecosdeeldra.ui.CombatFeed
import com.fabiosilva.ecosdeeldra.ui.DefeatOverlay
import com.fabiosilva.ecosdeeldra.ui.MatchResultOverlay
import com.fabiosilva.ecosdeeldra.ui.HomeScreen
import com.fabiosilva.ecosdeeldra.ui.HudEditorScreen
import com.fabiosilva.ecosdeeldra.ui.HudView
import com.fabiosilva.ecosdeeldra.ui.InventoryScreen
import com.fabiosilva.ecosdeeldra.ui.LoadingOverlay
import com.fabiosilva.ecosdeeldra.ui.MapOverlay
import com.fabiosilva.ecosdeeldra.ui.Palette
import com.fabiosilva.ecosdeeldra.ui.PauseOverlay
import com.fabiosilva.ecosdeeldra.ui.Popup
import com.fabiosilva.ecosdeeldra.ui.RecoveryScreen
import com.fabiosilva.ecosdeeldra.ui.SettingsScreen
import com.fabiosilva.ecosdeeldra.ui.Ui

class MainActivity : ComponentActivity(), HudView.Listener {
    lateinit var progress: Progress
        private set
    lateinit var audio: GameAudio
        private set
    lateinit var store: SaveStore
        private set
    private lateinit var surface: GameSurfaceView
    private lateinit var hud: HudView
    private lateinit var uiLayer: FrameLayout
    private lateinit var overlayLayer: FrameLayout

    private var inMission = false
    private var missionStage = 1
    private var missionPaused = false
    private var missionEnded = false
    private var defeatShown = false
    private var bestCombo = 0
    private var missionKills = 0
    private var loading: LoadingOverlay? = null
    private var currentScreen: View? = null
    private var homeScreen: HomeScreen? = null
    private var bigMap: Bitmap? = null
    private var worldSize = 0f

    private var eventNests = 0
    private var eventCrystals = 0
    private var inventoryOpen = false


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
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

        NativeBridge.init(assets)
        val root = FrameLayout(this).apply { setBackgroundColor(Palette.bg) }
        surface = GameSurfaceView(this) { events, n -> onEngineEvents(events, n) }
        surface.setQuality(progress.settings.quality)
        hud = HudView(this, this).apply { visibility = View.GONE }
        uiLayer = FrameLayout(this)
        overlayLayer = FrameLayout(this)
        root.addView(surface, FrameLayout.LayoutParams(-1, -1))
        root.addView(hud, FrameLayout.LayoutParams(-1, -1))
        root.addView(uiLayer, FrameLayout.LayoutParams(-1, -1))
        root.addView(overlayLayer, FrameLayout.LayoutParams(-1, -1))
        setContentView(root)
        enterImmersive()
        val s = progress.settings
        engine { NativeBridge.setGraphics(s.quality) }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() = onBack()
        })

        if (loaded is LoadResult.NeedsRecovery) {
            showRecovery(loaded.report)
            return
        }
        startSession((loaded as? LoadResult.Loaded)?.notices.orEmpty())
        // Diagnóstico de desempenho: extras de Intent só existem/valem em build de depuração.
        if (BuildConfig.DEBUG) {
            perfRun = PerfRun.parse { key, def -> intent.getIntExtra(key, def) }
            perfRun?.let { run -> surface.postDelayed({ startPerfRun(run) }, 1500) }
        }
    }

    private var perfRun: PerfRun? = null

    private fun startPerfRun(run: PerfRun) {
        if (run.quality >= 0) surface.setQuality(run.quality)  // a escala de resolução vive no Kotlin
        if (run.mode == PerfRun.MODE_BENCH) {
            perfRun = null
            engine { NativeBridge.perfBench(run.seed, run.stage, 8, run.meters, run.policy, run.flags) }
            return
        }
        missionStage = run.stage
        launchMission()
    }

    /** Tudo que o arranque fazia depois de carregar o save; também roda ao sair da tela de recuperação. */
    private fun startSession(notices: List<SaveNotice>) {
        showHome()
        showSaveNotices(notices)
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

    // ------------------------------------------------------------------ ciclo de vida

    override fun onResume() {
        super.onResume()
        enterImmersive()
        surface.onResume()
        surface.start()
        audio.resume()
        // Virou o dia com o app em segundo plano.
    }

    override fun onPause() {
        // Durante derrota/carregamento a missão já está parada; não sobrepor a pausa.
        if (inMission && !missionPaused && !missionEnded && !defeatShown && loading == null) pauseMission()
        surface.stop()
        surface.onPause()
        audio.pause()
        store.saveNow(progress)
        super.onPause()
    }

    override fun onDestroy() {
        audio.release()
        super.onDestroy()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enterImmersive()
    }

    private fun enterImmersive() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let {
                it.hide(WindowInsets.Type.systemBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        }
    }

    private fun onBack() {
        when {
            store.recoveryPending -> finish()
            loading != null || defeatShown -> Unit
            // close() fecha primeiro o diálogo de descarte, se estiver aberto; senão volta ao jogo.
            inventoryOpen && inMission -> (overlayLayer.getChildAt(0) as? InventoryScreen)?.close() ?: Unit
            overlayLayer.childCount > 0 && !inMission -> closeOverlay()
            inMission && !missionEnded -> if (missionPaused) resumeMission() else pauseMission()
            inMission && missionEnded -> abandonMission(retry = false)
            currentScreen is InventoryScreen -> (currentScreen as InventoryScreen).close()
            currentScreen !is HomeScreen -> showHome()
            else -> finish()
        }
    }

    // ------------------------------------------------------------------ utilidades


    fun save() {
        store.save(progress)
    }

    fun engine(block: () -> Unit) = surface.engine(block)

    fun showOverlay(view: View) {
        overlayLayer.removeAllViews()
        overlayLayer.addView(view, FrameLayout.LayoutParams(-1, -1))
    }

    fun closeOverlay() {
        overlayLayer.removeAllViews()
    }

    private fun showScreen(view: View) {
        uiLayer.removeAllViews()
        uiLayer.addView(view, FrameLayout.LayoutParams(-1, -1))
        view.alpha = 0f
        view.animate().alpha(1f).setDuration(180).start()
        currentScreen = view
        homeScreen = view as? HomeScreen
    }

    // ------------------------------------------------------------------ telas

    fun showHome() {
        closeOverlay()
        hud.visibility = View.GONE
        audio.setMusic(GameAudio.Music.MENU)
        val look = progress.lookCode
        engine { NativeBridge.enterLobby(look, 0) }
        showScreen(HomeScreen(this))
    }

    /** Equipamento e mochila fora da partida: tela própria com o herói em 3D. */
    fun showInventory(back: () -> Unit = { showHome() }) {
        closeOverlay()
        hud.visibility = View.GONE
        val look = progress.lookCode
        engine { NativeBridge.enterLobby(look, 0) }
        showScreen(InventoryScreen(this, progress.inventory, inMatch = false) { back() })
    }
    fun showSettings() = showScreen(SettingsScreen(this, fromPause = false))

    fun applySettings() {
        val s = progress.settings
        audio.sfxVolume = s.sfxVolume
        audio.musicVolume = s.musicVolume
        surface.setQuality(s.quality)
        hud.applySettings(s.leftHanded, s.vibration, s.showFps, s.hudLayout, s.hudOpacity)
        engine {
            NativeBridge.setSettings(s.sensitivity, s.aimAssist, s.invertY)
            NativeBridge.setGraphics(s.quality)
        }
        save()
    }

    // ------------------------------------------------------------------ missão

    fun startGame() {
        if (inMission || loading != null) return
        missionStage = 1
        launchMission()
    }

    private fun launchMission() {
        val p = progress
        val diff = MatchRules.difficulty()
        val config = FloatArray(Cfg.COUNT)
        config[Cfg.STAGE] = missionStage.toFloat()
        config[Cfg.SKIN] = 0f  // a aparência vai à parte (32 bits não cabem num float)
        val look = p.lookCode
        config[Cfg.ZOMBIES] = diff.zombieCount.toFloat()
        config[Cfg.ZOMBIE_HP] = diff.zombieHealth
        config[Cfg.ZOMBIE_DMG] = diff.zombieDamage
        config[Cfg.ZOMBIE_SPEED] = diff.zombieSpeed
        config[Cfg.PLAYER_HP] = diff.playerHealth
        config[Cfg.MAGAZINE] = diff.magazine.toFloat()
        config[Cfg.POWER] = diff.power
        config[Cfg.CRYSTAL_MASK] = 0f
        config[Cfg.CRYSTAL_TOTAL] = MatchRules.NEST_COUNT.toFloat()
        config[Cfg.SENSITIVITY] = p.settings.sensitivity
        config[Cfg.AIM_ASSIST] = if (p.settings.aimAssist) 1f else 0f
        config[Cfg.INVERT_Y] = if (p.settings.invertY) 1f else 0f
        // Mantém a ilha procedural, com semente nova a cada partida.
        config[Cfg.SEED] = kotlin.random.Random.nextInt(1, 16_000_000).toFloat()
        config[Cfg.RELOAD_SPEED] = diff.reloadSpeed
        config[Cfg.BOOST_HEALTH] = 1f
        config[Cfg.BOOST_DAMAGE] = 1f
        config[Cfg.QUALITY] = p.settings.quality.toFloat()
        config[Cfg.MODE] = Cfg.MODE_CAMPAIGN.toFloat()
        config[Cfg.SPEED] = 1f
        config[Cfg.HEADSHOT] = 1f
        config[Cfg.LUCK] = 1f
        perfRun?.applyTo(config)  // diagnóstico (só debug): semente fixa e autopiloto
        perfRun = null            // vale só para esta partida
        p.inventory.normalize()
        val inventory = p.inventory.toIntArray()
        inMission = true
        missionPaused = false
        missionEnded = false
        defeatShown = false
        bestCombo = 0
        missionKills = 0
        eventNests = 0
        eventCrystals = 0
        bigMap = null
        uiLayer.removeAllViews()
        currentScreen = null
        homeScreen = null
        loading = LoadingOverlay(this).also { showOverlay(it) }
        hud.reset()
        hud.playerName = p.settings.playerName.ifBlank { "Jogador" }
        hud.applySettings(p.settings.leftHanded, p.settings.vibration, p.settings.showFps, p.settings.hudLayout, p.settings.hudOpacity)
        audio.setMusic(GameAudio.Music.BATTLE)
        // Deixa a tela de carregamento aparecer antes de gerar o mundo.
        surface.postDelayed({
            engine {
                NativeBridge.startMission(config, look)
                NativeBridge.setInventory(inventory)
            }
        }, 120)
    }

    /** Chamado na thread GL com eventos do motor. */
    private fun onEngineEvents(events: IntArray, count: Int) {
        var i = 0
        while (i + 3 < count) {
            val type = events[i]
            val a = events[i + 1]
            val b = events[i + 2]
            val c = events[i + 3]
            i += 4
            when (type) {
                Ev.SFX -> audio.playEngine(a, b, c)
                Ev.MISSION_READY -> {
                    val size = NativeBridge.minimapSize()
                    val minimap = copyBitmap(size) { NativeBridge.copyMinimap(it) }
                    val bigSize = NativeBridge.bigMapSize()
                    val big = if (bigSize > 0) copyBitmap(bigSize) { NativeBridge.copyBigMap(it) } else null
                    runOnUiThread {
                        bigMap = big
                        worldSize = if (big != null) 6144f else size.toFloat()
                        if (big == null) {
                            // A origem da ilha é atualizada pelo evento MINIMAP.
                            hud.minimapOriginX = 0f
                            hud.minimapOriginZ = 0f
                        }
                        onMissionReady(minimap)
                    }
                }
                Ev.MINIMAP -> {
                    val size = NativeBridge.minimapSize()
                    val minimap = copyBitmap(size) { NativeBridge.copyMinimap(it) }
                    runOnUiThread {
                        hud.minimap = minimap
                        hud.minimapOriginX = (a - size / 2).toFloat()
                        hud.minimapOriginZ = (b - size / 2).toFloat()
                    }
                }
                else -> runOnUiThread { onGameEvent(type, a, b, c) }
            }
        }
    }

    private fun copyBitmap(size: Int, fill: (IntArray) -> Unit): Bitmap? {
        if (size <= 0) return null
        val pixels = IntArray(size * size)
        fill(pixels)
        return Bitmap.createBitmap(pixels, size, size, Bitmap.Config.ARGB_8888)
    }

    private fun onMissionReady(minimap: Bitmap?) {
        if (!inMission) return
        hud.minimap = minimap
        closeOverlay()
        loading = null
        hud.visibility = View.VISIBLE
        hud.message(ObjectiveText.SENTENCE_UPPER, Palette.lime, 2)
        hud.tip("Explore: as construções guardam itens e equipamento")
    }

    /** O objetivo lido do HUD (o HUD já está publicado quando um evento é tratado). */
    private fun currentObjective(): Objective {
        val h = FloatArray(Hud.SIZE)
        NativeBridge.copyHud(h)
        return Objective.from(h)
    }

    private fun onGameEvent(type: Int, a: Int, b: Int, c: Int) {
        if (!inMission) return
        when (type) {
            Ev.KILL -> {
                bestCombo = maxOf(bestCombo, c)
                missionKills += 1
                hud.message(if (a == 1) "TIRO NA CABEÇA! ☠ $missionKills" else "+1 ABATE ☠ $missionKills", if (a == 1) Palette.gold else Palette.lime, CombatFeed.PRIORITY_LOW)
                hud.haptic(false)
            }
            Ev.CRYSTAL -> {
                eventCrystals += 1
                val total = currentObjective().nestTotal.takeIf { it > 0 } ?: 3
                hud.message(ObjectiveText.crystalPicked(eventCrystals.coerceAtMost(total), total), Palette.cyan)
                audio.playUi("reward", 0.7f)
            }
            Ev.NEST_DESTROYED -> {
                eventNests += 1
                val total = currentObjective().nestTotal.takeIf { it > 0 } ?: 3
                hud.message(ObjectiveText.nestDestroyed(eventNests.coerceAtMost(total), total), Palette.purple, CombatFeed.PRIORITY_HIGH)
                hud.haptic(true)
            }
            Ev.CHEST -> {
                hud.message(if (b == 1) "SUPRIMENTOS ENCONTRADOS! +1 GRANADA" else "SUPRIMENTOS ENCONTRADOS!", Palette.gold)
                audio.playUi("reward", 0.8f)
            }
            Ev.ITEM -> {
                val def = Items.get(a)
                if (c == 1) {
                    hud.message("MOCHILA CHEIA! ABRA ESPAÇO 🎒", Palette.muted)
                } else if (def != null) {
                    hud.message(if (Items.isWeapon(a)) "NOVA ARMA: ${def.name.uppercase()}!" else "+$b ${def.name.uppercase()}", def.color)
                    if (Items.isWeapon(a)) {
                        audio.playUi("unlock", 0.9f)
                        hud.tip("Toque na arma na barra de itens para equipar")
                    }
                }
            }
            Ev.ITEM_USED -> {
                val def = Items.get(a)
                if (b == 2) {
                    hud.message("AGUARDE ${c}s PARA SE CURAR DE NOVO", Palette.muted)
                } else if (b == 0) {
                    hud.message(
                        when (a) {
                            Items.ARMOR -> "O COLETE JÁ ESTÁ INTEIRO"
                            Items.ANTIDOTE -> "NADA PARA CURAR AGORA"
                            else -> "SUA VIDA JÁ ESTÁ CHEIA"
                        },
                        Palette.muted,
                    )
                } else if (def != null) {
                    hud.message("${def.name.uppercase()} ✓", def.color)
                    hud.haptic(false)
                }
            }
            Ev.WEAPON -> Items.get(a)?.let { hud.message(it.name.uppercase(), it.color) }
            Ev.POI -> {
                val label = when (a) {
                    PoiKind.FIRE -> "ACAMPAMENTO ABANDONADO"
                    PoiKind.RUINS -> "RUÍNAS ANTIGAS"
                    PoiKind.GRAVEYARD -> "CEMITÉRIO"
                    PoiKind.DEPOT -> "DEPÓSITO DE SUPRIMENTOS"
                    PoiKind.FARM -> "FAZENDA"
                    PoiKind.TOWER -> "TORRE DE VIGIA"
                    PoiKind.OUTPOST -> "POSTO AVANÇADO"
                    PoiKind.HOUSE -> "CASA ABANDONADA"
                    PoiKind.VILLAGE -> "VILAREJO"
                    PoiKind.CHURCH -> "IGREJA"
                    PoiKind.GAS -> "POSTO DE GASOLINA"
                    PoiKind.APARTMENT -> "PRÉDIO EM RUÍNAS"
                    PoiKind.BARN -> "CELEIRO"
                    PoiKind.NEST -> "NINHO DE INFECÇÃO!"
                    else -> "PORTAL DE EXTRAÇÃO"
                }
                hud.message(label, if (a == PoiKind.NEST) Palette.purple else Color.WHITE)
                if (a == PoiKind.TOWER) hud.tip("Suba a escada: lá de cima dá para ver longe — e tem um baú!")
                if (a == PoiKind.APARTMENT) hud.tip("Suba as escadas até o terraço: tem um baú lá em cima!")
                if (a == PoiKind.HOUSE || a == PoiKind.VILLAGE || a == PoiKind.CHURCH || a == PoiKind.GAS || a == PoiKind.BARN) hud.tip("Entre pelas portas: as construções escondem baús")
            }
            Ev.SUPPLY -> {
                if (a == 0) hud.message("SUPRIMENTOS CAINDO! SIGA A FUMAÇA VERDE", Palette.cyan)
                else {
                    hud.message("SUPRIMENTOS: VIDA, MUNIÇÃO E ITENS NA MOCHILA!", Palette.gold)
                    hud.haptic(true)
                }
            }
            Ev.ELITE -> hud.message("ELITE: ${ZombieKinds.get(a).name.uppercase()}", Palette.gold)
            Ev.NEW_KIND -> {
                val info = ZombieKinds.get(a)
                hud.tip("NOVO ZUMBI: ${info.name} — ${info.ability}")
            }
            Ev.STATUS -> {
                when (a) {
                    1 -> hud.message("QUEIMANDO! PULE NA ÁGUA", Color.rgb(255, 150, 60))
                    2 -> hud.message("SANGRANDO!", Palette.red)
                    3 -> hud.message("ENVENENADO!", Color.rgb(140, 255, 90))
                    4 -> hud.message("ÁCIDO! VOCÊ ESTÁ LENTO", Color.rgb(140, 255, 90))
                    5 -> {
                        hud.message("VOCÊ ESCALOU PARA FORA DO BURACO", Palette.cyan)
                        hud.tip("Dica: empurre contra paredes de até 3 blocos para escalar")
                    }
                }
                hud.haptic(true)
            }
            Ev.PORTAL_OPEN -> hud.message(ObjectiveText.PORTAL_OPEN, Palette.purple, CombatFeed.PRIORITY_HIGH)
            Ev.HEAL -> hud.message("+$a VIDA", Palette.red)
            Ev.VICTORY -> surface.postDelayed({ finishMission(true) }, 700)
            Ev.DEFEAT -> surface.postDelayed({ showDefeat() }, 900)
        }
    }

    /** A mochila da partida (itens ganhos/gastos) volta para o progresso salvo. */
    private fun syncInventoryFromEngine() {
        val h = FloatArray(Hud.SIZE)
        NativeBridge.copyHud(h)
        progress.inventory.readFromHud(h)
    }

    private fun finishMission(victory: Boolean) {
        if (missionEnded || !inMission) return
        missionEnded = true
        syncInventoryFromEngine()
        save()
        hud.visibility = View.GONE
        hud.reset()
        val result = FloatArray(Res.SIZE)
        NativeBridge.copyResult(result)
        audio.playUi(if (victory) "victory" else "defeat", 1f)
        audio.setMusic(GameAudio.Music.MENU)
        showOverlay(MatchResultOverlay(this, victory, result[Res.KILLS].toInt(), result[Res.ELAPSED].toInt()))
    }

    private fun showDefeat() {
        if (missionEnded || !inMission) return
        defeatShown = true
        hud.visibility = View.GONE
        NativeBridge.setFire(false)
        audio.playUi("defeat", 1f)
        showOverlay(DefeatOverlay(this))
    }

    fun abandonMission(retry: Boolean) {
        syncInventoryFromEngine()
        save()
        endMissionUi()
        showHome()
        if (retry) startGame()
    }

    private fun endMissionUi() {
        inMission = false
        missionPaused = false
        defeatShown = false
        hud.visibility = View.GONE
        hud.reset()
        closeOverlay()
    }

    override fun onPauseRequested() = pauseMission()

    override fun onInventoryRequested() = openInventoryInMatch()

    // ------------------------------------------------------------------ mochila durante a partida

    private var matchInventory: Inventory? = null

    /** Abre a mochila (pausa o jogo); ao fechar as mudanças voltam ao motor. */
    fun openInventoryInMatch() {
        if (!inMission || missionEnded || defeatShown || loading != null || inventoryOpen) return
        missionPaused = true
        inventoryOpen = true
        hud.reset()
        // A cópia do HUD é feita na thread GL já com o jogo pausado: nenhum quadro muda a mochila entre a cópia
        // e o fechamento (senão um item recolhido nesse quadro sumiria, ou uma granada lançada voltaria).
        engine {
            NativeBridge.setPaused(true)
            val h = FloatArray(Hud.SIZE)
            NativeBridge.copyHud(h)
            runOnUiThread { showInventoryInMatch(h) }
        }
    }

    private fun showInventoryInMatch(h: FloatArray) {
        // A partida terminou enquanto a cópia era feita (a pausa normal não abre: missionPaused já está ligado).
        if (!inventoryOpen || !inMission || missionEnded || defeatShown) {
            inventoryOpen = false
            return
        }
        val working = Inventory().apply {
            copyFrom(progress.inventory)
            readFromHud(h)
        }
        matchInventory = working
        lateinit var screen: InventoryScreen
        screen = InventoryScreen(this, working, inMatch = true, hud = h) { use -> closeInventoryInMatch(screen, use) }
        // Painel translúcido (estilo Free Fire): o jogo pausado aparece atrás, sem os botões do HUD por cima.
        hud.visibility = View.GONE
        showOverlay(screen)
    }

    fun closeInventoryInMatch(screen: InventoryScreen, useSlot: Int) {
        val inv = matchInventory ?: return
        matchInventory = null
        inventoryOpen = false
        inv.normalize()
        progress.inventory.copyFrom(inv)
        save()
        val data = inv.toIntArray()
        // Descartados na mochila caem no chão à frente do jogador (depois do inventário novo chegar ao motor).
        val drops = screen.drops.toList()
        engine {
            NativeBridge.setInventory(data)
            for ((id, count) in drops) NativeBridge.dropItem(id, count)
        }
        if (useSlot >= 0) NativeBridge.useSlot(useSlot)
        resumeMission()
    }

    override fun onMapRequested() {
        if (!inMission || missionEnded) return
        missionPaused = true
        engine { NativeBridge.setPaused(true) }
        hud.reset()
        showOverlay(MapOverlay(this, bigMap ?: hud.minimap, worldSize) {
            closeOverlay()
            resumeMission()
        })
    }

    fun openMapFromPause() {
        if (!inMission || missionEnded) return
        showOverlay(MapOverlay(this, bigMap ?: hud.minimap, worldSize) { showOverlay(PauseOverlay(this, ObjectiveText.SENTENCE_UPPER)) })
    }

    fun pauseMission() {
        if (!inMission || missionEnded) return
        missionPaused = true
        engine { NativeBridge.setPaused(true) }
        hud.reset()
        showOverlay(PauseOverlay(this, ObjectiveText.SENTENCE_UPPER))
    }

    fun resumeMission() {
        if (!inMission) return
        missionPaused = false
        closeOverlay()
        hud.visibility = View.VISIBLE
        engine { NativeBridge.setPaused(false) }
    }

    fun showPauseSettings() {
        showOverlay(SettingsScreen(this, fromPause = true))
    }

    /** Editor de HUD (posição, tamanho e transparência dos controles). */
    fun showHudEditor(fromPause: Boolean) = showOverlay(HudEditorScreen(this, fromPause))

}
