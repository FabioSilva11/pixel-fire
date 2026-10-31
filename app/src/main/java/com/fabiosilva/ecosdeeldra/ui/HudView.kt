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
import com.fabiosilva.ecosdeeldra.engine.Hud
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.min
import kotlin.math.sin

/** Original customizable controls, with authoritative Battle Royale status and inventory. */
@SuppressLint("ViewConstructor")
class HudView(context: Context, private val listener: Listener) : View(context) {
    interface Listener {
        fun onPauseRequested()
        fun onMapRequested()
        fun onInventoryRequested() = Unit
    }

    private enum class Role { NONE, JOYSTICK, LOOK, FIRE, FIRE_LEFT, AIM, JUMP, RELOAD, CROUCH, SPRINT, MELEE, GRENADE, MEDKIT, EMOTE, EMOTE_ITEM, WEAPON_SLOT, PAUSE, MAP, HOTBAR, BAG, REVIVE }
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

    var editing = false
        set(value) {
            field = value
            selected = null
            invalidate()
        }
    var selected: HudControl? = null
        private set
    var onSelectionChanged: (() -> Unit)? = null
    var layout: HudLayout = HudLayout.defaults(900f, 410f, false)
        private set

    private var aiming = false
    private var aimStage = 0
    private var aimWeapon = -1
    
    private val autoRun get() = hud[Hud.SPRINT_LOCK] > 0.5f
    private var stickBaseX = 0f
    private var stickBaseY = 0f
    private var stickX = 0f
    private var stickY = 0f
    private var stickActive = false
    
    private val combatFeed = CombatFeed()
    
    private val tipState = HudTip()
    private var lastHealth = -1f
    private var hurtUntil = 0L
    private var battle: FloatArray? = null
    private var localId = -1
    private var latency = 0
    private var healHeld = false
    private var grenadeCount = 0
    private var medkitCount = 0
    private val throwAiming get() = touches.values.any { it.role == Role.GRENADE }
    private var reviveTarget = -1
    private var reviveHeld = false
    private val planeJumpRect = RectF()
    private val planeJumpTouchArea = RectF()
    private val chuteButtonRect = RectF()
    private val chuteTouchArea = RectF()

    val flight: Int
        get() {
            if (editing) return 3
            val state = battle
            val me = own()
            if (state != null && me >= 0) {
                val netFlight = state[me + 11].toInt()
                val localFlight = hud[Hud.FLIGHT].toInt()
                if (localFlight in 0..3) {
                    return if (netFlight == 0 && localFlight > 0) localFlight else netFlight
                }
                return netFlight
            }
            val localFlight = hud[Hud.FLIGHT].toInt()
            return if (localFlight in 0..3) localFlight else 3
        }

    fun updateBattle(value: FloatArray, id: Int, ping: Int) {
        battle = value
        localId = id
        latency = ping
        invalidate()
    }

    private fun own(): Int {
        val state = battle ?: return -1
        repeat(state[16].toInt()) {
            val at = 24 + it * 32
            if (state[at].toInt() == localId) return at
        }
        return -1
    }

    private fun inventoryCount(id: Int): Int = (0 until Inventory.SIZE).sumOf {
        if (hud[Hud.INV + it * 2].toInt() == id) hud[Hud.INV + it * 2 + 1].toInt() else 0
    }

    private fun updatePresentation() {
        grenadeCount = inventoryCount(25)
        medkitCount = inventoryCount(23)
        val state = battle
        val me = own()
        reviveTarget = -1
        if (state != null && me >= 0) {
            repeat(state[16].toInt()) {
                val at = 24 + it * 32
                if (at != me && state[at + 1] == state[me + 1] &&
                    (state[at + 3].toInt() and 3) == 3 &&
                    hypot(state[at + 4] - state[me + 4], state[at + 6] - state[me + 6]) < 2.5f)
                    reviveTarget = state[at].toInt()
            }
            val seconds = state[me + 14]
            val duration = when (state[me + 12].toInt()) { 1 -> 1.4f; 2 -> 1.7f; 4 -> 2.5f; 5 -> 2.8f; else -> 2f }
            hud[Hud.RELOAD] = if (seconds > 0f) (1f - seconds / duration).coerceIn(0f, 1f) else -1f
        } else if (hud[Hud.RELOAD] <= 0f) hud[Hud.RELOAD] = -1f
        if (healHeld) NativeBridge.battleAction(64)
        if (reviveHeld && reviveTarget >= 0) NativeBridge.battleAction(1024, reviveTarget)
    }

    private fun reviveRect(): RectF = RectF(width * .5f - d(85f), height * .64f - d(18f), width * .5f + d(85f), height * .64f + d(10f))

    private fun drawItem(canvas: Canvas, id: Int, x: Float, y: Float, size: Float, alpha: Int = 255) {
        val legacy = BattleItems.iconId(id)
        if (legacy != 0) ItemIcons.draw(canvas, legacy, x, y, size, alpha)
        else {
            textPaint.textSize = size * .22f
            textPaint.color = Color.WHITE
            canvas.drawText(BattleItems.short(id), x, y + size * .08f, textPaint)
        }
    }
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
    
