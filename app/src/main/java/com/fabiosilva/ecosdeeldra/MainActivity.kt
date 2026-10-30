package com.fabiosilva.ecosdeeldra

import android.graphics.Bitmap
import android.os.Build
import android.os.Bundle
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.OnBackPressedCallback
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.viewinterop.AndroidView
import com.fabiosilva.ecosdeeldra.audio.GameAudio
import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.SaveStore
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.RecoveryReport
import com.fabiosilva.ecosdeeldra.data.save.SaveNotice
import com.fabiosilva.ecosdeeldra.engine.BattleSession
import com.fabiosilva.ecosdeeldra.engine.GameSurfaceView
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import com.fabiosilva.ecosdeeldra.ui.*
import org.json.JSONObject
import java.util.concurrent.atomic.AtomicReference

class MainActivity : ComponentActivity(), HudView.Listener, BattleSession.Listener {
    private enum class Screen { HOME, ARMORY, SETTINGS, RECOVERY }
    private enum class Overlay { NONE, LOADING, PAUSE, PAUSE_SETTINGS, HUD_EDITOR_HOME, HUD_EDITOR_PAUSE, BACKPACK, MAP, RESULT }

    lateinit var progress: Progress; private set
    lateinit var audio: GameAudio; private set
    lateinit var store: SaveStore; private set
    private lateinit var surface: GameSurfaceView
    private lateinit var hud: HudView
    private lateinit var session: BattleSession

    private var activeScreen by mutableStateOf(Screen.HOME)
    private var activeOverlay by mutableStateOf(Overlay.NONE)
    private var inBattle by mutableStateOf(false)
    private var loading by mutableStateOf(false)
    private var searching by mutableStateOf(false)
    private var teamSize by mutableIntStateOf(1)
    private var autoFill by mutableStateOf(true)
    private var previewWeaponState by mutableIntStateOf(3)
    private var status by mutableStateOf("Conectando…")
    private var resultData by mutableStateOf(Triple(0, 0, false))
    private var recoveryReport by mutableStateOf<RecoveryReport?>(null)

