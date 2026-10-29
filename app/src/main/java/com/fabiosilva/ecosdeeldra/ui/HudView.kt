package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RadialGradient
import android.graphics.RectF
import android.graphics.Shader
import android.os.SystemClock
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.Items
import com.fabiosilva.ecosdeeldra.data.Reticle
import com.fabiosilva.ecosdeeldra.data.WeaponSpecs
import com.fabiosilva.ecosdeeldra.data.ZombieKinds
import com.fabiosilva.ecosdeeldra.engine.Hud
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import com.fabiosilva.ecosdeeldra.objective.Objective
import com.fabiosilva.ecosdeeldra.objective.PillState
import kotlin.math.PI
import kotlin.math.abs
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin
import kotlin.math.tan

/**
 * HUD da missão + controles multitoque:
 * - joystick dinâmico (surge onde o polegar toca; empurrar até o fim = correr);
 * - arrastar na metade direita = girar a câmera;
 * - botão ATIRAR (segurar) também gira a câmera ao arrastar;
 * - MIRAR, PULAR, RECARREGAR, AGACHAR, CORRER (corrida automática), FACA, GRANADA,
 *   PAUSA e MINIMAPA; um toque em CORRER faz o personagem sair correndo
 *   sozinho para a frente (o joystick ainda dirige; puxar para trás ou tocar de novo para).
 * Todos os controles podem ser movidos/redimensionados no editor (modo [editing]).
 * Na ilha mostra ninhos, portal, efeitos (fogo, sangramento, veneno) e zumbis elite.
 */
@SuppressLint("ViewConstructor")
class HudView(context: Context, private val listener: Listener) : View(context) {
    interface Listener {
        fun onPauseRequested()
        fun onMapRequested()
        fun onInventoryRequested() = Unit
    }

    private enum class Role { NONE, JOYSTICK, LOOK, FIRE, FIRE_LEFT, AIM, JUMP, RELOAD, CROUCH, SPRINT, MELEE, GRENADE, MEDKIT, EMOTE, EMOTE_ITEM, WEAPON_SLOT, PAUSE, MAP, HOTBAR, BAG }
    private class Touch(var role: Role, var x: Float, var y: Float, val startX: Float, val startY: Float, var slot: Int = -1, var cancel: Boolean = false)
    private class Geo(var x: Float = 0f, var y: Float = 0f, var r: Float = 0f, var visible: Boolean = true)