    private val fireDash = android.graphics.DashPathEffect(floatArrayOf(d(1.5f), d(4f)), 0f)
    
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
    }

    fun rebuildLayout() {
        if (width <= 0 || height <= 0) return
        layout = HudLayout.decode(layoutText, defaultLayout())
        layoutControls()
    }

    fun defaultLayout(): HudLayout = HudLayout.defaults(width / density, height / density, leftHanded)

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
        healHeld = false
        reviveHeld = false
        emoteOpen = false
        aiming = false
        aimStage = 0
        aimWeapon = -1
        stickActive = false
        combatFeed.clear()
        tipState.clear()
        lastHealth = -1f
        hurtUntil = 0L
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
                onSelectionChanged?.invoke()
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

    fun message(text: String, color: Int = Palette.lime, priority: Int = CombatFeed.PRIORITY_NORMAL) {
        combatFeed.show(text, color, System.currentTimeMillis(), priority)
    }

    fun tip(text: String) {
        tipState.show(text, System.currentTimeMillis())
    }

    fun haptic(strong: Boolean) {
        if (!vibration) return
        performHapticFeedback(if (strong) HapticFeedbackConstants.LONG_PRESS else HapticFeedbackConstants.KEYBOARD_TAP)
    }

    private val inventoryOn get() = hud[Hud.INV_ENABLED] > 0.5f

    // ------------------------------------------------------------------ toque

    private fun available(c: HudControl): Boolean {
        if (!g(c).visible) return false
        if (editing) return true
        if (flight < 3) {
            // Enquanto estiver no avião ou em descida (antes de aterrisar):
            // - No avião (flight == 0): apenas MAP e PAUSE
            // - Em queda livre / paraquedas (flight in 1..2): MAP, PAUSE e JOYSTICK (para direcionar a descida)
            // - Todos os controles terrestres (FIRE, AIM, JUMP, RELOAD, CROUCH, SPRINT, MELEE, GRENADE, MEDKIT, WEAPON, HOTBAR, BAG, EMOTE) ficam ocultos!
            return when (c) {
                HudControl.MAP, HudControl.PAUSE -> true
                HudControl.JOYSTICK -> flight in 1..2
                else -> false
            }
        }
        return when (c) {
            HudControl.HOTBAR, HudControl.BAG -> inventoryOn
            else -> true
        }
    }

    private val buttons = listOf(
        HudControl.FIRE to Role.FIRE, HudControl.FIRE_LEFT to Role.FIRE_LEFT, HudControl.AIM to Role.AIM,
        HudControl.JUMP to Role.JUMP, HudControl.CROUCH to Role.CROUCH, HudControl.RELOAD to Role.RELOAD,
        HudControl.SPRINT to Role.SPRINT, HudControl.MELEE to Role.MELEE, HudControl.GRENADE to Role.GRENADE,
        HudControl.MEDKIT to Role.MEDKIT, HudControl.PAUSE to Role.PAUSE,
    )

    private fun hotbarSlotAt(x: Float, y: Float): Int {
        if (!available(HudControl.HOTBAR)) return -1
        val left = hotbarX - slotSize * 3f
        if (y < hotbarY - slotSize * 0.6f || y > hotbarY + slotSize * 0.6f || x < left || x > left + slotSize * 6f) return -1
        return ((x - left) / slotSize).toInt().coerceIn(0, 5)
    }

    private fun touchHit(c: HudControl, x: Float, y: Float): Boolean {
        if (!available(c)) return false
        val gc = g(c)
        return HudTouch.area(c, gc.x, gc.y, gc.r, density).contains(x, y)
    }

    private fun roleAt(x: Float, y: Float): Role {
        val w = width.toFloat()
        val h = height.toFloat()
        if (!editing && flight == 0) {
            // No avião: botão de saltar, mapa, pausa ou olhar em volta
            if (planeJumpTouchArea.contains(x, y)) return Role.JUMP
            if (touchHit(HudControl.MAP, x, y)) return Role.MAP
            if (touchHit(HudControl.PAUSE, x, y)) return Role.PAUSE
            return Role.LOOK
        }
        if (!editing && flight in 1..2) {
            // Em queda livre / paraquedas: botão de abrir paraquedas, mapa, pausa, joystick ou olhar
            if (chuteTouchArea.contains(x, y)) return Role.JUMP
            if (touchHit(HudControl.MAP, x, y)) return Role.MAP
            if (touchHit(HudControl.PAUSE, x, y)) return Role.PAUSE
            if (HudGestures.joystickRegion(x, y, w, h, stickHomeX, touches.values.any { it.role == Role.JOYSTICK })) return Role.JOYSTICK
            return Role.LOOK
        }
        if (reviveTarget >= 0 && !throwAiming && reviveRect().contains(x, y)) return Role.REVIVE
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
        if (HudGestures.joystickRegion(x, y, w, h, stickHomeX, touches.values.any { it.role == Role.JOYSTICK })) return Role.JOYSTICK
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
                    Role.JUMP -> {
                        val f = flight
                        if (f == 0) {
                            NativeBridge.pressJump()
                            haptic(true)
                            message("SALTANDO DO AVIÃO!", Palette.gold)
                        } else if (f in 1..2) {
                            NativeBridge.battleAction(2048)
                            haptic(true)
                        } else {
                            NativeBridge.pressJump()
                            haptic(false)
                        }
                    }
                    Role.RELOAD -> { NativeBridge.pressReload(); haptic(false) }
                    Role.CROUCH -> { NativeBridge.toggleCrouch(); haptic(false) }
                    Role.SPRINT -> { NativeBridge.toggleSprint(); haptic(false) }
                    Role.MELEE -> { NativeBridge.pressMelee(); haptic(false) }
                    Role.MEDKIT -> { healHeld = true; haptic(false) }
                    Role.REVIVE -> { reviveHeld = true; haptic(false) }
                    Role.EMOTE -> { emoteOpen = !emoteOpen; haptic(false) }
                    Role.WEAPON_SLOT -> { weaponSlotTap(x, y); haptic(false) }
                    Role.EMOTE_ITEM -> { sendEmote(emoteItemAt(x, y)); emoteOpen = false; haptic(false) }
                    Role.GRENADE -> {
                        // Segurar mostra a trajetória; arrastar até o X cancela; soltar arremessa.
                        if (grenadeCount <= 0) message("SEM GRANADAS NA MOCHILA", Palette.muted)
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
                            val moved = HudGestures.stick(stickBaseX, stickBaseY, x, y, stickRadius)
                            stickBaseX = moved.baseX; stickBaseY = moved.baseY
                            stickX = moved.knobX; stickY = moved.knobY
                            NativeBridge.setMove(moved.moveX, moved.moveY)
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
                        Role.FIRE, Role.FIRE_LEFT -> if (!HudGestures.fireHeld(touches.values.map {
                            when (it.role) { Role.FIRE -> HudControl.FIRE; Role.FIRE_LEFT -> HudControl.FIRE_LEFT; else -> null }
                        })) NativeBridge.setFire(false)
                        Role.GRENADE -> NativeBridge.setThrowAim(false, HudGestures.throwCancelled(t.cancel, event.actionMasked == MotionEvent.ACTION_CANCEL))
                        Role.MEDKIT -> healHeld = touches.values.any { it.role == Role.MEDKIT }
                        Role.REVIVE -> reviveHeld = touches.values.any { it.role == Role.REVIVE }
                        else -> Unit
                    }
                }
            }
        }
        return true
    }

    private fun cancelSpot(): Pair<Float, Float> {
        val gc = g(HudControl.GRENADE)
        val up = gc.y - gc.r * 2.6f
        return gc.x to if (up > d(40f)) up else gc.y + gc.r * 2.6f
    }

    // ------------------------------------------------------------------ desenho

    override fun onDraw(canvas: Canvas) {
        if (editing) { drawEditor(canvas); return }
        NativeBridge.copyHud(hud)
        updatePresentation()
        val currentWeapon = hud[Hud.WEAPON].toInt()
        if (aimWeapon != currentWeapon) {
            aimWeapon = currentWeapon; aimStage = 0; aiming = false
            NativeBridge.setAimStage(0)
        }
        val w = width.toFloat()
        val h = height.toFloat()
        val health = hud[Hud.HEALTH]
        val maxHealth = hud[Hud.MAX_HEALTH].coerceAtLeast(1f)
        if (lastHealth >= 0 && health < lastHealth - 2f) {
            haptic(true); hurtUntil = SystemClock.elapsedRealtime() + 450
        }
        lastHealth = health
        drawVignette(canvas, w, h, health / maxHealth)
        drawDamageDirection(canvas, w, h)
        drawWorldLabels(canvas, w, h)
        if (flight == 3 || editing) {
            drawReticle(canvas, w, h)
            drawStatus(canvas, health, maxHealth)
            drawThrowAim(canvas, w, h)
        }
        if (available(HudControl.MAP)) drawMinimap(canvas)
        drawBattleStatus(canvas, w, h)
        drawFeed(canvas, w, h)
        drawTip(canvas, w, h)
        drawControls(canvas)
        if (!editing) {
            if (flight == 0) drawPlaneJumpButton(canvas, w, h)
            else if (flight in 1..2) drawDescentControls(canvas, w, h)
        }
        if (showFps) {
            textPaint.textAlign = Paint.Align.RIGHT
            textPaint.textSize = d(10f); textPaint.color = Palette.muted
            canvas.drawText("${hud[Hud.FPS].toInt()} FPS", w - d(10f), h - d(5f), textPaint)
            textPaint.textAlign = Paint.Align.CENTER
        }
        postInvalidateOnAnimation()
    }

    private fun drawVignette(canvas: Canvas, w: Float, h: Float, ratio: Float) {
        val flash = ((hurtUntil - SystemClock.elapsedRealtime()) / 450f).coerceIn(0f, 1f)
        val low = if (ratio < 0.3f) (0.35f + 0.25f * sin(System.currentTimeMillis() / 180.0).toFloat()) else 0f
        val a = (flash * 0.85f + low).coerceIn(0f, 1f)
        if (a <= 0.01f) return
        vignettePaint.alpha = (a * 255).toInt()
        canvas.drawRect(0f, 0f, w, h, vignettePaint)
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
        val bars = hud[Hud.BAR_COUNT].toInt()
        for (i in 0 until bars) {
            val o = Hud.BARS + i * 5
            val x = hud[o] * w
            val y = hud[o + 1] * h
            val f = hud[o + 2].coerceIn(0f, 1f)
            val a = hud[o + 3].coerceIn(0f, 1f)
            val bw = d(38f)
            val bh = d(5f)
            p.style = Paint.Style.FILL
            p.color = Color.argb((a * 170).toInt(), 0, 0, 0)
            rect.set(x - bw / 2 - d(1f), y - d(1f), x + bw / 2 + d(1f), y + bh + d(1f))
            canvas.drawRoundRect(rect, d(2f), d(2f), p)
            p.color = if (f > 0.5f) Color.argb((a * 255).toInt(), 168, 255, 62) else Color.argb((a * 255).toInt(), 255, 90, 70)
            rect.set(x - bw / 2, y, x - bw / 2 + bw * f, y + bh)
            canvas.drawRoundRect(rect, d(2f), d(2f), p)
        }
        val labels = hud[Hud.LABEL_COUNT].toInt()
        for (i in 0 until labels) {
            val o = Hud.LABELS + i * 5
            val kind = hud[o + 2].toInt()
            val value = hud[o + 3].toInt()
            val a = hud[o + 4].coerceIn(0f, 1f)
            textPaint.textSize = d(if (kind == 1 || kind == 2) 21f else 16f)
            val color = when (kind) { 1, 2 -> Palette.gold; 3 -> Palette.lime; else -> Color.WHITE }
            val label = when (kind) { 3 -> "+$value"; 1, 2 -> "$value!"; else -> "$value" }
            textPaint.color = Color.argb((a * 140).toInt(), 0, 0, 0)
            canvas.drawText(label, hud[o] * w + d(1.5f), hud[o + 1] * h + d(1.5f), textPaint)
            textPaint.color = Color.argb((a * 255).toInt(), Color.red(color), Color.green(color), Color.blue(color))
            canvas.drawText(label, hud[o] * w, hud[o + 1] * h, textPaint)
        }
    }

    private fun drawReticle(canvas: Canvas, w: Float, h: Float) {
        val cx = w / 2
        val cy = h / 2
        val ready = hud[Hud.WEAPON_READY] > 0.5f
        val locked = hud[Hud.TARGET_LOCKED] > 0.5f
        val color = when {
            !ready -> Color.rgb(140, 140, 140)
            locked -> Palette.red
            else -> Color.WHITE
        }
        // As texturas do template têm 256 px; aqui a mira inteira ocupa 60 dp.
        val size = d(60f)
        val k = size / 256f
        val type = WeaponSpecs.reticle(BattleItems.iconId(hud[Hud.WEAPON].toInt()))
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
            val f = hud[Hud.TARGET_HP].coerceIn(0f, 1f)
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
            p.style = Paint.Style.STROKE
            p.strokeCap = Paint.Cap.ROUND
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
            p.style = Paint.Style.FILL
        }

    }

    private fun drawMinimap(canvas: Canvas) {
        val left = mapX - mapSize / 2
        val top = mapY - mapSize / 2
        val bounds = RectF(left, top, left + mapSize, top + mapSize)
        p.style = Paint.Style.FILL; p.color = Color.rgb(35, 54, 43)
        canvas.drawRoundRect(bounds, d(MAP_CORNER), d(MAP_CORNER), p)
        canvas.save()
        path.reset(); path.addRoundRect(bounds, d(MAP_CORNER), d(MAP_CORNER), Path.Direction.CW)
        canvas.clipPath(path)
        minimap?.let { canvas.drawBitmap(it, null, bounds, null) }
        val state = battle
        val me = own()
        if (state != null && me >= 0) {
            fun mx(value: Float) = left + value / 1440f * mapSize
            fun mz(value: Float) = top + value / 1440f * mapSize
            p.style = Paint.Style.STROKE; p.strokeWidth = d(1f); p.color = Color.WHITE
            val zoneRevealed = state[11] > 0f || state[12] > 0f || state[1].toInt() == 2 || (state[1].toInt() == 1 && state[me + 11].toInt() == 3 && state[22] >= 30f)
            if (zoneRevealed) {
                val waitProg = if (state[1].toInt() == 2) (state[22] / (state[22] + state[10]).coerceAtLeast(30f)).coerceIn(0f, 1f) else 0f
                val isInitialWait = state[11].toInt() == 0 && state[12] <= 0f
                val zx = if (isInitialWait) 720f else state[4]
                val zz = if (isInitialWait) 720f else state[5]
                val zr = if (isInitialWait) (1050f + (state[6] - 1050f) * waitProg) else state[6]
                p.style = Paint.Style.STROKE; p.strokeWidth = d(1.5f); p.color = Palette.cyan
                canvas.drawCircle(mx(zx), mz(zz), zr / 1440f * mapSize, p)
                p.strokeWidth = d(1f); p.color = Color.WHITE
                canvas.drawCircle(mx(state[7]), mz(state[8]), state[9] / 1440f * mapSize, p)
    
            }
            p.style = Paint.Style.FILL

            p.style = Paint.Style.FILL
            repeat(state[16].toInt()) {
                val at = 24 + it * 32
                if (state[at + 1] == state[me + 1] && (state[at + 3].toInt() and 1) != 0) {
                    p.color = if (at == me) Color.WHITE else Palette.lime
                    val x = mx(if (at == me && hud[Hud.PLAYER_X] > 0f) hud[Hud.PLAYER_X] else state[at + 4]); val z = mz(if (at == me && hud[Hud.PLAYER_Z] > 0f) hud[Hud.PLAYER_Z] else state[at + 6])
                    if (at == me) {
                        canvas.save(); canvas.rotate(Math.toDegrees(hud[Hud.CAM_YAW].toDouble()).toFloat(), x, z)
                        val r = d(4f)
                        path.reset(); path.moveTo(x, z - r); path.lineTo(x + r * .7f, z + r * .7f)
                        path.lineTo(x, z + r * .3f); path.lineTo(x - r * .7f, z + r * .7f); path.close()
                        canvas.drawPath(path, p); canvas.restore()
                    } else canvas.drawCircle(x, z, d(2.5f), p)
                    if (state[at + 24] >= 0f) {
                        p.color = Palette.gold
                        canvas.drawCircle(mx(state[at + 24]), mz(state[at + 25]), d(2f), p)
                    }
                }
            }
        }
        canvas.restore()
        p.style = Paint.Style.STROKE; p.strokeWidth = d(1.5f); p.color = RING
        canvas.drawRoundRect(bounds, d(MAP_CORNER), d(MAP_CORNER), p)
        p.style = Paint.Style.FILL
    }

    private fun drawBattleStatus(canvas: Canvas, w: Float, h: Float) {
        val state = battle ?: return
        val me = own()
        if (me < 0) return
        textPaint.typeface = Fonts.bold; textPaint.textSize = d(10f); textPaint.color = Color.WHITE
        textPaint.setShadowLayer(d(2f), d(1f), d(1f), Color.BLACK)
        canvas.drawText("${state[21].toInt()} EQUIPES   •   ${state[me + 10].toInt()} ELIMINAÇÕES   •   $latency ms", w * .5f, d(18f), textPaint)
        textPaint.color = if (state[12] > 0f) Palette.gold else Color.WHITE
        val zoneRevealed = state[11] > 0f || state[12] > 0f || state[1].toInt() == 2 || (state[1].toInt() == 1 && state[me + 11].toInt() == 3 && state[22] >= 30f)
        textPaint.color = if (state[12] > 0f) Palette.gold else if (zoneRevealed) Palette.cyan else Color.WHITE
        val zoneText = if (!zoneRevealed) {
            if (state[me + 11].toInt() == 0) "AGUARDANDO SALTO • ZONA SEGURA EM BREVE"
            else "ZONA SEGURA APARECE EM ${kotlin.math.ceil((25f - state[2]).coerceAtLeast(1f)).toInt()}s"
        } else {
            "ZONA ${state[11].toInt() + 1}  ${if (state[12] > 0f) "REDUZINDO" else "FECHA EM"} ${kotlin.math.ceil(state[10].coerceAtLeast(0f)).toInt()}s"
        }
        canvas.drawText(zoneText, w * .5f, d(35f), textPaint)
        textPaint.color = Color.WHITE; textPaint.textSize = d(9f)
        canvas.drawText("COLETE ${state[me + 15].toInt()}  •  CAPACETE ${state[me + 16].toInt()}", w * .5f, h - d(27f), textPaint)
        if ((state[me + 3].toInt() and 2) != 0) canvas.drawText("DERRUBADO — aguarde um aliado", w * .5f, h * .42f, textPaint)
        if (state[me + 21] > 0f) canvas.drawText("CURANDO ${state[me + 21].toInt()}/3s", w * .5f, h * .58f, textPaint)
        if (reviveTarget >= 0 && !throwAiming) {
            p.color = PANEL; canvas.drawRoundRect(reviveRect(), d(8f), d(8f), p)
            textPaint.textSize = d(12f); textPaint.color = Palette.lime
            canvas.drawText("SEGURE PARA REANIMAR", w * .5f, h * .64f, textPaint)
        }
        textPaint.clearShadowLayer(); textPaint.typeface = Fonts.title
    }

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

    private fun statusBaselineCenter(): Float = height - d(14.7f)

    private fun drawFeed(canvas: Canvas, w: Float, h: Float) {
        val now = System.currentTimeMillis()
        val y = d(CombatFeed.noticeBaselineDp(h / d(1f)))
        val notice = combatFeed.current(now)
        if (notice != null) {
            val age = (now - notice.shownAtMs) / CombatFeed.DURATION_MS.toFloat()
            val a = if (age < 0.8f) 1f else ((1f - age) / 0.2f).coerceIn(0f, 1f)
            val scale = if (age < 0.08f) 1f + (0.08f - age) * 4f else 1f
            // Frases longas encolhem para caber na largura da tela.
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

    private fun k2(gc: Geo): Float = (gc.r / d(24f)).coerceIn(0.8f, 1.4f)

    private fun drawControls(canvas: Canvas) {
        val alpha = opacity
        val save = if (alpha < 0.99f) canvas.saveLayerAlpha(0f, 0f, width.toFloat(), height.toFloat(), (alpha * 255).toInt()) else -1
        if (editing) for (c in HudControl.entries) if (!g(c).visible) drawGhost(canvas, c)
        drawJoystick(canvas)
        val firing = touches.values.any { it.role == Role.FIRE || it.role == Role.FIRE_LEFT }
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
        if (flight == 3 || editing) drawClock(canvas)
        if (available(HudControl.HOTBAR)) drawHotbar(canvas)
        if (available(HudControl.BAG)) drawBag(canvas)
        if (save >= 0) canvas.restoreToCount(save)
    }

    // ------------------------------------------------------------------ inventário e arremesso

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
            val id = if (editing) listOf(3, 4, 25, 24, 23, 0)[i] else hud[Hud.INV + i * 2].toInt()
            val count = if (editing) (if (i in 2..3) 3 else 1) else hud[Hud.INV + i * 2 + 1].toInt()
            val pressed = touches.values.any { it.role == Role.HOTBAR && it.slot == i }
            val equipped = id != 0 && i == weaponSlot && BattleItems.isGun(id)
            if (equipped) {
                // arma em mãos: fundo um pouco mais claro (o aro dourado vem abaixo)
                p.color = Color.argb(50, 255, 255, 255)
                canvas.drawRoundRect(rect, d(5f), d(5f), p)
            }
            val border = when {
                pressed -> Color.WHITE
                id != 0 && i == weaponSlot && BattleItems.isGun(id) -> Palette.gold
                id != 0 && i == throwSlot && BattleItems.isThrowable(id) -> Color.rgb(255, 150, 60)
                else -> RING
            }
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(if (border == RING) 1.2f else 2.2f)
            p.color = border
            canvas.drawRoundRect(rect, d(5f), d(5f), p)
            p.style = Paint.Style.FILL
            if (id != 0 && count > 0) {
                drawItem(canvas, id, rect.centerX(), rect.centerY(), rect.width() * 0.82f)
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

    private fun iconUnit(gc: Geo): Float = gc.r * 0.62f / 24f

    private fun drawThrowAim(canvas: Canvas, w: Float, h: Float) {
        if (!throwAiming) return
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
        val text = if (over) "SOLTE PARA CANCELAR" else "SOLTE PARA LANÇAR A GRANADA • ARRASTE ATÉ ✕ PARA CANCELAR"
        textPaint.textSize = d(13f)
        val tw = textPaint.measureText(text) + d(24f)
        rect.set(w / 2 - tw / 2, h * 0.64f - d(18f), w / 2 + tw / 2, h * 0.64f + d(8f))
        p.color = Color.argb(190, 30, 14, 6)
        canvas.drawRoundRect(rect, d(12f), d(12f), p)
        textPaint.color = if (over) Palette.red else Color.rgb(255, 214, 120)
        canvas.drawText(text, w / 2, h * 0.64f, textPaint)

    }

    private fun k(c: HudControl): Float = g(c).r / d(c.baseDp)

    private fun label(canvas: Canvas, text: String, x: Float, y: Float, size: Float, color: Int) {
        textPaint.textSize = d(maxOf(size, 7f))   // legível mesmo com o controle reduzido
        textPaint.color = color
        canvas.drawText(text, x, y, textPaint)
    }

    private fun drawJoystick(canvas: Canvas) {
        if (!editing && flight == 0) return
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
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawJumpFigure(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, ICON)
    }

    private fun drawPlaneJumpButton(canvas: Canvas, w: Float, h: Float) {
        val cx = w - d(145f)
        val cy = h * 0.58f
        val btnW = d(180f)
        val btnH = d(60f)
        val left = cx - btnW / 2f
        val top = cy - btnH / 2f
        val right = cx + btnW / 2f
        val bottom = cy + btnH / 2f
        planeJumpRect.set(left, top, right, bottom)
        planeJumpTouchArea.set(left - d(20f), top - d(20f), right + d(20f), bottom + d(20f))

        val now = SystemClock.elapsedRealtime()
        val pulse = (sin(now * 0.0055f) * 0.2f + 0.8f)

        // 1. Halo luminoso pulsante amarelo âmbar
        p.style = Paint.Style.FILL
        p.color = Color.argb((55 * pulse).toInt(), 255, 195, 20)
        rect.set(left - d(8f), top - d(8f), right + d(8f), bottom + d(8f))
        canvas.drawRoundRect(rect, d(20f), d(20f), p)

        // 2. Fundo gradiente tático amarelo ouro / âmbar
        val pressed = touches.values.any { planeJumpTouchArea.contains(it.x, it.y) }
        val topC = if (pressed) Color.rgb(230, 145, 0) else Color.rgb(255, 200, 25)
        val botC = if (pressed) Color.rgb(180, 85, 0) else Color.rgb(225, 125, 0)
        p.shader = LinearGradient(left, top, left, bottom, topC, botC, Shader.TileMode.CLAMP)
        canvas.drawRoundRect(planeJumpRect, d(14f), d(14f), p)
        p.shader = null

        // 3. Moldura de precisão metálica
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(2.5f)
        p.color = if (pressed) Color.WHITE else Color.rgb(255, 245, 160)
        canvas.drawRoundRect(planeJumpRect, d(14f), d(14f), p)

        // Linha interna decorativa militar
        p.strokeWidth = d(1f)
        p.color = Color.argb(90, 0, 0, 0)
        rect.set(left + d(3f), top + d(3f), right - d(3f), bottom - d(3f))
        canvas.drawRoundRect(rect, d(11f), d(11f), p)
        p.style = Paint.Style.FILL

        // 4. Ícone de Paraquedista / Salto
        val iconCx = left + d(36f)
        val iconCy = cy
        val u = d(1.2f)

        // Paraquedas estilizado
        p.color = Color.rgb(30, 22, 10)
        rect.set(iconCx - 14f * u, iconCy - 15f * u, iconCx + 14f * u, iconCy - 1f * u)
        canvas.drawArc(rect, 180f, 180f, true, p)
        // Cordas do paraquedas
        p.style = Paint.Style.STROKE
        p.strokeWidth = 1.4f * u
        canvas.drawLine(iconCx - 12f * u, iconCy - 2f * u, iconCx, iconCy + 9f * u, p)
        canvas.drawLine(iconCx - 4f * u, iconCy - 2f * u, iconCx, iconCy + 9f * u, p)
        canvas.drawLine(iconCx + 4f * u, iconCy - 2f * u, iconCx, iconCy + 9f * u, p)
        canvas.drawLine(iconCx + 12f * u, iconCy - 2f * u, iconCx, iconCy + 9f * u, p)
        p.style = Paint.Style.FILL
        // Ponto do paraquedista
        canvas.drawCircle(iconCx, iconCy + 10.5f * u, 2.5f * u, p)

        // 5. Textos principais do botão
        textPaint.textAlign = Paint.Align.LEFT
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(19f)
        textPaint.color = Color.rgb(28, 20, 10)
        textPaint.setShadowLayer(d(1.2f), 0f, d(1f), Color.argb(100, 255, 255, 255))
        canvas.drawText("SALTAR", iconCx + d(22f), cy + d(3f), textPaint)
        textPaint.clearShadowLayer()

        textPaint.textSize = d(8.5f)
        textPaint.color = Color.rgb(65, 45, 15)
        canvas.drawText("DO AVIÃO", iconCx + d(24f), cy + d(15f), textPaint)

        // 6. Placa de Telemetria de Voo acima do botão
        val cardW = d(190f)
        val cardH = d(28f)
        val cardL = cx - cardW / 2f
        val cardT = top - d(36f)
        rect.set(cardL, cardT, cardL + cardW, cardT + cardH)
        p.color = Color.argb(190, 16, 20, 26)
        canvas.drawRoundRect(rect, d(6f), d(6f), p)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(1f)
        p.color = Color.argb(130, 255, 195, 20)
        canvas.drawRoundRect(rect, d(6f), d(6f), p)
        p.style = Paint.Style.FILL

        textPaint.textAlign = Paint.Align.CENTER
        textPaint.typeface = Fonts.title
        textPaint.textSize = d(10.5f)
        textPaint.color = Color.WHITE
        canvas.drawText("✈ SOBREVOANDO A ILHA", cx, cardT + d(18f), textPaint)
    }

    private fun drawDescentControls(canvas: Canvas, w: Float, h: Float) {
        val cx = w - d(145f)
        val cy = h * 0.58f
        val btnW = d(180f)
        val btnH = d(56f)
        val left = cx - btnW / 2f
        val top = cy - btnH / 2f
        val right = cx + btnW / 2f
        val bottom = cy + btnH / 2f

        val f = flight
        if (f == 1) { // Queda Livre (FREEFALL)
            chuteButtonRect.set(left, top, right, bottom)
            chuteTouchArea.set(left - d(20f), top - d(20f), right + d(20f), bottom + d(20f))

            val pressed = touches.values.any { chuteTouchArea.contains(it.x, it.y) }
            val now = SystemClock.elapsedRealtime()
            val pulse = (sin(now * 0.007f) * 0.2f + 0.8f)

            // Halo ciano
            p.style = Paint.Style.FILL
            p.color = Color.argb((60 * pulse).toInt(), 0, 180, 216)
            rect.set(left - d(6f), top - d(6f), right + d(6f), bottom + d(6f))
            canvas.drawRoundRect(rect, d(18f), d(18f), p)

            // Fundo ciano tático
            val topC = if (pressed) Color.rgb(0, 140, 180) else Color.rgb(0, 190, 225)
            val botC = if (pressed) Color.rgb(0, 95, 145) else Color.rgb(0, 130, 190)
            p.shader = LinearGradient(left, top, left, bottom, topC, botC, Shader.TileMode.CLAMP)
            canvas.drawRoundRect(chuteButtonRect, d(14f), d(14f), p)
            p.shader = null

            // Borda
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(2.2f)
            p.color = if (pressed) Color.WHITE else Color.rgb(180, 240, 255)
            canvas.drawRoundRect(chuteButtonRect, d(14f), d(14f), p)
            p.style = Paint.Style.FILL

            // Texto e Ícone
            textPaint.textAlign = Paint.Align.CENTER
            textPaint.typeface = Fonts.bold
            textPaint.textSize = d(15f)
            textPaint.color = Color.WHITE
            canvas.drawText("ABRIR PARAQUEDAS", cx, cy + d(1f), textPaint)
            textPaint.textSize = d(8.5f)
            textPaint.color = Color.argb(220, 230, 250, 255)
            canvas.drawText("TOQUE PARA PLANAR", cx, cy + d(13f), textPaint)
        } else if (f == 2) { // Paraquedas Aberto (CHUTE)
            chuteButtonRect.setEmpty()
            chuteTouchArea.setEmpty()
            val cardW = d(200f)
            val cardH = d(36f)
            val cardL = cx - cardW / 2f
            val cardT = cy - cardH / 2f
            rect.set(cardL, cardT, cardL + cardW, cardT + cardH)
            p.style = Paint.Style.FILL
            p.color = Color.argb(180, 16, 24, 32)
            canvas.drawRoundRect(rect, d(8f), d(8f), p)
            p.style = Paint.Style.STROKE
            p.strokeWidth = d(1.2f)
            p.color = Color.rgb(0, 180, 216)
            canvas.drawRoundRect(rect, d(8f), d(8f), p)
            p.style = Paint.Style.FILL

            textPaint.textAlign = Paint.Align.CENTER
            textPaint.typeface = Fonts.bold
            textPaint.textSize = d(12f)
            textPaint.color = Color.rgb(0, 220, 255)
            canvas.drawText("PARAQUEDAS ABERTO", cx, cardT + d(15f), textPaint)
            textPaint.textSize = d(9f)
            textPaint.color = Color.argb(220, 255, 255, 255)
            canvas.drawText("DIRECIONE O POUSO NO JOYSTICK", cx, cardT + d(28f), textPaint)
        }

        // Medidor de altitude do lado direito
        val alt = hud[Hud.PLAYER_Y].toInt().coerceAtLeast(0)
        val altCardW = d(110f)
        val altCardH = d(26f)
        val altL = cx - altCardW / 2f
        val altT = top - d(36f)
        rect.set(altL, altT, altL + altCardW, altT + altCardH)
        p.style = Paint.Style.FILL
        p.color = Color.argb(180, 10, 15, 20)
        canvas.drawRoundRect(rect, d(6f), d(6f), p)
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(1f)
        p.color = Color.argb(120, 255, 255, 255)
        canvas.drawRoundRect(rect, d(6f), d(6f), p)
        p.style = Paint.Style.FILL

        textPaint.textAlign = Paint.Align.CENTER
        textPaint.typeface = Fonts.bold
        textPaint.textSize = d(11f)
        textPaint.color = Palette.gold
        canvas.drawText("ALTITUDE: ${alt}m", cx, altT + d(17f), textPaint)
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
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawKnifeIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.2f, ICON)

    }

    private fun drawGrenade(canvas: Canvas) {
        val gc = g(HudControl.GRENADE)
        val count = grenadeCount
        val aiming = throwAiming
        // Aro externo claro como o botão de habilidade da referência (E17).
        p.style = Paint.Style.STROKE
        p.strokeWidth = d(3f)
        p.color = Color.argb(45, 255, 255, 255)
        canvas.drawCircle(gc.x, gc.y, gc.r + d(3.5f), p)
        p.style = Paint.Style.FILL
        drawButton(canvas, gc.x, gc.y, gc.r, Color.rgb(255, 170, 70), aiming)
        val color = if (count > 0) ICON else ICON_DIM
        drawGrenadeIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.15f, color)
        drawCount(canvas, gc, count)
    }

    private fun drawMedkit(canvas: Canvas) {
        val gc = g(HudControl.MEDKIT)
        val count = medkitCount
        drawButton(canvas, gc.x, gc.y, gc.r, Color.WHITE, false)
        drawMedkitIcon(canvas, gc.x, gc.y, iconUnit(gc) * 1.1f, if (count > 0) ICON else ICON_DIM)
        drawCount(canvas, gc, count)
    }

    // ------------------------------------------------------------------ painel da arma e emoji

    private var emoteOpen = false
    private val emotes = listOf("😀", "😡", "😱", "👍", "💀")

    private fun weaponSlotIndexes(): List<Int> =
        (0 until Inventory.SIZE).filter { i -> hud[Hud.INV + i * 2].toInt().let { it != 0 && BattleItems.isGun(it) && !BattleItems.isThrowable(it) } } + -1

    private fun weaponRows(): Int = (weaponSlotIndexes().size + 2) / 3

    private fun weaponRect(): RectF {
        val gc = g(HudControl.WEAPON)
        val s = k(HudControl.WEAPON)
        val top = gc.y - d(WEAPON_ROW_DP) * s
        return RectF(gc.x - d(80f) * s, top, gc.x + d(80f) * s, top + d(WEAPON_ROW_DP) * s * (1 + weaponRows()))
    }

    private fun weaponTopRect(): RectF {
        val r = weaponRect()
        return RectF(r.left, r.top, r.right, r.top + d(WEAPON_ROW_DP - 3f) * k(HudControl.WEAPON))
    }

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

    private fun weaponSlotTap(x: Float, y: Float) {
        val j = weaponSlotAt(x, y)
        if (j < 0) return
        val qi = weaponSlotIndexes()[j]
        if (qi < 0) NativeBridge.pressMelee() else NativeBridge.tapSlot(qi)
    }

    private fun drawWeaponPanel(canvas: Canvas) {
        val s = k(HudControl.WEAPON)
        val sel = hud[Hud.WEAPON_SLOT].toInt()
        val wid = hud[Hud.WEAPON].toInt()
        val isGun = wid != 0 && BattleItems.isGun(wid)
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
        if (wid != 0) drawItem(canvas, wid, top.right - d(42f) * s, top.top + d(19f) * s, d(54f) * s)
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
                drawItem(canvas, hud[Hud.INV + qi * 2].toInt(), rr.centerX(), rr.centerY(), rr.height() * 0.8f)
            }
            textPaint.textAlign = Paint.Align.LEFT
            textPaint.typeface = Fonts.title
            textPaint.textSize = d(8f) * s
            textPaint.color = Color.argb(if (selected) 170 else 110, 255, 255, 255)
            canvas.drawText("${j + 1}", rr.left + d(4f) * s, rr.top + d(10f) * s, textPaint)
            textPaint.textAlign = Paint.Align.CENTER
        }
    }

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
        grenadeCount = 3; medkitCount = 1
        hud[Hud.INV_ENABLED] = 1f
        hud[Hud.WEAPON] = 3f
        hud[Hud.WEAPON_SLOT] = 0f
        listOf(3, 4, 25, 24, 23, 0).forEachIndexed { i, id ->
            hud[Hud.INV + i * 2] = id.toFloat()
            hud[Hud.INV + i * 2 + 1] = if (id == 0) 0f else 1f
        }
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
        
        private val ICON_CUT = Color.rgb(48, 48, 46)
        private const val MAP_CORNER = 3f
        
        private const val HP_BAR_LEFT = 0.430f
        private const val HP_BAR_RIGHT = 0.595f
        
        private const val WEAPON_ROW_DP = 52f

        const val TOP_BAR_TOP_DP = 6f
        const val TOP_BAR_BOTTOM_DP = 36f
        const val COMPASS_TOP_DP = 40f
    }
}