    private var localId = -1
    private var ping = 0
    private var state: FloatArray? = null
    private var map: Bitmap? = null
    private var mapHash = 0L
    private var expectedMapHash = 0L
    private val incoming = AtomicReference<FloatArray?>()
    private val incomingEffects = AtomicReference<FloatArray?>()
    private val display = AtomicReference<FloatArray?>()
    private var resultShown = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        if (Build.VERSION.SDK_INT >= 28) {
            window.attributes.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        store = SaveStore(this)
        val saved = store.load()
        progress = when (saved) {
            is LoadResult.Loaded -> saved.progress
            else -> Progress()
        }
        previewWeaponState = progress.settings.previewWeapon
        audio = GameAudio(this)
        Ui.clickSound = { audio.playUi("click", 0.6f) }
        NativeBridge.init(assets)
        session = BattleSession(this, this)
        surface = GameSurfaceView(this, ::events, { session.input(it) }, ::nativeFrame)
        hud = HudView(this, this).apply { visibility = View.GONE }

        applySettings()
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                when {
                    activeOverlay != Overlay.NONE -> {
                        if (activeOverlay == Overlay.HUD_EDITOR_PAUSE || activeOverlay == Overlay.PAUSE_SETTINGS) {
                            activeOverlay = Overlay.PAUSE
                        } else if (activeOverlay == Overlay.PAUSE) {
                            resumeMission()
                        } else if (activeOverlay != Overlay.LOADING && activeOverlay != Overlay.RESULT) {
                            closeOverlay()
                        }
                    }
                    inBattle -> pauseMission()
                    activeScreen != Screen.HOME && activeScreen != Screen.RECOVERY -> showHome()
                }
            }
        })

        if (saved is LoadResult.NeedsRecovery) {
            recoveryReport = saved.report
            activeScreen = Screen.RECOVERY
        } else {
            showHome()
            session.connect()
        }

        setContent {
            Box(modifier = Modifier.fillMaxSize()) {
                AndroidView(
                    factory = { surface },
                    modifier = Modifier.fillMaxSize()
                )
                AndroidView(
                    factory = { hud },
                    update = { view ->
                        val showHud = inBattle && !loading && activeOverlay == Overlay.NONE
                        view.visibility = if (showHud) View.VISIBLE else View.GONE
                    },
                    modifier = Modifier.fillMaxSize()
                )

                if (!inBattle) {
                    when (activeScreen) {
                        Screen.HOME -> LobbyComposeScreen(
                            app = this@MainActivity,
                            statusText = status,
                            teamSize = teamSize,
                            autoFill = autoFill,
                            searching = searching,
                            previewWeapon = previewWeaponState,
                            onSelectFormation = { sz, fill -> selectFormation(sz, fill) },
                            onStartOrCancel = { if (searching) cancelSearch() else startGame() }
                        )
                        Screen.ARMORY -> ArmoryComposeScreen(
                            app = this@MainActivity,
                            selectedWeapon = previewWeaponState,
                            onSelectWeapon = { id ->
                                progress.settings.previewWeapon = id
                                previewWeaponState = id
                                save()
                                engine { NativeBridge.previewWeapon(id) }
                            },
                            onBack = { showHome() }
                        )
                        Screen.SETTINGS -> SettingsComposeScreen(
                            app = this@MainActivity,
                            fromPause = false,
                            onOpenHudEditor = { showHudEditor(false) },
                            onClose = { showHome() }
                        )
                        Screen.RECOVERY -> recoveryReport?.let { rep ->
                            RecoveryComposeScreen(app = this@MainActivity, initialReport = rep)
                        }
                    }
                }

                when (activeOverlay) {
                    Overlay.NONE -> Unit
                    Overlay.LOADING -> LoadingComposeOverlay()
                    Overlay.PAUSE -> PauseComposeOverlay(
                        onResume = { resumeMission() },
                        onSettings = { showPauseSettings() },
                        onAbandon = { abandonMission(false) }
                    )
                    Overlay.PAUSE_SETTINGS -> SettingsComposeScreen(
                        app = this@MainActivity,
                        fromPause = true,
                        onOpenHudEditor = { showHudEditor(true) },
                        onClose = { pauseMission() }
                    )
                    Overlay.HUD_EDITOR_HOME -> HudEditorComposeScreen(
                        app = this@MainActivity,
                        onClose = { closeOverlay() }
                    )
                    Overlay.HUD_EDITOR_PAUSE -> HudEditorComposeScreen(
                        app = this@MainActivity,
                        onClose = { showPauseSettings() }
                    )
                    Overlay.BACKPACK -> state?.let { s ->
                        BackpackComposeOverlay(
                            app = this@MainActivity,
                            state = s,
                            onClose = { closeOverlay() }
                        )
                    }
                    Overlay.MAP -> state?.let { s ->
                        BattleMapComposeOverlay(
                            mapBitmap = map,
                            state = s,
                            localId = localId,
                            onMarkAndClose = { x, z ->
                                if (x >= 0f) NativeBridge.battleAction(16384, -1, x, z)
                                closeOverlay()
                            }
                        )
                    }
                    Overlay.RESULT -> ResultComposeOverlay(
                        place = resultData.first,
                        kills = resultData.second,
                        training = resultData.third,
                        onReturnLobby = { leaveBattle() }
                    )
                }
            }
        }
    }

    fun onRecoveryResolved(value: Progress, notices: List<SaveNotice> = emptyList()) {
        progress = value
        previewWeaponState = progress.settings.previewWeapon
        applySettings()
        showHome()
        session.connect()
    }

    override fun onResume() {
        super.onResume()
        surface.onResume()
        surface.start()
        audio.resume()
        immersive()
    }

    override fun onPause() {
        hud.reset()
        engine { NativeBridge.setPaused(true) }
        surface.stop()
        surface.onPause()
        audio.pause()
        store.saveNow(progress)
        super.onPause()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            immersive()
            if (inBattle && !loading && activeOverlay == Overlay.NONE) {
                engine { NativeBridge.setPaused(false) }
            }
        }
    }

    override fun onDestroy() {
        session.close()
        audio.release()
        super.onDestroy()
    }

    private fun immersive() {
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false)
            window.insetsController?.let {
                it.hide(WindowInsets.Type.systemBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        } else {
            @Suppress("DEPRECATION")
            window.decorView.systemUiVisibility = 5894
        }
    }

    fun engine(block: () -> Unit) = surface.engine(block)
    fun save() { store.save(progress) }

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

    fun closeOverlay() {
        activeOverlay = Overlay.NONE
    }

    fun showHome() {
        closeOverlay()
        activeScreen = Screen.HOME
        previewWeaponState = progress.settings.previewWeapon
        if (!inBattle) {
            engine {
                NativeBridge.enterLobby(progress.lookCode, 0)
                NativeBridge.previewWeapon(progress.settings.previewWeapon)
            }
        }
        audio.setMusic(GameAudio.Music.MENU)
    }

    fun showInventory(back: () -> Unit = { showHome() }) {
        if (inBattle) {
            onInventoryRequested()
            return
        }
        engine {
            NativeBridge.enterLobby(progress.lookCode, 2)
            NativeBridge.previewWeapon(progress.settings.previewWeapon)
        }
        activeScreen = Screen.ARMORY
    }

    fun showSettings() {
        activeScreen = Screen.SETTINGS
    }

    fun showPauseSettings() {
        activeOverlay = Overlay.PAUSE_SETTINGS
    }

    fun showHudEditor(fromPause: Boolean) {
        activeOverlay = if (fromPause) Overlay.HUD_EDITOR_PAUSE else Overlay.HUD_EDITOR_HOME
    }

    fun selectFormation(size: Int, fill: Boolean) {
        teamSize = size
        autoFill = fill
    }

    fun group(action: String, code: String = "") {
        session.party(action, teamSize, autoFill, code)
    }

    fun startGame() {
        if (inBattle) return
        if (session.leaving) {
            onStatus("Saindo da partida anterior…")
            return
        }
        try {
            val manifest = assets.open("map-manifest.json").bufferedReader().use { JSONObject(it.readText()) }
            mapHash = manifest.getLong("hash")
            require(manifest.getInt("version") == 4)
        } catch (_: Exception) {
            onStatus("O mapa desta versão está indisponível.")
            return
        }
        searching = true
        status = "Buscando adversários…"
        session.queue(teamSize, autoFill, mapHash, progress.lookCode)
    }

    fun cancelSearch() {
        searching = false
        session.cancelQueue()
    }

    override fun onStatus(text: String) {
        runOnUiThread {
            status = text
            if (inBattle) hud.message(text)
        }
    }

    override fun onLatency(ms: Int) { ping = ms }
    override fun onSnapshot(state: FloatArray) { incoming.set(state) }
    override fun onEffects(state: FloatArray) { incomingEffects.set(state) }

    private fun nativeFrame() {
        incoming.getAndSet(null)?.let {
            NativeBridge.applyBattleSnapshot(it)
            display.set(it)
        }
        incomingEffects.getAndSet(null)?.let {
            NativeBridge.applyBattleEffects(it)
        }
    }

    override fun onMessage(message: JSONObject) {
        runOnUiThread {
            if (BuildConfig.DEBUG) android.util.Log.i("ZZBattle", "server_event=" + message.optString("type"))
            when (message.optString("type")) {
                "connected" -> {
                    if (inBattle && !resultShown) leaveBattle(false)
                    searching = false
                    status = "Online • escolha Solo, Duo ou Squad"
                }
                "queued" -> {
                    searching = true
                    status = "Aguardando adversários…"
                }
                "cancelled" -> {
                    searching = false
                    status = message.optString("message", "Busca cancelada")
                }
                "countdown" -> {
                    val seconds = (message.optLong("remainingMs") + 999) / 1000
                    val text = if (message.optString("stage") == "lobby") {
                        "Partida começa em ${seconds}s • ${message.optInt("humans")} jogadores"
                    } else {
                        "Voo em ${seconds}s"
                    }
                    status = text
                    hud.message(text)
                }
                "party" -> {
                    val party = message.optJSONObject("party")
                    status = if (party == null) {
                        "Você saiu do grupo"
                    } else {
                        "Grupo ${party.getString("code")} • ${party.getJSONArray("members").length()}/${party.getInt("teamSize")}"
                    }
                    party?.let {
                        teamSize = it.getInt("teamSize")
                        autoFill = it.getBoolean("autoFill")
                    }
                }
                "start" -> {
                    expectedMapHash = message.getLong("mapHash")
                    incomingEffects.set(null)
                    localId = message.getInt("localId")
                    val resume = message.optBoolean("resume")
                    searching = false
                    inBattle = true
                    loading = true
                    resultShown = false
                    activeOverlay = Overlay.LOADING
                    engine {
                        if (!NativeBridge.prepareBattle(localId, progress.lookCode, message.optInt("lastSequence"), resume)) {
                            runOnUiThread {
                                leaveBattle()
                                onStatus("Não foi possível preparar a partida")
                            }
                        }
                    }
                }
                "result" -> {
                    val player = message.optJSONObject("player")
                    showResult(player?.optInt("placement") ?: 0, player?.optInt("kills") ?: 0, message.optBoolean("training"))
                }
                "matchClosed" -> {
                    if (inBattle && !resultShown) {
                        leaveBattle(false)
                        onStatus("Partida encerrada: ${message.optString("reason")}")
                    }
                }
                "error" -> {
                    searching = false
                    onStatus(message.optString("message", "Não foi possível iniciar"))
                }
                "draining" -> onStatus("Servidor em atualização. A partida será encerrada em breve.")
            }
        }
    }

    private fun events(values: IntArray, n: Int) {
        val copy = values.copyOf(n)
        var readyHash = 0L
        var pixels: IntArray? = null
        if ((0 until n step 4).any { copy[it] == 100 }) {
            readyHash = NativeBridge.battleMapHash()
            pixels = IntArray(256 * 256).also { NativeBridge.copyBigMap(it) }
        }
        runOnUiThread {
            for (i in 0 until n step 4) {
                when (copy[i]) {
                    1 -> audio.playEngine(copy[i + 1], copy[i + 2], copy[i + 3])
                    100 -> {
                        if (readyHash != expectedMapHash) {
                            leaveBattle()
                            onStatus("Mapa incompatível com o servidor")
                            return@runOnUiThread
                        }
                        pixels?.let {
                            map?.recycle()
                            map = Bitmap.createBitmap(it, 256, 256, Bitmap.Config.ARGB_8888)
                            hud.minimap = map
                        }
                        loading = false
                        closeOverlay()
                        hud.reset()
                        session.ready(readyHash, progress.lookCode)
                        engine { NativeBridge.setPaused(false) }
                        audio.setMusic(GameAudio.Music.BATTLE)
                    }
                    101 -> {
                        leaveBattle()
                        onStatus("Falha ao carregar o mapa competitivo")
                    }
                    7 -> hud.haptic(true)
                }
            }
        }
    }

    private val refresh = object : Runnable {
        override fun run() {
            display.getAndSet(null)?.let {
                state = it
                hud.updateBattle(it, localId, ping)
                val me = (0 until it[16].toInt()).map { n -> 24 + n * 32 }.firstOrNull { n -> it[n].toInt() == localId }
                if (me != null && it[1] == 3f && !resultShown) {
                    showResult(it[me + 23].toInt(), it[me + 10].toInt(), false)
                }
            }
            surface.postDelayed(this, 100)
        }
    }

    override fun onStart() {
        super.onStart()
        surface.post(refresh)
    }

    override fun onStop() {
        surface.removeCallbacks(refresh)
        super.onStop()
    }

    private fun showResult(place: Int, kills: Int, training: Boolean) {
        if (resultShown) return
        resultShown = true
        hud.reset()
        resultData = Triple(place, kills, training)
        activeOverlay = Overlay.RESULT
    }

    private fun leaveBattle(notifyServer: Boolean = true) {
        if (BuildConfig.DEBUG) android.util.Log.i("ZZBattle", "return_lobby notifyServer=$notifyServer")
        if (notifyServer) session.leaveMatch()
        inBattle = false
        loading = false
        searching = false
        localId = -1
        state = null
        incoming.set(null)
        incomingEffects.set(null)
        display.set(null)
        hud.reset()
        showHome()
    }

    fun abandonMission(retry: Boolean) {
        leaveBattle()
        if (retry) startGame()
    }

    fun pauseMission() {
        if (!inBattle) return
        hud.reset()
        engine { NativeBridge.setPaused(true) }
        activeOverlay = Overlay.PAUSE
    }

    fun resumeMission() {
        closeOverlay()
        hud.reset()
        engine { NativeBridge.setPaused(false) }
    }

    override fun onPauseRequested() = pauseMission()

    override fun onInventoryRequested() {
        if (state == null) return
        hud.reset()
        activeOverlay = Overlay.BACKPACK
    }

    override fun onMapRequested() {
        if (state == null) return
        hud.reset()
        activeOverlay = Overlay.MAP
    }

    fun openMapFromPause() = onMapRequested()
}