    private val hud = FloatArray(Hud.SIZE)
    private val touches = HashMap<Int, Touch>()
    private val density = resources.displayMetrics.density
    private val p = Paint(Paint.ANTI_ALIAS_FLAG)
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Fonts.title; textAlign = Paint.Align.CENTER }
    private val rect = RectF()
    private val path = Path()
    private var vignette: RadialGradient? = null
    private var vignettePaint = Paint()
    private val statusPaint = Paint()
    private val statusShaders = HashMap<Int, RadialGradient>()
    private val dash = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }

    var minimap: Bitmap? = null
    var minimapOriginX = 0f   // canto do bitmap do minimapa em coordenadas do mundo
    var minimapOriginZ = 0f
    var bigMap: Bitmap? = null
    var playerName = ""
    var showFps = false
    var leftHanded = false
    var vibration = true
    var layoutText = ""   // layout salvo nos ajustes (vazio = padrão)
    var opacity = 0.85f   // transparência dos controles

    /** Modo de edição do HUD: arrastar move os controles em vez de jogar. */
    var editing = false
        set(value) {
            field = value
            selected = null
            invalidate()
        }
    var selected: HudControl? = null
        private set
    var onSelectionChanged: ((HudControl?) -> Unit)? = null
    var layout: HudLayout = HudLayout.defaults(900f, 410f, false)
        private set

    private var aiming = false
    private var aimStage = 0
    private var aimWeapon = -1
    /** Corrida automática ligada pelo botão CORRER (o estado vem do motor). */
    private val autoRun get() = hud[Hud.SPRINT_LOCK] > 0.5f
    private var stickBaseX = 0f
    private var stickBaseY = 0f
    private var stickX = 0f
    private var stickY = 0f
    private var stickActive = false
    /** Aviso de combate (uma só mensagem por vez) e dica longa: estado puro em CombatFeed.kt. */
    private val combatFeed = CombatFeed()
    /** Banner de onda desenhado neste quadro: o aviso de combate cede a faixa a ele. */
    private var bannerShown = false
    private val tipState = HudTip()
    private var objective = Objective.INACTIVE
    private var objectiveAt = 0L
    private var lastHealth = -1f
    private var dragControl: HudControl? = null
    private var dragDx = 0f
    private var dragDy = 0f

    // Geometria dos controles (px), recalculada a partir do layout.
    private val geo: Map<HudControl, Geo> = HudControl.entries.associateWith { Geo() }
    private fun g(c: HudControl): Geo = geo.getValue(c)
    private var mapX = 0f; private var mapY = 0f; private var mapSize = 0f
    private var stickRadius = 0f
    private var stickHomeX = 0f; private var stickHomeY = 0f
    private var hotbarX = 0f; private var hotbarY = 0f; private var slotSize = 0f
    private var lastWeapon = -1
    /** Base do chip de contrato neste quadro (0 sem chip): os efeitos ativos começam abaixo dele. */
    private val fireDash = android.graphics.DashPathEffect(floatArrayOf(d(1.5f), d(4f)), 0f)
    /** Degradê dourado do painel de armas, refeito só quando o painel muda de lugar. */
    private var goldShader: LinearGradient? = null
    private var goldShaderLeft = 0f
    private var goldShaderRight = 0f

    private fun d(v: Float) = v * density

    init {
        isClickable = true
        ItemIcons.initialize(context)
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        rebuildLayout()
        vignette = RadialGradient(w / 2f, h / 2f, hypot(w / 2f, h / 2f), intArrayOf(Color.TRANSPARENT, Color.TRANSPARENT, Color.argb(220, 180, 0, 0)), floatArrayOf(0f, 0.55f, 1f), Shader.TileMode.CLAMP)
        vignettePaint.shader = vignette
        statusShaders.clear()
    }

    /** Recalcula o layout (padrão ou o salvo pelo jogador) para o tamanho atual da tela. */
    fun rebuildLayout() {
        if (width <= 0 || height <= 0) return
        layout = HudLayout.decode(layoutText, defaultLayout())
        layoutControls()
    }

    fun defaultLayout(): HudLayout = HudLayout.defaults(width / density, height / density, leftHanded)

    /** Editor: aplica uma cópia de trabalho sem salvar. */
    fun setWorkingLayout(l: HudLayout) {
        layout = l
        layoutControls()
        invalidate()
    }

    private fun layoutControls() {
        val w = width.toFloat()
        val h = height.toFloat()
        if (w <= 0f) return
        for (c in HudControl.entries) {
            val pos = layout[c]
            val gc = g(c)
            gc.x = pos.x * w
            gc.y = pos.y * h
            gc.r = d(c.baseDp) * pos.scale
            gc.visible = pos.visible
        }
        g(HudControl.MAP).let { mapX = it.x; mapY = it.y; mapSize = it.r * 2f }
        g(HudControl.JOYSTICK).let { stickRadius = it.r; stickHomeX = it.x; stickHomeY = it.y }
        g(HudControl.HOTBAR).let { hotbarX = it.x; hotbarY = it.y; slotSize = it.r * 2f }
        if (!stickActive) {
            stickBaseX = stickHomeX; stickBaseY = stickHomeY; stickX = stickBaseX; stickY = stickBaseY
        }
    }

    fun applySettings(leftHanded: Boolean, vibration: Boolean, showFps: Boolean, layoutText: String = this.layoutText, opacity: Float = this.opacity) {
        this.leftHanded = leftHanded
        this.vibration = vibration
        this.showFps = showFps
        this.layoutText = layoutText
        this.opacity = opacity.coerceIn(0.3f, 1f)
        rebuildLayout()
    }

    fun reset() {
        touches.clear()
        aiming = false
        aimStage = 0
        aimWeapon = -1
        stickActive = false
        combatFeed.clear()
        tipState.clear()
        lastHealth = -1f
        NativeBridge.setMove(0f, 0f)
        NativeBridge.setFire(false)
        NativeBridge.setAimStage(0)
        NativeBridge.setThrowAim(false, true)
        lastWeapon = -1
        if (width > 0) layoutControls()
    }

    // ------------------------------------------------------------------ editor do HUD

    fun scaleSelected(delta: Float) {
        val c = selected ?: return
        val pos = layout[c]
        pos.scale = (pos.scale + delta).coerceIn(HudLayout.MIN_SCALE, HudLayout.MAX_SCALE)
        layoutControls()
        invalidate()
    }

    /** Mostra/esconde o controle selecionado; retorna o novo estado. */
    fun toggleSelectedVisible(): Boolean? {
        val c = selected ?: return null
        if (!c.optional) return null
        val pos = layout[c]
        pos.visible = !pos.visible
        layoutControls()
        invalidate()
        return pos.visible
    }

    private fun pickControl(x: Float, y: Float): HudControl? {
        var best: HudControl? = null
        var bestDist = Float.MAX_VALUE
        for (c in HudControl.entries) {
            val gc = g(c)
            val hit = when (c) {
                HudControl.MAP -> abs(x - gc.x) < gc.r && abs(y - gc.y) < gc.r
                HudControl.HOTBAR -> abs(x - gc.x) < gc.r * 6.2f && abs(y - gc.y) < gc.r * 1.2f
                HudControl.WEAPON -> weaponRect().contains(x, y)
                else -> hypot(x - gc.x, y - gc.y) < gc.r * 1.25f + d(6f)
            }
            if (!hit) continue
            val dist = hypot(x - gc.x, y - gc.y)
            if (dist < bestDist) {
                bestDist = dist
                best = c
            }
        }
        return best
    }

    private fun editTouch(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                val c = pickControl(event.x, event.y)
                selected = c
                onSelectionChanged?.invoke(c)
                dragControl = c
                if (c != null) {
                    dragDx = g(c).x - event.x
                    dragDy = g(c).y - event.y
                }
                invalidate()
            }
            MotionEvent.ACTION_MOVE -> dragControl?.let { c ->
                val pos = layout[c]
                pos.x = ((event.x + dragDx) / width).coerceIn(0.02f, 0.98f)
                pos.y = ((event.y + dragDy) / height).coerceIn(0.03f, 0.97f)
                layoutControls()
                invalidate()
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> dragControl = null
        }
        return true
    }

    /** Aviso de combate: só o mais recente fica na tela (substitui o anterior e expira em 1800 ms). */
    fun message(text: String, color: Int = Palette.lime, priority: Int = CombatFeed.PRIORITY_NORMAL) {
        combatFeed.show(text, color, System.currentTimeMillis(), priority)
    }

    /** Dica longa (ex.: habilidade de um zumbi novo) mostrada na parte de baixo. */
    fun tip(text: String) {
        tipState.show(text, System.currentTimeMillis())
    }

    fun haptic(strong: Boolean) {
        if (!vibration) return
        performHapticFeedback(if (strong) HapticFeedbackConstants.LONG_PRESS else HapticFeedbackConstants.KEYBOARD_TAP)
    }

    private val island get() = hud[Hud.ISLAND] > 0.5f
    private val inventoryOn get() = hud[Hud.INV_ENABLED] > 0.5f
    private val survival get() = hud[Hud.SURV] > 0.5f

    // ------------------------------------------------------------------ toque

    private fun inside(x: Float, y: Float, cx: Float, cy: Float, r: Float) = hypot(x - cx, y - cy) <= r

    /** O controle aparece nesta partida? (granada e faca só na ilha; suprimentos só quando há.) */
    private fun available(c: HudControl): Boolean {
        if (!g(c).visible) return false
        return when (c) {
            HudControl.GRENADE -> hud[Hud.GRENADES] >= 0f
            HudControl.MEDKIT -> true
            HudControl.WEAPON, HudControl.EMOTE -> true
            HudControl.MELEE -> hud[Hud.MELEE] >= 0f
            HudControl.HOTBAR, HudControl.BAG -> editing || inventoryOn
            else -> true
        }
    }

    private val buttons = listOf(
        HudControl.FIRE to Role.FIRE, HudControl.FIRE_LEFT to Role.FIRE_LEFT, HudControl.AIM to Role.AIM,
        HudControl.JUMP to Role.JUMP, HudControl.CROUCH to Role.CROUCH, HudControl.RELOAD to Role.RELOAD,
        HudControl.SPRINT to Role.SPRINT, HudControl.MELEE to Role.MELEE, HudControl.GRENADE to Role.GRENADE,
        HudControl.MEDKIT to Role.MEDKIT, HudControl.PAUSE to Role.PAUSE,
    )

    /** Espaço da barra rápida sob o toque (-1 se nenhum). */
    private fun hotbarSlotAt(x: Float, y: Float): Int {
        if (!available(HudControl.HOTBAR)) return -1
        val left = hotbarX - slotSize * 3f
        if (y < hotbarY - slotSize * 0.6f || y > hotbarY + slotSize * 0.6f || x < left || x > left + slotSize * 6f) return -1
        return ((x - left) / slotSize).toInt().coerceIn(0, 5)
    }

    /** O toque cai na área do controle [c] (formas em HudTouch, as mesmas usadas nos testes de layout)? */
    private fun touchHit(c: HudControl, x: Float, y: Float): Boolean {
        if (!available(c)) return false
        val gc = g(c)
        return HudTouch.area(c, gc.x, gc.y, gc.r, density).contains(x, y)
    }

    private fun roleAt(x: Float, y: Float): Role {
        val w = width.toFloat()
        val h = height.toFloat()
        if (emoteOpen && emoteItemAt(x, y) >= 0) return Role.EMOTE_ITEM
        if (available(HudControl.WEAPON) && weaponSlotAt(x, y) >= 0) return Role.WEAPON_SLOT
        if (available(HudControl.WEAPON) && weaponTopRect().contains(x, y)) return Role.RELOAD
        if (touchHit(HudControl.EMOTE, x, y)) return Role.EMOTE
        if (hotbarSlotAt(x, y) >= 0) return Role.HOTBAR
        if (touchHit(HudControl.BAG, x, y)) return Role.BAG
        // As folgas de toque de botões vizinhos se cruzam (como na referência): vale o centro mais próximo.
        buttons.filter { touchHit(it.first, x, y) }
            .minByOrNull { hypot(x - g(it.first).x, y - g(it.first).y) }
            ?.let { return it.second }
        if (touchHit(HudControl.MAP, x, y)) return Role.MAP
        // Joystick dinâmico no lado em que ele foi posicionado.
        val leftSide = if (stickHomeX < w / 2) x < w * 0.45f else x > w * 0.55f
        if (leftSide && y > h * 0.28f && touches.values.none { it.role == Role.JOYSTICK }) return Role.JOYSTICK
        return Role.LOOK
    }

    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (editing) return editTouch(event)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = event.actionIndex
                val id = event.getPointerId(i)
                val x = event.getX(i)
                val y = event.getY(i)
                val role = roleAt(x, y)
                if (emoteOpen && role != Role.EMOTE && role != Role.EMOTE_ITEM) emoteOpen = false
                val touch = Touch(role, x, y, x, y)
                touches[id] = touch
                when (role) {
                    Role.JOYSTICK -> {
                        stickActive = true
                        stickBaseX = x; stickBaseY = y; stickX = x; stickY = y
                    }
                    Role.FIRE, Role.FIRE_LEFT -> { NativeBridge.setFire(true); haptic(false) }
                    Role.AIM -> {
                        aimStage = if (aimStage == 0) 1 else 0
                        aiming = aimStage > 0
                        NativeBridge.setAimStage(aimStage)
                        haptic(false)
                    }
                    Role.JUMP -> { NativeBridge.pressJump(); haptic(false) }
                    Role.RELOAD -> { NativeBridge.pressReload(); haptic(false) }
                    Role.CROUCH -> { NativeBridge.toggleCrouch(); haptic(false) }
                    Role.SPRINT -> { NativeBridge.toggleSprint(); haptic(false) }
                    Role.MELEE -> { NativeBridge.pressMelee(); haptic(false) }
                    Role.MEDKIT -> { useMedkit(); haptic(false) }
                    Role.EMOTE -> { emoteOpen = !emoteOpen; haptic(false) }
                    Role.WEAPON_SLOT -> { weaponSlotTap(x, y); haptic(false) }
                    Role.EMOTE_ITEM -> { sendEmote(emoteItemAt(x, y)); emoteOpen = false; haptic(false) }
                    Role.GRENADE -> {
                        // Segurar mostra a trajetória; arrastar até o X cancela; soltar arremessa.
                        if (hud[Hud.GRENADES] <= 0f) message("SEM GRANADAS NA MOCHILA", Palette.muted)
                        NativeBridge.setThrowAim(true, false)
                        haptic(false)
                    }
                    Role.HOTBAR -> {
                        touch.slot = hotbarSlotAt(x, y)
                        NativeBridge.tapSlot(touch.slot)
                        haptic(false)
                    }
                    Role.BAG -> listener.onInventoryRequested()
                    Role.PAUSE -> listener.onPauseRequested()
                    Role.MAP -> listener.onMapRequested()
                    else -> Unit
                }
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val t = touches[event.getPointerId(i)] ?: continue
                    val x = event.getX(i)
                    val y = event.getY(i)
                    when (t.role) {
                        Role.JOYSTICK -> {
                            var dx = x - stickBaseX
                            var dy = y - stickBaseY
                            val len = hypot(dx, dy)
                            // Base acompanha o polegar se ele passar muito do limite.
                            if (len > stickRadius * 1.6f) {
                                val k = (len - stickRadius * 1.6f) / len
                                stickBaseX += dx * k; stickBaseY += dy * k
                                dx = x - stickBaseX; dy = y - stickBaseY
                            }
                            val l2 = hypot(dx, dy)
                            val clamped = min(l2, stickRadius)
                            val nx = if (l2 > 0) dx / l2 * clamped else 0f
                            val ny = if (l2 > 0) dy / l2 * clamped else 0f
                            stickX = stickBaseX + nx; stickY = stickBaseY + ny
                            var mx = nx / stickRadius
                            var my = -ny / stickRadius
                            val mag = hypot(mx, my)
                            if (mag < 0.12f) { mx = 0f; my = 0f }
                            NativeBridge.setMove(mx, my)
                        }
                        Role.LOOK, Role.FIRE, Role.FIRE_LEFT -> NativeBridge.addLook((x - t.x) / density, (y - t.y) / density)
                        Role.GRENADE -> {
                            // Arrastar gira a mira; sobre o X o arremesso é cancelado.
                            NativeBridge.addLook((x - t.x) / density, (y - t.y) / density)
                            val (cx, cy) = cancelSpot()
                            val now = hypot(x - cx, y - cy) < g(HudControl.GRENADE).r * 0.9f + d(10f)
                            if (now != t.cancel) {
                                t.cancel = now
                                haptic(false)
                            }
                        }
                        else -> Unit
                    }
                    t.x = x
                    t.y = y
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                val ids = if (event.actionMasked == MotionEvent.ACTION_CANCEL) touches.keys.toList() else listOf(event.getPointerId(event.actionIndex))
                for (id in ids) {
                    val t = touches.remove(id) ?: continue
                    when (t.role) {
                        Role.JOYSTICK -> {
                            stickActive = false
                            stickBaseX = stickHomeX; stickBaseY = stickHomeY; stickX = stickBaseX; stickY = stickBaseY
                            NativeBridge.setMove(0f, 0f)
                        }
                        Role.FIRE, Role.FIRE_LEFT -> if (touches.values.none { it.role == Role.FIRE || it.role == Role.FIRE_LEFT }) NativeBridge.setFire(false)
                        Role.GRENADE -> NativeBridge.setThrowAim(false, t.cancel || event.actionMasked == MotionEvent.ACTION_CANCEL)
                        else -> Unit
                    }
                }
            }
        }
        return true
    }

    /** Onde fica o X de cancelar o arremesso (acima do botão, ou abaixo se não couber). */
    private fun cancelSpot(): Pair<Float, Float> {
        val gc = g(HudControl.GRENADE)
        val up = gc.y - gc.r * 2.6f
        return gc.x to if (up > d(40f)) up else gc.y + gc.r * 2.6f
    }

    // ------------------------------------------------------------------ desenho

    override fun onDraw(canvas: Canvas) {
        if (editing) {
            drawEditor(canvas)
            return
        }
        NativeBridge.copyHud(hud)
        val currentWeapon = hud[Hud.WEAPON].toInt()
        if (aimWeapon != currentWeapon) {
            aimWeapon = currentWeapon
            aimStage = 0
            aiming = false
            NativeBridge.setAimStage(0)
        }
        val w = width.toFloat()
        val h = height.toFloat()
        val health = hud[Hud.HEALTH]
        val maxHealth = hud[Hud.MAX_HEALTH].coerceAtLeast(1f)
        if (lastHealth >= 0 && health < lastHealth - 2f) haptic(true)
        lastHealth = health

        drawStatusTint(canvas, w, h)
        drawVignette(canvas, w, h, health / maxHealth)
        drawDamageDirection(canvas, w, h)
        drawWorldLabels(canvas, w, h)
        if (island) {
            if (survival) drawFronts(canvas, w) else drawObjectives(canvas, w, h)
        }
        drawReticle(canvas, w, h)
        if (available(HudControl.MAP)) drawMinimap(canvas)
        drawStatus(canvas, health, maxHealth)
        if (survival) drawSurvivalTop(canvas, w, h)
        if (island) {
            if (!survival) drawNestBar(canvas, w)
        }
        drawEffects(canvas)
        drawFeed(canvas, w, h)
        drawTip(canvas, w, h)
        drawThrowAim(canvas, w, h)
        drawControls(canvas)
        if (showFps) {
            // canto inferior direito, com margem segura (fica abaixo da linha de 16 dp dos controles)
            textPaint.textAlign = Paint.Align.RIGHT
            textPaint.textSize = d(10f)
            textPaint.color = Palette.muted
            canvas.drawText("${hud[Hud.FPS].toInt()} FPS", w - d(10f), h - d(5f), textPaint)
            textPaint.textAlign = Paint.Align.CENTER
        }
        postInvalidateOnAnimation()
    }

    private fun drawVignette(canvas: Canvas, w: Float, h: Float, ratio: Float) {
        val flash = hud[Hud.DAMAGE_FLASH]
        val low = if (ratio < 0.3f) (0.35f + 0.25f * sin(System.currentTimeMillis() / 180.0).toFloat()) else 0f
        val a = (flash * 0.85f + low).coerceIn(0f, 1f)
        if (a <= 0.01f) return
        vignettePaint.alpha = (a * 255).toInt()
        canvas.drawRect(0f, 0f, w, h, vignettePaint)
    }

    /** Bordas coloridas: laranja queimando, vermelho sangrando, verde envenenado/lento. */
    private fun drawStatusTint(canvas: Canvas, w: Float, h: Float) {
        if (!island) return
        val pulse = 0.75f + 0.25f * sin(System.currentTimeMillis() / 140.0).toFloat()
        val states = listOf(
            Triple(hud[Hud.BURNING], Color.rgb(255, 120, 20), 0.75f),
            Triple(hud[Hud.BLEEDING], Color.rgb(200, 0, 0), 0.5f),
            Triple(maxOf(hud[Hud.POISON], hud[Hud.SLOWED]), Color.rgb(90, 220, 40), 0.55f),
        )
        for ((value, color, strength) in states) {
            if (value <= 0f) continue
            val shader = statusShaders.getOrPut(color) {
                RadialGradient(w / 2f, h / 2f, hypot(w / 2f, h / 2f), intArrayOf(Color.TRANSPARENT, Color.TRANSPARENT, color), floatArrayOf(0f, 0.6f, 1f), Shader.TileMode.CLAMP)
            }
            statusPaint.shader = shader
            statusPaint.alpha = (min(1f, value) * strength * pulse * 255).toInt().coerceIn(0, 255)
            canvas.drawRect(0f, 0f, w, h, statusPaint)
        }
    }

    private fun drawDamageDirection(canvas: Canvas, w: Float, h: Float) {
        val t = hud[Hud.DAMAGE_DIR_T]
        if (t <= 0f) return
        val angle = hud[Hud.DAMAGE_DIR]
        val r = min(w, h) * 0.32f
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(7f)
        p.strokeCap = Paint.Cap.ROUND
        p.color = Color.argb((min(1f, t) * 220).toInt(), 255, 60, 50)
        rect.set(w / 2 - r, h / 2 - r, w / 2 + r, h / 2 + r)
        val deg = Math.toDegrees(angle.toDouble()).toFloat() - 90f
        canvas.drawArc(rect, deg - 18f, 36f, false, p)
        p.style = Paint.Style.FILL
    }

    private fun drawWorldLabels(canvas: Canvas, w: Float, h: Float) {
        // Barras de vida dos zumbis atingidos e dos elite próximos.
        val bars = hud[Hud.BAR_COUNT].toInt()
        for (i in 0 until bars) {
            val o = Hud.BARS + i * Hud.BAR_STRIDE
            val x = hud[o] * w
            val y = hud[o + 1] * h
            val f = hud[o + 2]
            val a = hud[o + 3]
            val code = hud[o + 4].toInt()
            val elite = code >= 100
            val info = ZombieKinds.get(code % 100)
            val bw = d(if (elite) 52f else 38f)
            val bh = d(5f)
            p.color = Color.argb((a * 170).toInt(), 0, 0, 0)
            rect.set(x - bw / 2 - d(1f), y - d(1f), x + bw / 2 + d(1f), y + bh + d(1f))
            canvas.drawRoundRect(rect, d(2f), d(2f), p)
            p.color = when {
                elite -> Color.argb((a * 255).toInt(), 255, 200, 60)
                f > 0.5f -> Color.argb((a * 255).toInt(), 168, 255, 62)
                else -> Color.argb((a * 255).toInt(), 255, 90, 70)
            }
            rect.set(x - bw / 2, y, x - bw / 2 + bw * f, y + bh)
            canvas.drawRoundRect(rect, d(2f), d(2f), p)
            if (elite || (code % 100) != 0) {
                textPaint.textSize = d(10f)
                val label = (if (elite) "★ " else "") + info.name.uppercase()
                textPaint.color = Color.argb((a * 160).toInt(), 0, 0, 0)
                canvas.drawText(label, x + d(1f), y - d(4f) + d(1f), textPaint)
                textPaint.color = if (elite) Color.argb((a * 255).toInt(), 255, 210, 80) else Color.argb((a * 255).toInt(), Color.red(info.color), Color.green(info.color), Color.blue(info.color))
                canvas.drawText(label, x, y - d(4f), textPaint)
            }
        }
        // Números de dano/cura.
        val labels = hud[Hud.LABEL_COUNT].toInt()
        for (i in 0 until labels) {
            val o = Hud.LABELS + i * 5
            val kind = hud[o + 2].toInt()
            val value = hud[o + 3].toInt()
            val a = hud[o + 4]
            textPaint.textSize = d(if (kind == 2) 22f else 16f)
            val color = when (kind) { 2 -> Palette.gold; 3 -> Palette.lime; else -> Color.WHITE }
            val label = when (kind) { 3 -> "+$value"; 2 -> "$value!"; else -> "$value" }
            textPaint.color = Color.argb((a * 120).toInt(), 0, 0, 0)
            canvas.drawText(label, hud[o] * w + d(1.5f), hud[o + 1] * h + d(1.5f), textPaint)
            textPaint.color = Color.argb((a * 255).toInt(), Color.red(color), Color.green(color), Color.blue(color))
            canvas.drawText(label, hud[o] * w, hud[o + 1] * h, textPaint)
        }
    }

    private fun drawMarker(canvas: Canvas, x: Float, y: Float, color: Int, label: String, scale: Float, skull: Boolean) {
        val s = d(11f) * scale
        path.reset()
        path.moveTo(x, y - s); path.lineTo(x + s, y); path.lineTo(x, y + s); path.lineTo(x - s, y); path.close()
        p.color = color
        canvas.drawPath(path, p)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(2f)
        p.color = Color.WHITE
        canvas.drawPath(path, p)
        p.style = Paint.Style.FILL
        if (skull) {
            p.color = Color.WHITE
            canvas.drawCircle(x, y - d(1.5f) * scale, d(3.5f) * scale, p)
            canvas.drawRect(x - d(2f) * scale, y, x + d(2f) * scale, y + d(3.5f) * scale, p)
            p.color = color
            canvas.drawCircle(x - d(1.3f) * scale, y - d(1.5f) * scale, d(1f) * scale, p)
            canvas.drawCircle(x + d(1.3f) * scale, y - d(1.5f) * scale, d(1f) * scale, p)
        }
        textPaint.textSize = d(11f)
        textPaint.color = Color.argb(150, 0, 0, 0)
        canvas.drawText(label, x + d(1f), y + s + d(14f), textPaint)
        textPaint.color = Color.WHITE
        canvas.drawText(label, x, y + s + d(13f), textPaint)
    }

    /** Alvo da bússola e do rótulo: portal aberto > cristal caído > ninho vivo mais próximo > carga revelada. */
    private class Target(val kind: Int, val x: Float, val z: Float, val dist: Float, val marked: Boolean)

    private fun crystalCount(): Int = hud[Hud.CRYSTAL_COUNT].toInt().coerceIn(0, Hud.MAX_CRYSTALS)

    /**
     * Posição (px) do marcador 3D, ou null se está fora da faixa útil da tela. Quando o ninho está alinhado com a
     * câmera o marcador cairia em cima da mira: ele é deslocado para a esquerda, deixando o núcleo à vista.
     */
    private fun markerSpot(sx: Float, sy: Float, on: Boolean, w: Float, h: Float, wide: Boolean): Pair<Float, Float>? {
        val xs = if (wide) 0.08f..0.92f else 0.18f..0.82f
        val ys = if (wide) 0.12f..0.74f else 0.16f..0.66f
        if (!on || sx !in xs || sy !in ys) return null
        var x = sx * w
        val y = sy * h
        if (abs(x - w / 2) < d(70f) && abs(y - h / 2) < d(60f)) x = w / 2 - d(96f)
        return x to y
    }

    /**
     * Ninhos, cristais caídos e portal: marcador 3D só na área central (longe dos botões) e uma bússola no topo
     * com a direção de todos os objetivos — nada de setas na borda por cima dos controles.
     */
    private fun drawObjectives(canvas: Canvas, w: Float, h: Float) {
        val n = hud[Hud.OBJ_COUNT].toInt().coerceIn(0, 4)
        val px = hud[Hud.PLAYER_X]
        val pz = hud[Hud.PLAYER_Z]
        val pulse = 1f + 0.12f * sin(System.currentTimeMillis() / 200.0).toFloat()
        var nest: Target? = null
        var portal: Target? = null
        var crystal: Target? = null
        for (i in 0 until n) {
            val o = Hud.OBJ + i * Hud.OBJ_STRIDE
            val state = hud[o + 2].toInt()
            if (state != 1 && state != 4) continue          // destruído e portal fechado não têm marcador
            val dist = hypot(hud[o] - px, hud[o + 1] - pz)
            val spot = markerSpot(hud[o + 3], hud[o + 4], hud[o + 5] > 0.5f, w, h, false)
            if (spot != null) {
                val label = (if (state == 4) "PORTAL " else "NINHO ") + formatDist(dist)
                drawMarker(canvas, spot.first, spot.second, if (state == 4) Palette.purple else NEST_COLOR, label, pulse, state == 1)
            }
            val tg = Target(if (state == 4) KIND_PORTAL else KIND_NEST, hud[o], hud[o + 1], dist, spot != null)
            if (state == 4) portal = tg else if (nest == null || dist < nest.dist) nest = tg
        }
        for (k in 0 until crystalCount()) {
            val o = Hud.CRYSTAL_ITEMS + k * Hud.CRYSTAL_STRIDE
            val dist = hypot(hud[o] - px, hud[o + 1] - pz)
            val spot = markerSpot(hud[o + 2], hud[o + 3], hud[o + 4] > 0.5f, w, h, true)
            if (spot != null) drawMarker(canvas, spot.first, spot.second, Palette.cyan, "CRISTAL " + formatDist(dist), pulse, false)
            if (crystal == null || dist < crystal.dist) crystal = Target(KIND_CRYSTAL, hud[o], hud[o + 1], dist, spot != null)
        }
        // Caixa perdida: a carga só tem marcador depois de revelada (estado 2); antes disso o chip dá a dica de distância.
        var cargo: Target? = null
        if (hud[Hud.CT_ID] > 0.5f && hud[Hud.CT_CARGO_STATE].toInt() == 2) {
            val dist = hypot(hud[Hud.CT_CARGO_X] - px, hud[Hud.CT_CARGO_Z] - pz)
            val spot = markerSpot(hud[Hud.CT_CARGO_SX], hud[Hud.CT_CARGO_SY], hud[Hud.CT_CARGO_ON] > 0.5f, w, h, true)
            if (spot != null) drawMarker(canvas, spot.first, spot.second, CARGO_COLOR, "CARGA " + formatDist(dist), pulse, false)
            cargo = Target(KIND_CARGO, hud[Hud.CT_CARGO_X], hud[Hud.CT_CARGO_Z], dist, spot != null)
        }
        // Portal aberto > (cristal caído ou ninho vivo); a carga revelada e não coletada vence quando está mais perto.
        val base = portal ?: crystal ?: nest
        val pick = if (cargo != null && (base == null || (portal == null && cargo.dist < base.dist))) cargo else base
        drawCompass(canvas, w, n, px, pz, pick)
    }

    private fun wrapAngle(a: Float): Float {
        var x = a
        while (x > PI) x -= TAU
        while (x < -PI) x += TAU
        return x
    }

    /**
     * Bússola estreita sob a barra superior. A distância do alvo aparece UMA vez: no marcador do mundo
     * ("NINHO 276 m") quando ele está visível; senão no rótulo abaixo da bússola.
     * Alvo = portal aberto > cristal caído > ninho vivo mais próximo; a carga revelada (Caixa perdida) vence quando
     * está mais perto que esse alvo (e o portal ainda não abriu).
     */
    private fun drawCompass(canvas: Canvas, w: Float, n: Int, px: Float, pz: Float, target: Target?) {
        val yaw = hud[Hud.CAM_YAW]
        val cw = minOf(d(220f), w * 0.36f)
        val ch = d(16f)
        val top = d(COMPASS_TOP_DP)
        val left = w / 2 - cw / 2
        val half = (PI / 2).toFloat() // a bússola mostra 90° para cada lado
        fun xFor(rel: Float) = w / 2 + rel / half * (cw / 2 - d(8f))
        rect.set(left, top, left + cw, top + ch)
        p.color = PANEL
        canvas.drawRoundRect(rect, d(3f), d(3f), p)
        canvas.save()
        canvas.clipRect(rect)
        val names = arrayOf("N", "L", "S", "O")
        textPaint.textSize = d(10f)
        for (k in 0 until 24) {
            val rel = wrapAngle(k * TAU / 24f - yaw)
            if (abs(rel) > half) continue
            val x = xFor(rel)
            if (k % 6 == 0) {
                textPaint.color = if (k == 0) Palette.red else Color.WHITE
                canvas.drawText(names[k / 6], x, top + d(14f), textPaint)
            } else {
                p.color = Color.argb(120, 255, 255, 255)
                val th = if (k % 2 == 0) d(6f) else d(3f)
                canvas.drawRect(x - d(0.7f), top + ch - th - d(2f), x + d(0.7f), top + ch - d(2f), p)
            }
        }
        val cy = top + ch / 2
        fun mark(wx: Float, wz: Float, color: Int, isTarget: Boolean) {
            val rel = wrapAngle(atan2(wx - px, -(wz - pz)) - yaw)
            p.color = color
            val s = d(if (isTarget) 7f else 5f)
            if (abs(rel) <= half) {
                val x = xFor(rel)
                path.reset()
                path.moveTo(x, cy - s); path.lineTo(x + s, cy); path.lineTo(x, cy + s); path.lineTo(x - s, cy); path.close()
                canvas.drawPath(path, p)
            } else {
                // Fora do campo da bússola: triângulo na ponta indicando para que lado virar.
                val right = rel > 0
                val x = if (right) left + cw - d(6f) else left + d(6f)
                path.reset()
                if (right) { path.moveTo(x + s * 0.6f, cy); path.lineTo(x - s * 0.6f, cy - s); path.lineTo(x - s * 0.6f, cy + s) }
                else { path.moveTo(x - s * 0.6f, cy); path.lineTo(x + s * 0.6f, cy - s); path.lineTo(x + s * 0.6f, cy + s) }
                path.close()
                canvas.drawPath(path, p)
            }
        }
        for (i in 0 until n) {
            val o = Hud.OBJ + i * Hud.OBJ_STRIDE
            val state = hud[o + 2].toInt()
            if (state == 2) continue
            val color = when (state) { 4 -> Palette.purple; 3 -> Color.argb(150, 150, 120, 210); else -> NEST_COLOR }
            val isTarget = target != null && target.x == hud[o] && target.z == hud[o + 1]
            mark(hud[o], hud[o + 1], color, isTarget)
        }
        for (k in 0 until crystalCount()) {
            val o = Hud.CRYSTAL_ITEMS + k * Hud.CRYSTAL_STRIDE
            val isTarget = target != null && target.kind == KIND_CRYSTAL && target.x == hud[o] && target.z == hud[o + 1]
            mark(hud[o], hud[o + 1], Palette.cyan, isTarget)
        }
        if (hud[Hud.CT_ID] > 0.5f && hud[Hud.CT_CARGO_STATE].toInt() == 2) {
            val isTarget = target != null && target.kind == KIND_CARGO
            mark(hud[Hud.CT_CARGO_X], hud[Hud.CT_CARGO_Z], CARGO_COLOR, isTarget)
        }
        canvas.restore()
        p.color = Color.WHITE
        canvas.drawRect(w / 2 - d(1f), top - d(2f), w / 2 + d(1f), top + ch + d(2f), p)
        if (target != null) {
            val name = when (target.kind) { KIND_CRYSTAL -> "CRISTAL"; KIND_PORTAL -> "PORTAL"; KIND_CARGO -> "CARGA"; else -> "PRÓXIMO NINHO" }
            val label = if (target.marked) name else "$name ${formatDist(target.dist)}"
            // Texto branco fino com sombra, como os rótulos da referência; a cor do alvo fica só no marcador.
            val face = textPaint.typeface
            textPaint.typeface = Fonts.bold
            textPaint.textSize = d(if (target.marked) 9.5f else 11f)
            textPaint.color = Color.WHITE
            textPaint.setShadowLayer(d(2f), d(1f), d(1f), Color.argb(200, 0, 0, 0))
            canvas.drawText(label, w / 2, top + ch + d(13f), textPaint)
            textPaint.clearShadowLayer()
            textPaint.typeface = face
        }
    }

    private fun formatDist(m: Float): String = if (m >= 1000f) "%.1f km".format(m / 1000f) else "${m.toInt()} m"

    private fun drawNestBar(canvas: Canvas, w: Float) {
        val hp = hud[Hud.NEST_HP]
        if (hp < 0f) return
        val bw = minOf(d(240f), w * 0.4f)
        val top = d(76f)
        rect.set(w / 2 - bw / 2 - d(6f), top, w / 2 + bw / 2 + d(6f), top + d(30f))
        p.color = Color.argb(210, 40, 8, 48)
        canvas.drawRoundRect(rect, d(10f), d(10f), p)
        textPaint.textSize = d(10f)
        textPaint.color = Color.rgb(230, 170, 255)
        canvas.drawText("NÚCLEO DO NINHO", w / 2, top + d(12f), textPaint)
        rect.set(w / 2 - bw / 2, top + d(17f), w / 2 + bw / 2, top + d(25f))
        p.color = Color.argb(160, 0, 0, 0)
        canvas.drawRoundRect(rect, d(4f), d(4f), p)
        rect.right = rect.left + bw * hp.coerceIn(0f, 1f)
        p.color = Color.rgb(200, 70, 230)
        canvas.drawRoundRect(rect, d(4f), d(4f), p)
    }


    /**
     * Mira do template Multiplayer FPS (InGameHUD): cada arma tem o seu tipo (cruz no fuzil, círculo aberto na
     * escopeta). Fica cinza recarregando ou entre disparos e branca por 100 ms logo depois do tiro; recarregando,
     * um anel em volta enche conforme o progresso (o reticle_reload_circle do template).
     */
    private fun drawReticle(canvas: Canvas, w: Float, h: Float) {
        val cx = w / 2
        val cy = h / 2
        val locked = hud[Hud.TARGET_LOCKED] > 0.5f
        val boosted = hud[Hud.BOOST] > 0f
        val ready = hud[Hud.WEAPON_READY] > 0.5f
        val color = when {
            !ready -> Color.rgb(140, 140, 140)
            locked -> Palette.red
            boosted -> Color.rgb(255, 160, 60)
            else -> Color.WHITE
        }
        // As texturas do template têm 256 px; aqui a mira inteira ocupa 60 dp.
        val size = d(60f)
        val k = size / 256f
        val type = WeaponSpecs.reticle(hud[Hud.WEAPON].toInt())
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.ROUND
        for (pass in 0..1) {
            // Primeiro a sombra (o brilho escuro das texturas), depois a linha.
            if (pass == 0) { p.color = Color.argb(120, 0, 0, 0); p.strokeWidth = d(2.8f) }
            else { p.color = color; p.strokeWidth = d(1.6f) }
            when (type) {
                Reticle.CROSS -> {
                    canvas.drawLine(cx - 55 * k, cy, cx - 28 * k, cy, p)
                    canvas.drawLine(cx + 28 * k, cy, cx + 55 * k, cy, p)
                    canvas.drawLine(cx, cy - 55 * k, cx, cy - 28 * k, p)
                    canvas.drawLine(cx, cy + 28 * k, cx, cy + 55 * k, p)
                }
                Reticle.TCROSS -> {
                    canvas.drawLine(cx - 112 * k, cy, cx - 56 * k, cy, p)
                    canvas.drawLine(cx + 56 * k, cy, cx + 112 * k, cy, p)
                    canvas.drawLine(cx, cy + 47 * k, cx, cy + 72 * k, p)
                }
                Reticle.OPEN_CIRCULAR -> {
                    // Quatro arcos de 50° em cima, embaixo e dos lados.
                    rect.set(cx - 90 * k, cy - 90 * k, cx + 90 * k, cy + 90 * k)
                    for (q in 0 until 4) canvas.drawArc(rect, q * 90f - 25f, 50f, false, p)
                }
                Reticle.CIRCULAR_CROSS -> {
                    canvas.drawCircle(cx, cy, 108 * k, p)
                    canvas.drawLine(cx - 108 * k, cy, cx - 88 * k, cy, p)
                    canvas.drawLine(cx + 88 * k, cy, cx + 108 * k, cy, p)
                    canvas.drawLine(cx, cy - 108 * k, cx, cy - 88 * k, p)
                    canvas.drawLine(cx, cy + 88 * k, cx, cy + 108 * k, p)
                }
            }
        }
        val reload = hud[Hud.RELOAD]
        if (reload >= 0f) {
            rect.set(cx - 70 * k, cy - 70 * k, cx + 70 * k, cy + 70 * k)
            p.strokeWidth = d(3f)
            p.color = Color.argb(90, 255, 255, 255)
            canvas.drawArc(rect, 0f, 360f, false, p)
            p.color = Palette.gold
            canvas.drawArc(rect, -90f, 360f * reload.coerceIn(0f, 1f), false, p)
        }
        p.style = Paint.Style.FILL
        p.strokeCap = Paint.Cap.SQUARE
        p.color = color
        canvas.drawCircle(cx, cy, d(1.4f), p)
        val outer = 56 * k
        if (locked) {
            val f = hud[Hud.TARGET_HP]
            val bw = d(46f)
            p.color = Color.argb(160, 0, 0, 0)
            rect.set(cx - bw / 2, cy + outer + d(6f), cx + bw / 2, cy + outer + d(10f))
            canvas.drawRect(rect, p)
            p.color = Palette.red
            rect.right = rect.left + bw * f
            canvas.drawRect(rect, p)
        }
        val hit = hud[Hud.HITMARK]
        if (hit > 0f) {
            val hs = hud[Hud.HEADSHOT_MARK] > 0.5f
            p.color = if (hs) Palette.gold else Color.WHITE
            p.alpha = (hit * 255).toInt().coerceIn(0, 255)
            p.strokeWidth = d(if (hs) 3f else 2.2f)
            val a = d(7f)
            val b = d(15f)
            canvas.drawLine(cx - b, cy - b, cx - a, cy - a, p)
            canvas.drawLine(cx + b, cy - b, cx + a, cy - a, p)
            canvas.drawLine(cx - b, cy + b, cx - a, cy + a, p)
            canvas.drawLine(cx + b, cy + b, cx + a, cy + a, p)
            p.alpha = 255
        }
    }

    private fun drawMinimap(canvas: Canvas) {
        val left = mapX - mapSize / 2
        val top = mapY - mapSize / 2
        rect.set(left, top, left + mapSize, top + mapSize)
        p.color = Color.argb(200, 8, 14, 26)
        canvas.drawRoundRect(rect, d(MAP_CORNER), d(MAP_CORNER), p)
        val bmp = minimap
        val px = hud[Hud.PLAYER_X]
        val pz = hud[Hud.PLAYER_Z]
        val yawDeg = Math.toDegrees(hud[Hud.CAM_YAW].toDouble()).toFloat()
        val scale = mapSize / 80f // 80 blocos visíveis
        canvas.save()
        path.reset()
        path.addRoundRect(rect, d(MAP_CORNER), d(MAP_CORNER), Path.Direction.CW)
        canvas.clipPath(path)
        canvas.translate(mapX, mapY)
        canvas.rotate(-yawDeg)
        canvas.scale(scale, scale)
        canvas.translate(-px, -pz)
        if (bmp != null) canvas.drawBitmap(bmp, minimapOriginX, minimapOriginZ, null)
        val alive = hud[Hud.ALIVE].toInt()
        val n = hud[Hud.ENEMY_COUNT].toInt()
        for (i in 0 until n) {
            val o = Hud.ENEMIES + i * 3
            val ex = hud[o]
            val ez = hud[o + 1]
            val alerted = hud[o + 2] > 0.5f
            if (alive > 3 && !alerted && hypot(ex - px, ez - pz) > 30f) continue
            p.color = if (alerted) Palette.red else Color.rgb(255, 160, 60)
            canvas.drawCircle(ex, ez, 1.8f, p)
        }
        if (island) {
            val objs = hud[Hud.OBJ_COUNT].toInt()
            for (i in 0 until objs) {
                val o = Hud.OBJ + i * Hud.OBJ_STRIDE
                val state = hud[o + 2].toInt()
                p.color = when (state) { 1 -> Color.rgb(220, 70, 220); 2 -> Color.argb(150, 120, 120, 120); 4 -> Palette.purple; else -> Color.argb(170, 140, 110, 200) }
                canvas.drawCircle(hud[o], hud[o + 1], 3.2f, p)
            }
            for (k in 0 until crystalCount()) {
                val o = Hud.CRYSTAL_ITEMS + k * Hud.CRYSTAL_STRIDE
                p.color = Color.WHITE
                canvas.drawCircle(hud[o], hud[o + 1], 3.6f, p)
                p.color = Palette.cyan
                canvas.drawCircle(hud[o], hud[o + 1], 2.6f, p)
            }
            if (hud[Hud.SUPPLY_X] >= 0f) {
                p.color = Color.rgb(120, 255, 90)
                canvas.drawCircle(hud[Hud.SUPPLY_X], hud[Hud.SUPPLY_Z], 2.6f, p)
            }
        } else if (hud[Hud.PORTAL_OPEN] > 0.5f) {
            p.color = Palette.purple
            canvas.drawCircle(hud[Hud.PORTAL_X], hud[Hud.PORTAL_Z], 3f, p)
        }
        canvas.restore()
        // jogador (seta para cima = direção da câmera)
        path.reset()
        val s = d(7f)
        path.moveTo(mapX, mapY - s); path.lineTo(mapX + s * 0.7f, mapY + s * 0.7f); path.lineTo(mapX, mapY + s * 0.3f); path.lineTo(mapX - s * 0.7f, mapY + s * 0.7f); path.close()
        p.color = Color.WHITE
        canvas.drawPath(path, p)
        // Moldura fina e clara, cantos quase retos (E01 da referência), sem rótulo.
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(1.5f)
        p.color = RING
        canvas.drawRoundRect(rect, d(MAP_CORNER), d(MAP_CORNER), p)
        p.style = Paint.Style.FILL
    }

    /** Barra de vida (E26 da referência): "HP x/y" e, à direita dele, uma barra branca fina perto da base. */
    private fun drawStatus(canvas: Canvas, health: Float, maxHealth: Float) {
        val w = width.toFloat()
        val trackL = w * HP_BAR_LEFT
        val trackR = w * HP_BAR_RIGHT
        val barH = d(3.6f)
        val trackTop = statusBaselineCenter() - barH / 2f
        val ratio = (health / maxHealth).coerceIn(0f, 1f)
        val label = "HP ${health.toInt()}/${maxHealth.toInt()}"
        textPaint.textAlign = Paint.Align.RIGHT
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(11.5f)
        textPaint.color = Color.WHITE
        textPaint.setShadowLayer(d(2f), d(1f), d(1f), Color.rgb(34, 34, 34))
        canvas.drawText(label, trackL - d(3f), statusBaselineCenter() + d(4f), textPaint)
        textPaint.clearShadowLayer()
        textPaint.textAlign = Paint.Align.CENTER
        p.style = Paint.Style.FILL
        p.color = Color.argb(110, 0, 0, 0)
        canvas.drawRect(trackL, trackTop, trackR, trackTop + barH, p)
        p.color = if (ratio < 0.3f) Palette.danger else Color.WHITE
        canvas.drawRect(trackL, trackTop, trackL + (trackR - trackL) * ratio, trackTop + barH, p)
    }

    /** Altura (px) do centro da linha da barra de vida. */
    private fun statusBaselineCenter(): Float = height - d(14.7f)

    /** Efeitos ativos (texto com tempo): coluna à direita, abaixo do painel de armas e do chip de contrato. */
    private fun drawEffects(canvas: Canvas) {
        if (island) {
            var y = weaponRect().bottom + d(19f)
            textPaint.textAlign = Paint.Align.LEFT
            textPaint.typeface = Fonts.bold
            fun chip(label: String, seconds: Float, color: Int) {
                if (seconds <= 0f) return
                val txt = "$label ${seconds.toInt()}s"
                textPaint.textSize = d(10.5f)
                val tw = textPaint.measureText(txt) + d(14f)
                val x = width - d(8f) - tw
                rect.set(x, y - d(13f), x + tw, y + d(5f))
                p.color = PANEL
                canvas.drawRoundRect(rect, d(3f), d(3f), p)
                textPaint.color = color
                canvas.drawText(txt, x + d(7f), y, textPaint)
                y += d(22f)
            }
            chip("🔥 QUEIMANDO", hud[Hud.BURNING], Color.rgb(255, 150, 60))
            chip("🩸 SANGRANDO", hud[Hud.BLEEDING], Color.rgb(255, 90, 90))
            chip("☠ VENENO", hud[Hud.POISON], Color.rgb(140, 255, 90))
            chip("💥 MUNIÇÃO INCENDIÁRIA", hud[Hud.BOOST], Color.rgb(255, 180, 70))
            chip("⚡ ADRENALINA", hud[Hud.ADRENALINE], Color.rgb(90, 230, 255))
        }
        textPaint.textAlign = Paint.Align.CENTER
        textPaint.typeface = Fonts.title
    }

    /** Corta o texto com reticências para caber em [maxW] (usa o tamanho de texto atual do textPaint). */
    private fun fit(text: String, maxW: Float): String {
        if (maxW <= 0f) return ""
        if (textPaint.measureText(text) <= maxW) return text
        var t = text
        while (t.isNotEmpty() && textPaint.measureText("$t…") > maxW) t = t.dropLast(1)
        return "$t…"
    }

    private fun drawHeart(canvas: Canvas, cx: Float, cy: Float, px: Float) {
        val rows = arrayOf(".XX.XX.", "XXXXXXX", "XXXXXXX", ".XXXXX.", "..XXX..", "...X...")
        p.color = Palette.red
        for ((r, row) in rows.withIndex()) for ((c, ch) in row.withIndex()) {
            if (ch == 'X') canvas.drawRect(cx + (c - 3.5f) * px, cy + (r - 3f) * px, cx + (c - 2.5f) * px, cy + (r - 2f) * px, p)
        }
    }

    private class BarCell(
        val label: String,
        val value: String,
        val color: Int,
        val icon: String?,
        val state: PillState?,
        val attention: Boolean = false,
    )

    /** Objetivo lido do HUD no máximo a cada 100 ms (as pílulas não precisam de mais que isso). */
    private fun currentObjective(now: Long): Objective {
        if (now - objectiveAt >= 100L) {
            objective = Objective.from(hud)
            objectiveAt = now
        }
        return objective
    }

    /**
     * Barra superior: as três pílulas do objetivo na ordem NINHOS → CRISTAIS → PORTAL (a atual destacada, as
     * concluídas riscadas, a que pede ação pulsando), mais TEMPO e ABATES. O texto longo do objetivo
     * (ObjectiveText.SENTENCE_UPPER) só aparece em pausa, mapa e carregamento. "◆" é só de diamantes (moeda).
     */
    private fun drawObjective(canvas: Canvas, w: Float) {
        val now = System.currentTimeMillis()
        val obj = if (island) currentObjective(now) else Objective.INACTIVE
        val portal = obj.portalOpen
        val pillColors = intArrayOf(NEST_COLOR, Palette.cyan, Palette.purple)
        val pillIcons = arrayOf(BrandIcons.NINHO, BrandIcons.CRISTAL, BrandIcons.PORTAL_EXTRACAO)
        val core = ArrayList<BarCell>()
        if (obj.active) {
            for ((i, pill) in obj.pills.withIndex()) {
                core.add(BarCell(pill.label, pill.value.ifEmpty { "FECHADO" }, pillColors[i], pillIcons[i], pill.state, pill.attention))
            }
        }
        val elapsed = hud[Hud.ELAPSED].toInt()
        core.add(BarCell("TEMPO", "%02d:%02d".format(elapsed / 60, elapsed % 60), Palette.muted, null, null))
        val kills = BarCell("ABATES", "${hud[Hud.KILLS].toInt()}", Palette.muted, null, null)

        val maxW = (w - 2f * d(166f)).coerceAtLeast(d(200f))
        val chevron = d(9f)
        fun cellWidth(c: BarCell, icons: Boolean): Float {
            textPaint.textSize = d(8f)
            val lw = textPaint.measureText(c.label)
            textPaint.textSize = d(12f)
            val vw = textPaint.measureText(c.value)
            return maxOf(lw, vw) + d(16f) + if (icons && c.icon != null) d(17f) else 0f
        }
        fun total(cells: List<BarCell>, icons: Boolean): Float {
            var t = 0f
            for ((i, c) in cells.withIndex()) {
                t += cellWidth(c, icons)
                if (i < cells.size - 1 && c.state != null && cells[i + 1].state != null) t += chevron
            }
            return t
        }
        var cells: List<BarCell> = core + kills
        var icons = true
        if (total(cells, icons) > maxW) cells = core
        if (total(cells, icons) > maxW) icons = false
        val widths = FloatArray(cells.size) { cellWidth(cells[it], icons) }
        val totalW = total(cells, icons)

        val top = d(TOP_BAR_TOP_DP)
        val bh = d(30f)
        val left = w / 2 - totalW / 2
        rect.set(left, top, left + totalW, top + bh)
        p.color = if (portal) Color.argb(220, 60, 24, 110) else Color.argb(205, 8, 14, 26)
        canvas.drawRoundRect(rect, d(12f), d(12f), p)
        var x = left
        for ((i, c) in cells.withIndex()) {
            val cw = widths[i]
            val cx = x + cw / 2
            val st = c.state
            if (st != null) {
                val inset = d(2f)
                rect.set(x + inset, top + inset, x + cw - inset, top + bh - inset)
                if (st == PillState.CURRENT) {
                    p.color = Color.argb(46, Color.red(c.color), Color.green(c.color), Color.blue(c.color))
                    canvas.drawRoundRect(rect, d(10f), d(10f), p)
                    p.style = Paint.Style.STROKE
                    p.strokeWidth = d(1.3f)
                    p.color = Color.argb(200, Color.red(c.color), Color.green(c.color), Color.blue(c.color))
                    canvas.drawRoundRect(rect, d(10f), d(10f), p)
                    p.style = Paint.Style.FILL
                }
                if (c.attention) {
                    // Cristal caído e ainda no chão: contorno ciano pulsando.
                    val a = (0.45f + 0.45f * sin(now / 160.0).toFloat()).coerceIn(0f, 1f)
                    p.style = Paint.Style.STROKE
                    p.strokeWidth = d(1.8f)
                    p.color = Color.argb((a * 255).toInt(), 60, 230, 255)
                    canvas.drawRoundRect(rect, d(10f), d(10f), p)
                    p.style = Paint.Style.FILL
                }
            }
            val hasIcon = icons && c.icon != null
            val textCx = if (hasIcon) cx + d(8.5f) else cx
            val dim = st == PillState.DONE
            val accent = when (st) {
                PillState.CURRENT -> c.color
                PillState.DONE -> Color.argb(170, Color.red(c.color), Color.green(c.color), Color.blue(c.color))
                PillState.TODO -> Palette.muted
                null -> c.color
            }
            if (hasIcon) {
                val isz = d(14f)
                BrandIcons.draw(canvas, context, c.icon!!, x + d(8f), top + (bh - isz) / 2, isz, accent, if (st == PillState.TODO) 150 else 255)
            }
            textPaint.textSize = d(8f)
            textPaint.color = Palette.muted
            canvas.drawText(c.label, textCx, top + d(12f), textPaint)
            textPaint.textSize = d(12f)
            textPaint.color = if (st == null) (if (c === kills) Palette.muted else Palette.text) else accent
            val ty = top + d(25f)
            canvas.drawText(c.value, textCx, ty, textPaint)
            if (dim) {
                // Concluída: risco no valor.
                val vw = textPaint.measureText(c.value)
                p.color = Color.argb(200, 220, 230, 245)
                canvas.drawRect(textCx - vw / 2, ty - d(4.5f), textCx + vw / 2, ty - d(3.3f), p)
            }
            x += cw
            if (i < cells.size - 1 && st != null && cells[i + 1].state != null) {
                textPaint.textSize = d(12f)
                textPaint.color = Palette.muted
                canvas.drawText("›", x + chevron / 2, top + d(21f), textPaint)
                x += chevron
            }
        }
        if (island && hud[Hud.NEST_HP] < 0f) {
            textPaint.textSize = d(10f)
            textPaint.color = Color.argb(200, 255, 220, 120)
            val chests = hud[Hud.CHESTS].toInt()
            val gems = hud[Hud.CHEST_DIAMONDS].toInt()
            canvas.drawText("BAÚS $chests (◆$gems)   •   ${formatDist(hud[Hud.DISTANCE])} percorridos", w / 2, d(88f), textPaint)
        }
    }

    /**
     * Só UM aviso de combate por vez (o mais recente), acima do centro; o COMBO fica abaixo dele, menor.
     * Nada é desenhado na faixa da mira (h/2).
     */
    private fun drawFeed(canvas: Canvas, w: Float, h: Float) {
        val now = System.currentTimeMillis()
        val y = d(CombatFeed.noticeBaselineDp(h / d(1f)))
        val notice = combatFeed.current(now)
        val suppress = bannerShown
        bannerShown = false
        if (notice != null && !suppress) {
            val age = (now - notice.shownAtMs) / CombatFeed.DURATION_MS.toFloat()
            val a = if (age < 0.8f) 1f else ((1f - age) / 0.2f).coerceIn(0f, 1f)
            val scale = if (age < 0.08f) 1f + (0.08f - age) * 4f else 1f
            // Frases longas (o objetivo inteiro) encolhem para caber na largura da tela.
            var base = d(20f)
            textPaint.textSize = base
            val noticeW = textPaint.measureText(notice.text)
            if (noticeW > w * 0.92f) base = base * (w * 0.92f) / noticeW
            textPaint.textSize = base * scale
            textPaint.color = Color.argb((a * 140).toInt(), 0, 0, 0)
            canvas.drawText(notice.text, w / 2 + d(2f), y + d(2f), textPaint)
            textPaint.color = Color.argb((a * 255).toInt(), Color.red(notice.color), Color.green(notice.color), Color.blue(notice.color))
            canvas.drawText(notice.text, w / 2, y, textPaint)
        }
        val combo = hud[Hud.COMBO].toInt()
        if (combo >= 2) {
            // À direita da mira (fora do braço de 48 dp), nunca sobre o aviso nem a barra do ninho.
            val cx = w / 2 + d(100f)
            val cy = h / 2 + d(4f)
            textPaint.textSize = d(13f)
            textPaint.color = Color.argb(150, 0, 0, 0)
            canvas.drawText("COMBO x$combo", cx + d(1f), cy + d(1f), textPaint)
            textPaint.color = Palette.gold
            canvas.drawText("COMBO x$combo", cx, cy, textPaint)
        }
    }

    private fun drawTip(canvas: Canvas, w: Float, h: Float) {
        val (text, age) = tipState.current(System.currentTimeMillis()) ?: return
        val a = if (age < 4300) 1f else ((5000 - age) / 700f).coerceIn(0f, 1f)
        textPaint.textSize = d(13f)
        val tw = textPaint.measureText(text) + d(30f)
        val y = h * 0.72f
        rect.set(w / 2 - tw / 2, y - d(20f), w / 2 + tw / 2, y + d(10f))
        p.color = Color.argb((a * 210).toInt(), 20, 12, 30)
        canvas.drawRoundRect(rect, d(12f), d(12f), p)
        textPaint.color = Color.argb((a * 255).toInt(), 255, 214, 110)
        canvas.drawText(text, w / 2, y, textPaint)
    }

    /**
     * Botão no estilo da referência: disco preto translúcido e aro claro fino. Aceso ([active]) o fundo clareia
     * e o aro fica na cor [accent].
     */
    private fun drawButton(canvas: Canvas, x: Float, y: Float, r: Float, accent: Int, active: Boolean) {
        p.style = Paint.Style.FILL
        p.color = if (active) Color.argb(80, 255, 255, 255) else BUTTON_FILL
        canvas.drawCircle(x, y, r, p)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(if (active) 2.2f else 1.4f)
        p.color = if (active) accent else RING
        canvas.drawCircle(x, y, r - p.strokeWidth / 2f, p)
        p.style = Paint.Style.FILL
    }

    /** Contador pequeno no canto inferior direito do botão (kit médico, granadas), como o "0" da referência. */
    private fun drawCount(canvas: Canvas, gc: Geo, count: Int) {
        textPaint.textAlign = Paint.Align.LEFT
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(11f) * k2(gc)
        textPaint.color = if (count > 0) Color.WHITE else Color.argb(170, 255, 255, 255)
        textPaint.setShadowLayer(d(1.5f), 0f, d(1f), Color.BLACK)
        canvas.drawText("$count", gc.x + gc.r * 0.6f, gc.y + gc.r * 0.78f, textPaint)
        textPaint.clearShadowLayer()
        textPaint.textAlign = Paint.Align.CENTER
        textPaint.typeface = Fonts.title
    }

    /** Fator de tamanho do texto pelo raio do botão (botões de 24 dp = 1). */
    private fun k2(gc: Geo): Float = (gc.r / d(24f)).coerceIn(0.8f, 1.4f)

    private fun drawControls(canvas: Canvas) {
        val alpha = opacity
        val save = if (alpha < 0.99f) canvas.saveLayerAlpha(0f, 0f, width.toFloat(), height.toFloat(), (alpha * 255).toInt()) else -1
        if (editing) for (c in HudControl.entries) if (!g(c).visible) drawGhost(canvas, c)
        drawJoystick(canvas)
        val firing = hud[Hud.FIRING] > 0.5f || touches.values.any { it.role == Role.FIRE || it.role == Role.FIRE_LEFT }
        if (available(HudControl.FIRE)) drawFireButton(canvas, HudControl.FIRE, firing, true)
        if (available(HudControl.FIRE_LEFT)) drawFireButton(canvas, HudControl.FIRE_LEFT, firing, false)
        if (available(HudControl.AIM)) drawAim(canvas)
        if (available(HudControl.JUMP)) drawJump(canvas)
        if (available(HudControl.CROUCH)) drawCrouch(canvas)
        if (available(HudControl.SPRINT)) drawSprint(canvas)
        if (available(HudControl.RELOAD)) drawReload(canvas)
        if (available(HudControl.MELEE)) drawMelee(canvas)
        if (available(HudControl.GRENADE)) drawGrenade(canvas)
        if (available(HudControl.MEDKIT)) drawMedkit(canvas)
        if (available(HudControl.WEAPON)) drawWeaponPanel(canvas)
        if (available(HudControl.EMOTE)) drawEmote(canvas)
        if (available(HudControl.PAUSE)) drawPause(canvas)
        drawClock(canvas)
        if (available(HudControl.BAG)) drawBag(canvas)
        if (save >= 0) canvas.restoreToCount(save)
    }

    // ------------------------------------------------------------------ inventário, arremesso e sobrevivência

    private fun drawHotbar(canvas: Canvas) {
        val left = hotbarX - slotSize * 3f
        val top = hotbarY - slotSize / 2f
        val weaponSlot = hud[Hud.WEAPON_SLOT].toInt()
        val throwSlot = hud[Hud.THROW_SLOT].toInt()
        // Fileira de ícones discreta (E25 da referência): espaços escuros translúcidos com aro fino.
        for (i in 0 until 6) {
            val x = left + i * slotSize
            rect.set(x + d(2f), top + d(2f), x + slotSize - d(2f), top + slotSize - d(2f))
            p.color = BUTTON_FILL
            canvas.drawRoundRect(rect, d(5f), d(5f), p)
            val id = if (editing) listOf(Items.RIFLE, Items.SHOTGUN, Items.GRENADE, Items.BANDAGE, Items.MEDKIT, 0)[i] else hud[Hud.INV + i * 2].toInt()
            val count = if (editing) (if (i in 2..3) 3 else 1) else hud[Hud.INV + i * 2 + 1].toInt()
            val pressed = touches.values.any { it.role == Role.HOTBAR && it.slot == i }
            val equipped = id != 0 && i == weaponSlot && Items.isWeapon(id)
            if (equipped) {
                // arma em mãos: fundo um pouco mais claro (o aro dourado vem abaixo)
                p.color = Color.argb(50, 255, 255, 255)
                canvas.drawRoundRect(rect, d(5f), d(5f), p)
            }
            val border = when {
                pressed -> Color.WHITE
                id != 0 && i == weaponSlot && Items.isWeapon(id) -> Palette.gold
                id != 0 && i == throwSlot && Items.isThrowable(id) -> Color.rgb(255, 150, 60)
                else -> RING
            }
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(if (border == RING) 1.2f else 2.2f)
            p.color = border
            canvas.drawRoundRect(rect, d(5f), d(5f), p)
            p.style = Paint.Style.FILL
            if (id != 0 && count > 0) {
                ItemIcons.draw(canvas, id, rect.centerX(), rect.centerY(), rect.width() * 0.82f)
                if (count > 1) {
                    textPaint.textAlign = Paint.Align.RIGHT
                    textPaint.textSize = slotSize * 0.28f
                    textPaint.color = Color.argb(200, 0, 0, 0)
                    canvas.drawText("$count", rect.right - d(2f) + d(1f), rect.bottom - d(3f) + d(1f), textPaint)
                    textPaint.color = Color.WHITE
                    canvas.drawText("$count", rect.right - d(2f), rect.bottom - d(3f), textPaint)
                    textPaint.textAlign = Paint.Align.CENTER
                }
            }
        }
        // A arma em mãos e a munição ficam só no painel de armas (como na referência); aqui só a vibração da troca.
        val weapon = if (editing) Items.RIFLE else hud[Hud.WEAPON].toInt()
        if (weapon != lastWeapon && lastWeapon >= 0 && !editing) haptic(false)
        lastWeapon = weapon
    }

    private fun drawBag(canvas: Canvas) {
        val gc = g(HudControl.BAG)
        drawButton(canvas, gc.x, gc.y, gc.r, Palette.gold, false)
        val u = iconUnit(gc)
        // Mochila em silhueta branca: alça, corpo e bolso da frente recortado.
        p.color = ICON
        p.style = Paint.Style.STROKE
        p.strokeWidth = 3f * u
        canvas.drawArc(gc.x - 6f * u, gc.y - 19f * u, gc.x + 6f * u, gc.y - 7f * u, 180f, 180f, false, p)
        p.style = Paint.Style.FILL
        rect.set(gc.x - 13f * u, gc.y - 11f * u, gc.x + 13f * u, gc.y + 17f * u)
        canvas.drawRoundRect(rect, 7f * u, 7f * u, p)
        p.color = ICON_CUT
        rect.set(gc.x - 8f * u, gc.y + 3f * u, gc.x + 8f * u, gc.y + 13f * u)
        canvas.drawRoundRect(rect, 3f * u, 3f * u, p)
        canvas.drawRect(gc.x - 13f * u, gc.y - 2f * u, gc.x + 13f * u, gc.y - 0.5f * u, p)
    }

    /** Unidade dos ícones: o desenho ocupa cerca de ±24 unidades, ~62% do raio do botão. */
    private fun iconUnit(gc: Geo): Float = gc.r * 0.62f / 24f

    /** Durante a mira do arremesso: X para cancelar, instrução e distância no ponto de queda. */
    private fun drawThrowAim(canvas: Canvas, w: Float, h: Float) {
        if (hud[Hud.THROW_AIM] < 0.5f) return
        val (cx, cy) = cancelSpot()
        val r = g(HudControl.GRENADE).r * 0.9f
        val over = touches.values.any { it.role == Role.GRENADE && it.cancel }
        drawButton(canvas, cx, cy, r, Palette.red, over)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(3f)
        p.color = if (over) Palette.dark else Palette.red
        canvas.drawLine(cx - r * 0.4f, cy - r * 0.4f, cx + r * 0.4f, cy + r * 0.4f, p)
        canvas.drawLine(cx + r * 0.4f, cy - r * 0.4f, cx - r * 0.4f, cy + r * 0.4f, p)
        p.style = Paint.Style.FILL
        val kind = hud[Hud.THROW_KIND].toInt()
        val molotov = kind == Items.MOLOTOV
        val smoke = kind == Items.SMOKE
        val itemName = when {
            molotov -> "O MOLOTOV"
            smoke -> "A FUMAÇA"
            else -> "A GRANADA"
        }
        val text = if (over) "SOLTE PARA CANCELAR" else "SOLTE PARA LANÇAR $itemName • ARRASTE ATÉ ✕ PARA CANCELAR"
        textPaint.textSize = d(13f)
        val tw = textPaint.measureText(text) + d(24f)
        rect.set(w / 2 - tw / 2, h * 0.64f - d(18f), w / 2 + tw / 2, h * 0.64f + d(8f))
        p.color = Color.argb(190, 30, 14, 6)
        canvas.drawRoundRect(rect, d(12f), d(12f), p)
        textPaint.color = if (over) Palette.red else Color.rgb(255, 214, 120)
        canvas.drawText(text, w / 2, h * 0.64f, textPaint)
        if (hud[Hud.THROW_ONSCREEN] > 0.5f) {
            val x = hud[Hud.THROW_SX] * w
            val y = hud[Hud.THROW_SY] * h
            val label = "${hud[Hud.THROW_DIST].toInt()} m"
            textPaint.textSize = d(14f)
            textPaint.color = Color.argb(160, 0, 0, 0)
            canvas.drawText(label, x + d(1.5f), y - d(10f) + d(1.5f), textPaint)
            textPaint.color = when {
                molotov -> Color.rgb(255, 170, 80)
                smoke -> Color.rgb(205, 220, 230)
                else -> Color.rgb(255, 230, 120)
            }
            canvas.drawText(label, x, y - d(10f), textPaint)
        }
    }

    /** Sobrevivência: direções de onde a horda está vindo (setas vermelhas numa faixa). */
    private fun drawFronts(canvas: Canvas, w: Float) {
        val n = hud[Hud.FRONT_COUNT].toInt()
        if (n <= 0) return
        val yaw = hud[Hud.CAM_YAW]
        val cw = d(300f)
        val top = d(84f)
        val left = w / 2 - cw / 2
        rect.set(left, top, left + cw, top + d(16f))
        p.color = Color.argb(120, 40, 6, 6)
        canvas.drawRoundRect(rect, d(8f), d(8f), p)
        val half = (PI / 2).toFloat()
        for (i in 0 until n) {
            val rel = wrapAngle(hud[Hud.FRONTS + i] - yaw)
            val pulse = 0.75f + 0.25f * sin(System.currentTimeMillis() / 150.0).toFloat()
            p.color = Color.argb((255 * pulse).toInt(), 255, 70, 60)
            val cy = top + d(8f)
            val s = d(6f)
            if (abs(rel) <= half) {
                val x = w / 2 + rel / half * (cw / 2 - d(8f))
                path.reset()
                path.moveTo(x, cy + s); path.lineTo(x - s, cy - s); path.lineTo(x + s, cy - s); path.close()
                canvas.drawPath(path, p)
            } else {
                val right = rel > 0
                val x = if (right) left + cw - d(8f) else left + d(8f)
                path.reset()
                if (right) { path.moveTo(x + s, cy); path.lineTo(x - s * 0.6f, cy - s); path.lineTo(x - s * 0.6f, cy + s) }
                else { path.moveTo(x - s, cy); path.lineTo(x + s * 0.6f, cy - s); path.lineTo(x + s * 0.6f, cy + s) }
                path.close()
                canvas.drawPath(path, p)
            }
        }
        textPaint.textSize = d(9f)
        textPaint.color = Color.argb(200, 255, 140, 120)
        canvas.drawText("HORDA VINDO DAQUI", w / 2, top + d(26f), textPaint)
    }

    /** Sobrevivência: onda atual, barra de progresso com bandeira (estilo Plants vs Zombies), tempo e pontos. */
    private fun drawSurvivalTop(canvas: Canvas, w: Float, h: Float) {
        val wave = hud[Hud.WAVE].toInt()
        val state = hud[Hud.WAVE_STATE].toInt()
        val big = hud[Hud.WAVE_BIG] > 0.5f
        val t = hud[Hud.SURV_TIME].toInt()
        val kills = hud[Hud.KILLS].toInt()
        val score = hud[Hud.SCORE].toInt()
        val title = if (state == 1) "ONDA $wave" + if (big) " • GRANDE HORDA" else "" else "ONDA ${wave + 1} EM ${kotlin.math.ceil(hud[Hud.WAVE_TIMER]).toInt()}s"
        val text = "$title   •   ☠ $kills   •   %d:%02d   •   ★ $score".format(t / 60, t % 60)
        textPaint.textSize = d(14f)
        val tw = textPaint.measureText(text) + d(36f)
        rect.set(w / 2 - tw / 2, d(12f), w / 2 + tw / 2, d(44f))
        p.color = if (big && state == 1) Color.argb(220, 90, 20, 20) else Color.argb(200, 8, 14, 26)
        canvas.drawRoundRect(rect, d(16f), d(16f), p)
        textPaint.color = if (state == 1) (if (big) Color.rgb(255, 150, 120) else Palette.lime) else Palette.gold
        canvas.drawText(text, w / 2, d(33f), textPaint)
        // Barra de progresso da onda com a cabeça de zumbi avançando e a bandeira no fim.
        val bw = d(240f)
        val top = d(52f)
        val prog = if (state == 1) hud[Hud.WAVE_PROGRESS].coerceIn(0f, 1f) else if (state == 2) 1f else 0f
        rect.set(w / 2 - bw / 2, top, w / 2 + bw / 2, top + d(10f))
        p.color = Color.argb(170, 0, 0, 0)
        canvas.drawRoundRect(rect, d(5f), d(5f), p)
        rect.set(w / 2 - bw / 2, top, w / 2 - bw / 2 + bw * prog, top + d(10f))
        p.color = if (big) Color.rgb(230, 70, 60) else Color.rgb(140, 210, 70)
        canvas.drawRoundRect(rect, d(5f), d(5f), p)
        val fx = w / 2 + bw / 2
        p.color = Color.rgb(200, 200, 200)
        canvas.drawRect(fx - d(1f), top - d(8f), fx + d(1f), top + d(12f), p)
        p.color = if (big) Palette.red else Palette.gold
        path.reset()
        path.moveTo(fx + d(1f), top - d(8f)); path.lineTo(fx + d(12f), top - d(4f)); path.lineTo(fx + d(1f), top); path.close()
        canvas.drawPath(path, p)
        val hx = w / 2 - bw / 2 + bw * prog
        p.color = Color.rgb(120, 190, 90)
        canvas.drawCircle(hx, top + d(5f), d(8f), p)
        p.color = Color.rgb(30, 40, 20)
        canvas.drawCircle(hx - d(3f), top + d(3.5f), d(1.6f), p)
        canvas.drawCircle(hx + d(3f), top + d(3.5f), d(1.6f), p)
        canvas.drawRect(hx - d(3f), top + d(7.5f), hx + d(3f), top + d(9f), p)
        if (state == 1) {
            textPaint.textSize = d(10f)
            textPaint.color = Palette.muted
            canvas.drawText("RESTAM ${hud[Hud.WAVE_LEFT].toInt()} ZUMBIS", w / 2, top + d(24f), textPaint)
        }
        // Aviso grande no começo das grandes hordas e na contagem final do intervalo.
        val timer = hud[Hud.WAVE_TIMER]
        val banner = when {
            state == 1 && big && timer < 4f -> "⚠ UMA GRANDE HORDA DE ZUMBIS ESTÁ CHEGANDO!"
            state == 1 && timer < 3f -> "ONDA $wave!"
            state != 1 && timer <= 5f -> "PREPARE-SE: ${kotlin.math.ceil(timer).toInt()}"
            state == 2 && timer > 12f -> "ONDA ${wave} VENCIDA! +VIDA E UM ITEM"
            else -> null
        }
        if (banner != null) {
            val pulse = 1f + 0.06f * sin(System.currentTimeMillis() / 120.0).toFloat()
            textPaint.textSize = d(24f) * pulse
            textPaint.color = Color.argb(160, 0, 0, 0)
            // Usa a faixa do aviso de combate (que fica suprimido enquanto o banner existe): um só aviso por vez.
            bannerShown = true
            val by = d(CombatFeed.noticeBaselineDp(h / d(1f)))
            canvas.drawText(banner, w / 2 + d(2f), by + d(2f), textPaint)
            textPaint.color = if (big && state == 1) Palette.red else if (state == 2) Palette.lime else Palette.gold
            canvas.drawText(banner, w / 2, by, textPaint)
        }
    }

    /** Escala do controle (para textos e ícones acompanharem o tamanho escolhido). */
    private fun k(c: HudControl): Float = g(c).r / d(c.baseDp)

    private fun label(canvas: Canvas, text: String, x: Float, y: Float, size: Float, color: Int) {
        textPaint.textSize = d(maxOf(size, 7f))   // legível mesmo com o controle reduzido
        textPaint.color = color
        canvas.drawText(text, x, y, textPaint)
    }

    private fun drawJoystick(canvas: Canvas) {
        val lit = stickActive || autoRun
        if (!g(HudControl.JOYSTICK).visible && !lit) return
        // Parado: anel pequeno no canto (E24 da referência). Em uso: o anel inteiro onde o polegar tocou.
        val r = if (stickActive) stickRadius else stickRadius * HudLayout.JOYSTICK_IDLE
        p.color = if (stickActive) Color.argb(70, 0, 0, 0) else Color.argb(60, 255, 255, 255)
        canvas.drawCircle(stickBaseX, stickBaseY, r, p)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(1.4f)
        p.color = if (autoRun) Palette.gold else RING
        canvas.drawCircle(stickBaseX, stickBaseY, r, p)
        p.style = Paint.Style.FILL
        p.color = if (autoRun) Palette.gold else Color.argb(if (stickActive) 200 else 150, 230, 230, 230)
        // Corrida automática sem o polegar no joystick: a bolinha fica empurrada para a frente.
        canvas.drawCircle(stickX, if (autoRun && !stickActive) stickBaseY - r * 0.6f else stickY, r * 0.41f, p)
        val text = when {
            autoRun -> "» CORRIDA AUTOMÁTICA"
            hud[Hud.SPRINTING] > 0.5f -> "» CORRENDO"
            !stickActive && hud[Hud.SLOWED] > 0f -> "LENTO (ÁCIDO)"
            else -> return
        }
        // Rótulo acima do anel, sem sair da tela quando o joystick está no canto.
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(11f)
        val tw = textPaint.measureText(text)
        val tx = stickBaseX.coerceIn(tw / 2 + d(6f), width - tw / 2 - d(6f))
        textPaint.color = if (autoRun) Palette.gold else Color.WHITE
        textPaint.setShadowLayer(d(2f), d(1f), d(1f), Color.BLACK)
        canvas.drawText(text, tx, stickBaseY - r - d(8f), textPaint)
        textPaint.clearShadowLayer()
        textPaint.typeface = Fonts.title
    }

    private fun drawFireButton(canvas: Canvas, c: HudControl, firing: Boolean, main: Boolean) {
        val gc = g(c)
        // Anel pontilhado em volta e projétil com clarão (E14 da referência).
        dash.color = Color.argb(70, 255, 255, 255)
        dash.strokeWidth = d(1.5f)
        dash.pathEffect = fireDash
        canvas.drawCircle(gc.x, gc.y, gc.r * 1.22f, dash)
        dash.pathEffect = null
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, firing)
        drawBulletIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.15f, ICON)
        val reload = hud[Hud.RELOAD]
        if (main && reload >= 0f) {
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(5f)
            p.color = Palette.gold
            rect.set(gc.x - gc.r - d(5f), gc.y - gc.r - d(5f), gc.x + gc.r + d(5f), gc.y + gc.r + d(5f))
            canvas.drawArc(rect, -90f, 360f * reload, false, p)
            p.style = Paint.Style.FILL
        }
    }

    private fun drawAim(canvas: Canvas) {
        val gc = g(HudControl.AIM)
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, aiming)
        // Retícula com cantoneiras (E23 da referência).
        val u = iconUnit(gc)
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.BUTT
        p.strokeWidth = 2.6f * u
        p.color = ICON
        canvas.drawCircle(gc.x, gc.y, 11f * u, p)
        canvas.drawLine(gc.x - 18f * u, gc.y, gc.x - 4f * u, gc.y, p)
        canvas.drawLine(gc.x + 4f * u, gc.y, gc.x + 18f * u, gc.y, p)
        canvas.drawLine(gc.x, gc.y - 18f * u, gc.x, gc.y - 4f * u, p)
        canvas.drawLine(gc.x, gc.y + 4f * u, gc.x, gc.y + 18f * u, p)
        p.strokeWidth = 3f * u
        for (sx in listOf(-1f, 1f)) for (sy in listOf(-1f, 1f)) {
            path.reset()
            path.moveTo(gc.x + sx * 23f * u, gc.y + sy * 15f * u)
            path.lineTo(gc.x + sx * 23f * u, gc.y + sy * 23f * u)
            path.lineTo(gc.x + sx * 15f * u, gc.y + sy * 23f * u)
            canvas.drawPath(path, p)
        }
        p.style = Paint.Style.FILL
        canvas.drawCircle(gc.x, gc.y, 2.6f * u, p)
    }

    private fun drawJump(canvas: Canvas) {
        val gc = g(HudControl.JUMP)
        val jumpReady = hud[Hud.JUMP_READY] > 0.5f || hud[Hud.IN_WATER] > 0.5f
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawJumpFigure(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, if (jumpReady) ICON else ICON_DIM)
    }

    private fun drawReload(canvas: Canvas) {
        val gc = g(HudControl.RELOAD)
        val ammo = hud[Hud.AMMO].toInt()
        val mag = hud[Hud.MAG].toInt().coerceAtLeast(1)
        val lowAmmo = ammo <= mag / 4
        val reloading = hud[Hud.RELOAD] >= 0f
        drawButton(canvas, gc.x, gc.y, gc.r, Palette.cyan, reloading)
        drawMagazineIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.2f, if (lowAmmo && !reloading) Palette.gold else ICON)
    }

    /** Tempo discreto à direita da barra de vida. */
    private fun drawClock(canvas: Canvas) {
        val elapsed = hud[Hud.ELAPSED].toInt()
        textPaint.textAlign = Paint.Align.LEFT
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(10f)
        textPaint.color = Color.argb(170, 255, 255, 255)
        canvas.drawText("%02d:%02d".format(elapsed / 60, elapsed % 60), width * HP_BAR_RIGHT + d(8f), statusBaselineCenter() + d(3.5f), textPaint)
        textPaint.textAlign = Paint.Align.CENTER
        textPaint.typeface = Fonts.title
    }

    private fun drawCrouch(canvas: Canvas) {
        val gc = g(HudControl.CROUCH)
        val on = hud[Hud.CROUCH] > 0.5f
        drawButton(canvas, gc.x, gc.y, gc.r, Palette.gold, on)
        drawKneelFigure(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, if (on) Palette.gold else ICON)
    }

    private fun drawSprint(canvas: Canvas) {
        val gc = g(HudControl.SPRINT)
        val on = hud[Hud.SPRINT_LOCK] > 0.5f || hud[Hud.SPRINTING] > 0.5f || autoRun
        drawButton(canvas, gc.x, gc.y, gc.r, Palette.gold, on)
        drawRunFigure(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, if (on) Palette.gold else ICON)
    }

    private fun drawMelee(canvas: Canvas) {
        val gc = g(HudControl.MELEE)
        val cooldown = hud[Hud.MELEE].coerceIn(0f, 1f)
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawKnifeIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.2f, ICON)
        if (cooldown > 0f) {
            p.color = Color.argb(150, 0, 0, 0)
            rect.set(gc.x - gc.r, gc.y - gc.r, gc.x + gc.r, gc.y + gc.r)
            canvas.drawArc(rect, -90f, 360f * cooldown, true, p)
        }
    }

    private fun drawGrenade(canvas: Canvas) {
        val gc = g(HudControl.GRENADE)
        val count = hud[Hud.GRENADES].toInt()
        val aiming = hud[Hud.THROW_AIM] > 0.5f
        val kind = hud[Hud.THROW_KIND].toInt().takeIf { it != 0 } ?: Items.GRENADE
        // Aro externo claro como o botão de habilidade da referência (E17).
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(3f)
        p.color = Color.argb(45, 255, 255, 255)
        canvas.drawCircle(gc.x, gc.y, gc.r + d(3.5f), p)
        p.style = Paint.Style.FILL
        drawButton(canvas, gc.x, gc.y, gc.r, Color.rgb(255, 170, 70), aiming)
        val color = if (count > 0) ICON else ICON_DIM
        if (kind == Items.GRENADE) drawGrenadeIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.15f, color)
        else ItemIcons.draw(canvas, kind, gc.x, gc.y, gc.r * 1.1f, if (count > 0) 255 else 110)
        drawCount(canvas, gc, count)
    }

    /** Kit médico: usa o primeiro kit da mochila inteira (como no Free Fire; o mesmo efeito de tocar no slot). */
    private fun useMedkit() {
        val slot = (0 until Inventory.SIZE).firstOrNull { hud[Hud.INV + it * 2].toInt() == Items.MEDKIT && hud[Hud.INV + it * 2 + 1] > 0f }
        if (slot == null) message("SEM KIT MÉDICO NA MOCHILA", Palette.muted) else NativeBridge.useSlot(slot)
    }

    private fun drawMedkit(canvas: Canvas) {
        val gc = g(HudControl.MEDKIT)
        val count = (0 until Inventory.SIZE).sumOf { if (hud[Hud.INV + it * 2].toInt() == Items.MEDKIT) hud[Hud.INV + it * 2 + 1].toInt() else 0 }
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawMedkitIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, if (count > 0) ICON else ICON_DIM)
        drawCount(canvas, gc, count)
    }

    // ------------------------------------------------------------------ painel da arma e emoji

    private var emoteOpen = false
    private val emotes = listOf("😀", "😡", "😱", "👍", "💀")

    /** Espacos do seletor: as armas da barra de itens (sem arremessaveis); no fim, sempre a faca (-1). */
    private fun weaponSlotIndexes(): List<Int> =
        (0 until 6).filter { i -> hud[Hud.INV + i * 2].toInt().let { it != 0 && Items.isWeapon(it) && !Items.isThrowable(it) } } + -1

    private fun weaponRows(): Int = (weaponSlotIndexes().size + 2) / 3

    /**
     * Painel de armas (E09 + E10 da referência): arma atual em cima, espaços embaixo. O centro do controle é o
     * centro do painel com uma fileira; com mais armas ele cresce para baixo.
     */
    private fun weaponRect(): RectF {
        val gc = g(HudControl.WEAPON)
        val s = k(HudControl.WEAPON)
        val top = gc.y - d(WEAPON_ROW_DP) * s
        return RectF(gc.x - d(80f) * s, top, gc.x + d(80f) * s, top + d(WEAPON_ROW_DP) * s * (1 + weaponRows()))
    }

    /** Bloco da arma atual: tocar nele recarrega. */
    private fun weaponTopRect(): RectF {
        val r = weaponRect()
        return RectF(r.left, r.top, r.right, r.top + d(WEAPON_ROW_DP - 3f) * k(HudControl.WEAPON))
    }

    /** Espaco j da lista (3 por linha), toda a area do espaco responde ao toque. */
    private fun weaponSlotRect(j: Int): RectF {
        val r = weaponRect()
        val s = k(HudControl.WEAPON)
        val gap = d(3f) * s
        val w = (r.width() - 2f * gap) / 3f
        val row = j / 3
        val col = j % 3
        val left = r.left + col * (w + gap)
        val top = r.top + d(WEAPON_ROW_DP) * s * (1 + row)
        return RectF(left, top, left + w, top + d(WEAPON_ROW_DP - 3f) * s)
    }

    private fun weaponSlotAt(x: Float, y: Float): Int =
        weaponSlotIndexes().indices.firstOrNull { weaponSlotRect(it).contains(x, y) } ?: -1

    /** Toque num espaco: arma da barra de itens (troca) ou faca (corpo a corpo). */
    private fun weaponSlotTap(x: Float, y: Float) {
        val j = weaponSlotAt(x, y)
        if (j < 0) return
        val qi = weaponSlotIndexes()[j]
        if (qi < 0) NativeBridge.pressMelee() else NativeBridge.tapSlot(qi)
    }

    private fun drawWeaponPanel(canvas: Canvas) {
        val s = k(HudControl.WEAPON)
        val sel = hud[Hud.WEAPON_SLOT].toInt()
        val wid = hud[Hud.INV + sel * 2].toInt()
        val isGun = wid != 0 && Items.isWeapon(wid)
        val ammo = hud[Hud.AMMO].toInt()
        val mag = hud[Hud.MAG].toInt().coerceAtLeast(1)
        val reloading = hud[Hud.RELOAD] >= 0f
        val low = isGun && ammo <= mag / 4
        // Bloco da arma atual: fundo escuro, faixa dourada à direita com o canto chanfrado e barra do pente embaixo.
        val top = weaponTopRect()
        val cut = d(10f) * s
        p.style = Paint.Style.FILL
        p.color = PANEL
        path.reset()
        path.moveTo(top.left, top.top); path.lineTo(top.right - cut, top.top); path.lineTo(top.right, top.top + cut)
        path.lineTo(top.right, top.bottom); path.lineTo(top.left, top.bottom); path.close()
        canvas.drawPath(path, p)
        if (isGun) {
            val goldLeft = top.right - top.width() * 0.36f
            if (goldShader == null || goldShaderLeft != goldLeft || goldShaderRight != top.right) {
                goldShader = LinearGradient(goldLeft, 0f, top.right, 0f, Color.argb(0, 214, 160, 20), Color.argb(190, 214, 160, 20), Shader.TileMode.CLAMP)
                goldShaderLeft = goldLeft
                goldShaderRight = top.right
            }
            p.shader = goldShader
            path.reset()
            path.moveTo(goldLeft, top.top); path.lineTo(top.right - cut, top.top); path.lineTo(top.right, top.top + cut)
            path.lineTo(top.right, top.bottom); path.lineTo(goldLeft, top.bottom); path.close()
            canvas.drawPath(path, p)
            p.shader = null
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(1.5f) * s
            p.color = Color.rgb(255, 196, 0)
            path.reset()
            path.moveTo(goldLeft + top.width() * 0.12f, top.top); path.lineTo(top.right - cut, top.top)
            path.lineTo(top.right, top.top + cut); path.lineTo(top.right, top.bottom)
            canvas.drawPath(path, p)
            p.style = Paint.Style.FILL
        }
        textPaint.textAlign = Paint.Align.LEFT
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(25f) * s
        textPaint.color = if (low) Palette.gold else Color.WHITE
        val ammoStr = if (isGun) "$ammo" else "∞"
        canvas.drawText(ammoStr, top.left + d(8f) * s, top.top + d(25f) * s, textPaint)
        if (isGun) {
            val w1 = textPaint.measureText(ammoStr)
            textPaint.textSize = d(13f) * s
            textPaint.color = Color.argb(220, 255, 255, 255)
            canvas.drawText("/$mag", top.left + d(10f) * s + w1, top.top + d(25f) * s, textPaint)
            textPaint.textSize = d(10f) * s
            textPaint.color = if (reloading) Palette.cyan else Color.WHITE
            canvas.drawText(if (reloading) "Recarregando" else "Recarregar", top.left + d(8f) * s, top.top + d(39f) * s, textPaint)
            // Barra do pente: cheia com o pente cheio; recarregando mostra o progresso.
            val frac = if (reloading) hud[Hud.RELOAD].coerceIn(0f, 1f) else (ammo.toFloat() / mag).coerceIn(0f, 1f)
            rect.set(top.left + d(8f) * s, top.bottom - d(7f) * s, top.right - d(8f) * s, top.bottom - d(4f) * s)
            p.color = Color.argb(90, 255, 255, 255)
            canvas.drawRect(rect, p)
            rect.right = rect.left + rect.width() * frac
            p.color = if (reloading) Palette.cyan else if (low) Palette.gold else Color.WHITE
            canvas.drawRect(rect, p)
        }
        textPaint.textAlign = Paint.Align.CENTER
        if (wid != 0) ItemIcons.draw(canvas, wid, top.right - d(42f) * s, top.top + d(19f) * s, d(54f) * s)
        // Espacos: armas da barra de itens e a faca.
        val indexes = weaponSlotIndexes()
        for ((j, qi) in indexes.withIndex()) {
            val rr = weaponSlotRect(j)
            val selected = qi == sel || (qi < 0 && !isGun)
            p.color = PANEL
            canvas.drawRect(rr, p)
            if (selected) {
                p.style = Paint.Style.STROKE
                p.strokeWidth = d(1.5f)
                p.color = Color.rgb(255, 196, 0)
                canvas.drawRect(rr, p)
                p.style = Paint.Style.FILL
            }
            if (qi < 0) {
                // Faca com o "∞" embaixo, como o punho da referência.
                drawKnifeIcon(canvas, rr.centerX(), rr.centerY() - rr.height() * 0.12f, rr.height() / 80f, ICON)
                textPaint.typeface = Fonts.bold
                textPaint.textSize = d(13f) * s
                textPaint.color = ICON
                canvas.drawText("∞", rr.centerX(), rr.bottom - d(4f) * s, textPaint)
            } else {
                ItemIcons.draw(canvas, hud[Hud.INV + qi * 2].toInt(), rr.centerX(), rr.centerY(), rr.height() * 0.8f)
            }
            textPaint.textAlign = Paint.Align.LEFT
            textPaint.typeface = Fonts.title
            textPaint.textSize = d(8f) * s
            textPaint.color = Color.argb(if (selected) 170 else 110, 255, 255, 255)
            canvas.drawText("${j + 1}", rr.left + d(4f) * s, rr.top + d(10f) * s, textPaint)
            textPaint.textAlign = Paint.Align.CENTER
        }
    }

    /** Botão de emoji: rosto sorridente; ao tocar abre uma fileira de emojis à direita. */
    private fun drawEmote(canvas: Canvas) {
        val gc = g(HudControl.EMOTE)
        val s = k(HudControl.EMOTE)
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, emoteOpen)
        // Rosto sorridente em contorno branco (E03 da referência).
        val u = gc.r * 0.7f / 24f
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.ROUND
        p.strokeWidth = 3.4f * u
        p.color = ICON
        canvas.drawCircle(gc.x, gc.y, 20f * u, p)
        rect.set(gc.x - 11f * u, gc.y - 6f * u, gc.x + 11f * u, gc.y + 12f * u)
        canvas.drawArc(rect, 25f, 130f, false, p)
        p.style = Paint.Style.FILL
        canvas.drawCircle(gc.x - 7f * u, gc.y - 5f * u, 2.8f * u, p)
        canvas.drawCircle(gc.x + 7f * u, gc.y - 5f * u, 2.8f * u, p)
        if (emoteOpen) {
            for (i in emotes.indices) {
                val cx = emoteCenterX(i)
                p.color = Color.argb(225, 10, 16, 30)
                canvas.drawCircle(cx, gc.y, emoteRadius(), p)
                textPaint.textAlign = Paint.Align.CENTER
                textPaint.typeface = Fonts.body
                textPaint.textSize = d(18f) * s
                textPaint.color = Color.WHITE
                canvas.drawText(emotes[i], cx, gc.y + d(6f) * s, textPaint)
            }
        }
    }

    private fun emoteCenterX(i: Int): Float = g(HudControl.EMOTE).x + (i + 1) * d(46f) * k(HudControl.EMOTE)
    private fun emoteRadius(): Float = d(20f) * k(HudControl.EMOTE)

    private fun emoteItemAt(x: Float, y: Float): Int {
        if (!emoteOpen) return -1
        val cy = g(HudControl.EMOTE).y
        for (i in emotes.indices) if (hypot(x - emoteCenterX(i), y - cy) < emoteRadius() * 1.2f) return i
        return -1
    }

    private fun sendEmote(i: Int) {
        if (i in emotes.indices) message(emotes[i], Palette.lime)
    }

    private fun drawPause(canvas: Canvas) {
        val gc = g(HudControl.PAUSE)
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawGearIcon(canvas, gc.x, gc.y, gc.r * 0.72f / 24f, ICON)
    }

    // ------------------------------------------------------------------ ícones em silhueta branca
    // Desenhos próprios do jogo (pictogramas), numa grade de ±24 unidades centrada em (x, y); u = px por unidade.

    /** Traço arredondado dos bonecos (membros e tronco). */
    private fun limb(canvas: Canvas, vararg pts: Float) {
        path.reset()
        path.moveTo(pts[0], pts[1])
        var i = 2
        while (i < pts.size) { path.lineTo(pts[i], pts[i + 1]); i += 2 }
        canvas.drawPath(path, p)
    }

    private fun figurePaint(u: Float, color: Int) {
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.ROUND
        p.strokeJoin = Paint.Join.ROUND
        p.strokeWidth = 6f * u
        p.color = color
    }

    private fun endFigure() {
        p.style = Paint.Style.FILL
        p.strokeJoin = Paint.Join.MITER
        p.strokeCap = Paint.Cap.BUTT
    }

    /** Boneco correndo inclinado para a frente (CORRER). */
    private fun drawRunFigure(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        figurePaint(u, color)
        limb(canvas, x + 3f * u, y - 9f * u, x - 3f * u, y + 4f * u)                              // tronco
        limb(canvas, x + 2f * u, y - 7f * u, x + 10f * u, y - 3f * u, x + 16f * u, y - 9f * u)    // braço da frente
        limb(canvas, x + 2f * u, y - 7f * u, x - 6f * u, y - 4f * u, x - 11f * u, y + 2f * u)     // braço de trás
        limb(canvas, x - 3f * u, y + 4f * u, x + 7f * u, y + 9f * u, x + 5f * u, y + 19f * u)     // perna da frente
        limb(canvas, x - 3f * u, y + 4f * u, x - 9f * u, y + 12f * u, x - 18f * u, y + 11f * u)   // perna de trás
        endFigure()
        canvas.drawCircle(x + 6f * u, y - 16f * u, 4.8f * u, p)
    }

    /** Soldado no meio do pulo, joelhos encolhidos e arma à frente (PULAR). */
    private fun drawJumpFigure(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        figurePaint(u, color)
        limb(canvas, x + 3f * u, y - 9f * u, x - 1f * u, y + 3f * u)                              // tronco
        limb(canvas, x - 1f * u, y + 3f * u, x + 10f * u, y + 6f * u, x + 5f * u, y + 16f * u)    // perna encolhida
        limb(canvas, x - 1f * u, y + 3f * u, x - 8f * u, y + 11f * u, x - 2f * u, y + 19f * u)    // outra perna
        limb(canvas, x + 2f * u, y - 7f * u, x - 7f * u, y - 3f * u)                              // braços na arma
        p.strokeWidth = 3.6f * u
        limb(canvas, x - 19f * u, y - 1f * u, x + 6f * u, y - 9f * u)                             // arma
        endFigure()
        canvas.drawCircle(x + 6f * u, y - 16f * u, 4.8f * u, p)
    }

    /** Soldado ajoelhado mirando para a esquerda (AGACHAR). */
    private fun drawKneelFigure(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        figurePaint(u, color)
        limb(canvas, x + 4f * u, y - 8f * u, x + 6f * u, y + 4f * u)                              // tronco
        limb(canvas, x + 6f * u, y + 4f * u, x - 4f * u, y + 6f * u, x - 4f * u, y + 18f * u)     // perna da frente
        limb(canvas, x + 6f * u, y + 4f * u, x + 11f * u, y + 18f * u, x + 20f * u, y + 18f * u)  // joelho no chão
        limb(canvas, x + 4f * u, y - 6f * u, x - 7f * u, y - 6f * u)                              // braços na arma
        p.strokeWidth = 3.6f * u
        limb(canvas, x - 21f * u, y - 8f * u, x + 7f * u, y - 9f * u)                             // arma
        endFigure()
        canvas.drawCircle(x + 5f * u, y - 15f * u, 4.8f * u, p)
    }

    /** Projétil inclinado com o clarão do disparo (ATIRAR). */
    private fun drawBulletIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.FILL
        p.color = color
        canvas.save()
        canvas.rotate(-35f, x, y)
        path.reset()
        path.moveTo(x - 5f * u, y - 4f * u)
        path.quadTo(x - 5f * u, y - 15f * u, x, y - 21f * u)
        path.quadTo(x + 5f * u, y - 15f * u, x + 5f * u, y - 4f * u)
        path.close()
        canvas.drawPath(path, p)
        rect.set(x - 5f * u, y - 2f * u, x + 5f * u, y + 14f * u)
        canvas.drawRect(rect, p)
        canvas.restore()
        // Clarão: três raios saindo da base.
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.ROUND
        p.strokeWidth = 2.6f * u
        val bx = x + 7f * u
        val by = y + 12f * u
        canvas.drawLine(bx + 2f * u, by - 2f * u, bx + 11f * u, by - 10f * u, p)
        canvas.drawLine(bx + 4f * u, by + 2f * u, bx + 15f * u, by + 1f * u, p)
        canvas.drawLine(bx, by + 4f * u, bx + 4f * u, by + 13f * u, p)
        p.style = Paint.Style.FILL
        p.strokeCap = Paint.Cap.BUTT
    }

    /** Pente curvo com a ponta da bala aparecendo (RECARREGAR). */
    private fun drawMagazineIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.FILL
        p.color = color
        path.reset()
        path.moveTo(x - 8f * u, y - 10f * u)
        path.lineTo(x + 6f * u, y - 10f * u)
        path.quadTo(x + 9f * u, y + 4f * u, x + 14f * u, y + 16f * u)
        path.lineTo(x + 1f * u, y + 19f * u)
        path.quadTo(x - 5f * u, y + 6f * u, x - 8f * u, y - 10f * u)
        path.close()
        canvas.drawPath(path, p)
        path.reset()
        path.moveTo(x - 5f * u, y - 12f * u)
        path.quadTo(x - 1f * u, y - 22f * u, x + 3f * u, y - 12f * u)
        path.close()
        canvas.drawPath(path, p)
        p.color = ICON_CUT
        canvas.drawRect(x - 5f * u, y - 3f * u, x + 6f * u, y - 1f * u, p)
    }

    /** Faca na diagonal, lâmina e cabo separados por um vão (FACA). */
    private fun drawKnifeIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.FILL
        p.color = color
        canvas.save()
        canvas.rotate(45f, x, y)
        path.reset()
        path.moveTo(x - 3.5f * u, y - 2f * u)
        path.lineTo(x - 3.5f * u, y - 18f * u)
        path.quadTo(x + 4f * u, y - 14f * u, x + 4f * u, y - 2f * u)
        path.close()
        canvas.drawPath(path, p)
        rect.set(x - 4.5f * u, y + 1f * u, x + 4.5f * u, y + 3f * u)
        canvas.drawRect(rect, p)
        rect.set(x - 3f * u, y + 4.5f * u, x + 3f * u, y + 18f * u)
        canvas.drawRoundRect(rect, 2f * u, 2f * u, p)
        canvas.restore()
    }

    /** Granada de mão: corpo, alavanca e argola (GRANADA). */
    private fun drawGrenadeIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.FILL
        p.color = color
        rect.set(x - 10f * u, y - 6f * u, x + 10f * u, y + 18f * u)
        canvas.drawOval(rect, p)
        rect.set(x - 4.5f * u, y - 12f * u, x + 4.5f * u, y - 5f * u)
        canvas.drawRect(rect, p)
        p.style = Paint.Style.STROKE
        p.strokeCap = Paint.Cap.ROUND
        p.strokeWidth = 3f * u
        canvas.drawLine(x + 3f * u, y - 11f * u, x + 12f * u, y - 2f * u, p)
        canvas.drawCircle(x - 8f * u, y - 13f * u, 3.5f * u, p)
        p.style = Paint.Style.FILL
        p.strokeCap = Paint.Cap.BUTT
        p.color = ICON_CUT
        canvas.drawRect(x - 10f * u, y + 5f * u, x + 10f * u, y + 7f * u, p)
    }

    /** Maleta de primeiros socorros com a cruz vazada (KIT MÉDICO). */
    private fun drawMedkitIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.STROKE
        p.strokeWidth = 3f * u
        p.color = color
        rect.set(x - 6f * u, y - 16f * u, x + 6f * u, y - 8f * u)
        canvas.drawRoundRect(rect, 2f * u, 2f * u, p)
        p.style = Paint.Style.FILL
        rect.set(x - 17f * u, y - 10f * u, x + 17f * u, y + 15f * u)
        canvas.drawRoundRect(rect, 4f * u, 4f * u, p)
        p.color = ICON_CUT
        canvas.drawRect(x - 2.5f * u, y - 5f * u, x + 2.5f * u, y + 10f * u, p)
        canvas.drawRect(x - 7.5f * u, y, x + 7.5f * u, y + 5f * u, p)
    }

    /** Caixa de suprimentos presa a um paraquedas (SUPRIMENTOS). */

    /** Engrenagem com o furo no meio (PAUSA / configurações). */
    private fun drawGearIcon(canvas: Canvas, x: Float, y: Float, u: Float, color: Int) {
        p.style = Paint.Style.FILL
        p.color = color
        for (i in 0 until 8) {
            canvas.save()
            canvas.rotate(i * 45f, x, y)
            canvas.drawRect(x - 3.5f * u, y - 19f * u, x + 3.5f * u, y - 10f * u, p)
            canvas.restore()
        }
        canvas.drawCircle(x, y, 13f * u, p)
        p.color = ICON_CUT
        canvas.drawCircle(x, y, 5.5f * u, p)
    }

    /** Controle oculto, mostrado só no editor para poder ser ligado de novo. */
    private fun drawGhost(canvas: Canvas, c: HudControl) {
        val gc = g(c)
        dash.color = Color.argb(150, 255, 255, 255)
        dash.strokeWidth = d(1.5f)
        dash.pathEffect = android.graphics.DashPathEffect(floatArrayOf(d(5f), d(4f)), 0f)
        canvas.drawCircle(gc.x, gc.y, gc.r, dash)
        label(canvas, c.label.uppercase(), gc.x, gc.y + d(3f), 8f, Color.argb(170, 255, 255, 255))
        label(canvas, "OCULTO", gc.x, gc.y + d(13f), 7f, Color.argb(130, 255, 255, 255))
    }

    private fun drawEditor(canvas: Canvas) {
        java.util.Arrays.fill(hud, 0f)
        hud[Hud.AMMO] = 30f
        hud[Hud.MAG] = 30f
        hud[Hud.RELOAD] = -1f
        hud[Hud.GRENADES] = 3f
        hud[Hud.MELEE] = 0f
        hud[Hud.ISLAND] = 1f
        hud[Hud.JUMP_READY] = 1f
        hud[Hud.INV_ENABLED] = 1f
        hud[Hud.WEAPON] = Items.RIFLE.toFloat()
        hud[Hud.WEAPON_SLOT] = 0f
        hud[Hud.THROW_KIND] = Items.GRENADE.toFloat()
        val w = width.toFloat()
        val h = height.toFloat()
        p.color = Color.argb(24, 255, 255, 255)
        val step = d(32f)
        var x = 0f
        while (x < w) { canvas.drawRect(x, 0f, x + 1f, h, p); x += step }
        var y = 0f
        while (y < h) { canvas.drawRect(0f, y, w, y + 1f, p); y += step }
        if (g(HudControl.MAP).visible) {
            rect.set(mapX - mapSize / 2, mapY - mapSize / 2, mapX + mapSize / 2, mapY + mapSize / 2)
            p.color = Color.argb(200, 8, 14, 26)
            canvas.drawRoundRect(rect, d(MAP_CORNER), d(MAP_CORNER), p)
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(1.5f)
            p.color = RING
            canvas.drawRoundRect(rect, d(MAP_CORNER), d(MAP_CORNER), p)
            p.style = Paint.Style.FILL
            label(canvas, "MINIMAPA", mapX, mapY + d(4f), 10f * k(HudControl.MAP), Palette.muted)
        }
        drawControls(canvas)
        val c = selected ?: return
        val gc = g(c)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(2.5f)
        p.color = Palette.gold
        when (c) {
            HudControl.MAP -> { rect.set(gc.x - gc.r - d(5f), gc.y - gc.r - d(5f), gc.x + gc.r + d(5f), gc.y + gc.r + d(5f)); canvas.drawRoundRect(rect, d(16f), d(16f), p) }
            HudControl.WEAPON -> { canvas.drawRoundRect(weaponRect(), d(12f), d(12f), p) }
            HudControl.HOTBAR -> { rect.set(hotbarX - slotSize * 3f - d(6f), hotbarY - slotSize / 2f - d(6f), hotbarX + slotSize * 3f + d(6f), hotbarY + slotSize / 2f + d(6f)); canvas.drawRoundRect(rect, d(10f), d(10f), p) }
            HudControl.JOYSTICK -> canvas.drawCircle(gc.x, gc.y, gc.r * HudLayout.JOYSTICK_IDLE + d(6f), p)
            else -> canvas.drawCircle(gc.x, gc.y, gc.r + d(6f), p)
        }
        p.style = Paint.Style.FILL
        val top = when (c) {
            HudControl.WEAPON -> weaponRect().top
            HudControl.JOYSTICK -> gc.y - gc.r * HudLayout.JOYSTICK_IDLE
            else -> gc.y - gc.r
        }
        val text = "${c.label} • ${(layout[c].scale * 100).toInt()}%" + if (!gc.visible) " (oculto)" else ""
        textPaint.textSize = d(12f)
        val tw = textPaint.measureText(text) + d(16f)
        val ty = (top - d(14f)).coerceAtLeast(d(20f))
        rect.set(gc.x - tw / 2, ty - d(14f), gc.x + tw / 2, ty + d(6f))
        p.color = Color.argb(220, 40, 30, 8)
        canvas.drawRoundRect(rect, d(9f), d(9f), p)
        label(canvas, text, gc.x, ty, 12f, Palette.gold)
    }

    companion object {
        // Estilo da referência (screenshots/xray-hud): botões pretos translúcidos, aro claro fino, ícones brancos.
        private val BUTTON_FILL = Color.argb(120, 0, 0, 0)
        private val RING = Color.argb(115, 255, 255, 255)
        private val PANEL = Color.argb(185, 22, 22, 20)
        private val ICON = Color.rgb(245, 245, 245)
        private val ICON_DIM = Color.argb(120, 245, 245, 245)
        /** Detalhes vazados dos ícones (bolso, cruz, furo da engrenagem). */
        private val ICON_CUT = Color.rgb(48, 48, 46)
        private const val MAP_CORNER = 3f
        /** Barra de vida (E26): de 43% a 59,5% da largura. */
        private const val HP_BAR_LEFT = 0.430f
        private const val HP_BAR_RIGHT = 0.595f
        /** Altura (dp, escala 1) do bloco da arma e de cada fileira de espaços do painel de armas. */
        private const val WEAPON_ROW_DP = 52f

        private const val TAU = (PI * 2).toFloat()
        private val NEST_COLOR = Color.rgb(225, 75, 225)
        private val CARGO_COLOR = Palette.contractCrate   // dourado: a carga da Caixa perdida
        private const val KIND_NEST = 0
        private const val KIND_CRYSTAL = 1
        private const val KIND_PORTAL = 2
        private const val KIND_CARGO = 3

        /** Regiões do centro superior (dp): barra superior, bússola e, abaixo, os pontos de extensão. */
        const val TOP_BAR_TOP_DP = 6f
        const val TOP_BAR_BOTTOM_DP = 36f
        const val COMPASS_TOP_DP = 40f
    }
}