/** Pure gesture calculations shared by the HUD and regression tests. */
internal object HudGestures {
    data class Stick(val baseX: Float, val baseY: Float, val knobX: Float, val knobY: Float, val moveX: Float, val moveY: Float)

    fun stick(baseX: Float, baseY: Float, x: Float, y: Float, radius: Float): Stick {
        val r = radius.coerceAtLeast(1f)
        var bx = baseX; var by = baseY
        var dx = x - bx; var dy = y - by
        val length = hypot(dx, dy)
        if (length > r * 1.6f) {
            val k = (length - r * 1.6f) / length
            bx += dx * k; by += dy * k
            dx = x - bx; dy = y - by
        }
        val distance = hypot(dx, dy)
        val clamped = min(distance, r)
        val nx = if (distance > 0f) dx / distance * clamped else 0f
        val ny = if (distance > 0f) dy / distance * clamped else 0f
        val dead = hypot(nx / r, ny / r) < .12f
        return Stick(bx, by, bx + nx, by + ny, if (dead) 0f else nx / r, if (dead) 0f else -ny / r)
    }

    fun joystickRegion(x: Float, y: Float, width: Float, height: Float, homeX: Float, alreadyHeld: Boolean): Boolean =
        !alreadyHeld && y > height * .28f && (if (homeX < width / 2f) x < width * .45f else x > width * .55f)

    fun throwCancelled(overCancel: Boolean, cancelledEvent: Boolean) = overCancel || cancelledEvent
    fun fireHeld(controls: List<HudControl?>) = controls.any { it == HudControl.FIRE || it == HudControl.FIRE_LEFT }
}

/** Server IDs remain authoritative; only legacy artwork/spec lookups translate them. */
object BattleItems {
    private val names = mapOf(1 to "Pistola", 2 to "SMG", 3 to "Fuzil", 4 to "Escopeta", 5 to "Precisão", 6 to "Faca", 10 to "Munição pistola", 11 to "Munição SMG", 12 to "Munição fuzil", 13 to "Cartuchos", 14 to "Munição precisão", 20 to "Colete", 21 to "Capacete", 22 to "Mochila", 23 to "Kit médico", 24 to "Bandagem", 25 to "Granada", 26 to "Fumaça", 27 to "Gelo", 28 to "Recuperação")
    fun name(id: Int) = names[id] ?: "Desarmado"
    fun short(id: Int) = if (id == 0) "—" else name(id).take(6)
    fun isGun(id: Int) = id in 1..5
    fun isThrowable(id: Int) = id in 25..26
    fun iconId(id: Int): Int = when (id) {
        1, 2, 3, 5 -> Items.RIFLE
        4 -> Items.SHOTGUN
        20 -> Items.ARMOR
        23 -> Items.MEDKIT
        24 -> Items.BANDAGE
        25 -> Items.GRENADE
        26 -> Items.SMOKE
        28 -> Items.ADRENALINE
        else -> 0
    }
}
