package com.fabiosilva.ecosdeeldra.ui

import android.annotation.SuppressLint
import android.content.res.ColorStateList
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.os.SystemClock
import android.text.Layout
import android.text.StaticLayout
import android.text.TextPaint
import android.text.TextUtils
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewConfiguration
import android.widget.FrameLayout
import android.widget.SeekBar
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.ItemKind
import com.fabiosilva.ecosdeeldra.data.Items
import com.fabiosilva.ecosdeeldra.data.WeaponSpec
import com.fabiosilva.ecosdeeldra.data.WeaponSpecs
import com.fabiosilva.ecosdeeldra.data.WeaponTag
import com.fabiosilva.ecosdeeldra.engine.Hud
import kotlin.math.ceil
import kotlin.math.roundToInt

/**
 * Mochila no estilo da tela EQUIPAMENTO do Free Fire (referência: screenshots/freefire_124705.png).
 * Painel escuro translúcido à esquerda (o jogo pausado, ou o herói no menu, aparece à direita) com:
 * a coluna de equipamento (colete, capacete, mochila e os atalhos de arremesso e de kit médico), os cards das
 * armas (2 principais, pistola e faca), os atributos do que estiver selecionado embaixo e, à direita, as abas
 * ITEM / DISPOSITIVO com os itens agrupados por tipo. Em partida, mexer aqui pausa o jogo; as mudanças voltam
 * ao motor ao fechar e os itens descartados caem no chão à frente do jogador.
 */
@SuppressLint("ViewConstructor", "ClickableViewAccessibility")
class InventoryScreen(
    private val app: MainActivity,
    private val inventory: Inventory,
    private val inMatch: Boolean,
    /** Cópia do HUD do motor ao abrir (só em partida): munição de cada arma e colete em uso. */
    private val hud: FloatArray? = null,
    private val onClose: (useSlot: Int) -> Unit,
) : FrameLayout(app) {
    /** Itens descartados durante a partida (id, quantidade): ao fechar, caem no chão à frente do jogador. */
    val drops = ArrayList<Pair<Int, Int>>()
    private val explosives = 0
    private var dialog: View? = null
    private val board: Board

    // Tudo o que o Board usa é declarado acima deste init (o init já monta a tela).
    init {
        isClickable = true
        ItemIcons.initialize(app)
        if (inventory.fitLoadout()) commit()
        board = Board()
        addView(board, Ui.frame(Ui.MATCH, Ui.MATCH))
    }

    /** Fecha a mochila (volta ao jogo ou ao menu); com o diálogo de descarte aberto, fecha só o diálogo. */
    fun close() {
        if (dialog != null) {
            dismissDialog()
            return
        }
        inventory.normalize()
        onClose(-1)
    }

    /** Salva a mochila (em partida a cópia de trabalho também volta ao motor ao fechar). */
    private fun commit() {
        inventory.normalize()
        if (inventory !== app.progress.inventory) app.progress.inventory.copyFrom(inventory)
        app.save()
    }

    // ------------------------------------------------------------------ descarte com quantidade

    private fun dismissDialog() {
        dialog?.let { removeView(it) }
        dialog = null
    }

    private fun showDiscard(id: Int) {
        val def = Items.get(id) ?: return
        val total = inventory.count(id)
        if (total <= 0 || Items.isWeapon(id)) return
        dismissDialog()
        var amount = 1
        val layer = FrameLayout(app).apply {
            setBackgroundColor(Color.argb(170, 0, 0, 0))
            isClickable = true
            setOnClickListener { dismissDialog() }
        }
        val box = Ui.column(app).apply {
            background = PanelDrawable(app, Color.argb(246, 24, 24, 26), Color.argb(110, 255, 255, 255), 6f)
            setPadding(app.dpi(16), app.dpi(12), app.dpi(16), app.dpi(12))
            isClickable = true
        }
        box.addView(Ui.title(app, "DESCARTAR", 18f, Color.WHITE))
        val top = Ui.row(app).apply { setPadding(0, app.dpi(6), 0, app.dpi(4)) }
        top.addView(ItemIconView(app, id), Ui.lp(app.dpi(44), app.dpi(44)))
        val names = Ui.column(app).apply { setPadding(app.dpi(10), 0, 0, 0) }
        names.addView(Ui.text(app, def.name, 15f, def.color, Fonts.bold))
        names.addView(Ui.text(app, "Você tem $total", 12f, Palette.muted, Fonts.bold))
        top.addView(names, Ui.lp(0, Ui.WRAP, 1f))
        val countText = Ui.text(app, "", 20f, Color.WHITE, Fonts.title).apply { gravity = Gravity.CENTER }
        top.addView(countText, Ui.lp(app.dpi(90), Ui.WRAP))
        box.addView(top)

        val seek = SeekBar(app).apply {
            max = total - 1
            progress = 0
            progressTintList = ColorStateList.valueOf(GOLD)
            thumbTintList = ColorStateList.valueOf(GOLD)
            progressBackgroundTintList = ColorStateList.valueOf(Color.rgb(120, 120, 126))
            isEnabled = total > 1
        }
        fun refresh() {
            countText.text = "$amount/$total"
            if (seek.progress != amount - 1) seek.progress = amount - 1
        }
        seek.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                if (fromUser) {
                    amount = p + 1
                    countText.text = "$amount/$total"
                }
            }
            override fun onStartTrackingTouch(sb: SeekBar) {}
            override fun onStopTrackingTouch(sb: SeekBar) {}
        })
        val steps = Ui.row(app)
        steps.addView(Ui.button(app, "−", Palette.stone, Color.WHITE, 16f) { amount = (amount - 1).coerceAtLeast(1); refresh() }, Ui.lp(app.dpi(48), app.dpi(40)))
        steps.addView(seek, Ui.lp(0, Ui.WRAP, 1f))
        steps.addView(Ui.button(app, "+", Palette.stone, Color.WHITE, 16f) { amount = (amount + 1).coerceAtMost(total); refresh() }, Ui.lp(app.dpi(48), app.dpi(40)))
        steps.addView(Ui.button(app, "MÁX", Palette.stone, Color.WHITE, 12f) { amount = total; refresh() }, Ui.lp(Ui.WRAP, app.dpi(40)).apply { marginStart = app.dpi(6) })
        box.addView(steps)
        val hint = if (inMatch) "Os itens caem no chão à sua frente." else "Os itens descartados serão perdidos."
        box.addView(Ui.text(app, hint, 11f, Palette.muted, Fonts.bold).apply { setPadding(0, app.dpi(4), 0, app.dpi(8)) })

        val buttons = Ui.row(app)
        buttons.addView(Ui.button(app, "CANCELAR", Palette.stone, Color.WHITE, 13f) { dismissDialog() }, Ui.lp(0, app.dpi(42), 1f).apply { marginEnd = app.dpi(8) })
        buttons.addView(Ui.button(app, "DESCARTAR", Palette.red, Color.WHITE, 13f) {
            val removed = inventory.removeAmount(id, amount)
            if (inMatch && removed > 0) drops.add(id to removed)
            commit()
            dismissDialog()
            board.afterChange()
        }, Ui.lp(0, app.dpi(42), 1f))
        box.addView(buttons)
        refresh()

        layer.addView(box, Ui.frame(app.dpi(360), Ui.WRAP, Gravity.CENTER))
        addView(layer, Ui.frame(Ui.MATCH, Ui.MATCH))
        dialog = layer
        Ui.popIn(box)
    }

    // ------------------------------------------------------------------ painel desenhado

    private sealed class Sel {
        data class Weapon(val id: Int) : Sel()
        data class Item(val id: Int) : Sel()
        object Knife : Sel()
        object Vest : Sel()
        object Helmet : Sel()
        object Bag : Sel()
    }

    private class Hit(val rect: RectF, val action: () -> Unit)

    private class Pill(val label: String, val enabled: Boolean, val danger: Boolean = false, val action: () -> Unit)

    /** Item do lado direito: consumível agrupado por tipo ou arma guardada. */
    private class Entry(val id: Int, val count: Int, val weapon: Boolean)

    @SuppressLint("ViewConstructor")
    private inner class Board : View(app) {
        private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
        private val stroke = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE }
        private val text = TextPaint(Paint.ANTI_ALIAS_FLAG)
        private val path = Path()
        private val hits = ArrayList<Hit>()
        private val panel = RectF()
        private val gridRect = RectF()
        private var tab = 0          // 0 = ITEM, 1 = DISPOSITIVO
        private var sel: Sel = defaultSel()
        private var gridScroll = 0f
        private var gridMax = 0f
        private var toast: String? = null
        private var toastUntil = 0L
        private val slop = ViewConfiguration.get(app).scaledTouchSlop.toFloat()
        private var downX = 0f
        private var downY = 0f
        private var lastX = 0f
        private var lastY = 0f
        private var dragging = false
        private var gridDrag = false

        private fun defaultSel(): Sel {
            val id = inventory.slots.getOrNull(inventory.weaponSlot)?.id ?: 0
            return if (Items.isWeapon(id)) Sel.Weapon(id) else Sel.Knife
        }

        /** Depois de uma mudança na mochila: a seleção some se o item acabou. */
        fun afterChange() {
            val s = sel
            val ok = when (s) {
                is Sel.Weapon -> inventory.firstSlotOf(s.id) >= 0
                is Sel.Item -> inventory.count(s.id) > 0
                else -> true
            }
            if (!ok) sel = defaultSel()
            invalidate()
        }

        private fun done() {
            commit()
            app.audio.playUi("click", 0.4f)
            afterChange()
        }

        private fun showToast(msg: String) {
            toast = msg
            toastUntil = SystemClock.uptimeMillis() + 1800
            invalidate()
            postInvalidateDelayed(1850)
        }

        private fun select(s: Sel) {
            sel = s
            app.audio.playUi("click", 0.3f)
            invalidate()
        }

        /** Arremessável que o botão de arremesso usa (o escolhido ou, no automático, o primeiro da mochila). */
        private fun throwId(): Int {
            val ts = inventory.throwSlot
            val chosen = inventory.slots.getOrNull(ts)?.id ?: 0
            if (Items.isThrowable(chosen)) return chosen
            return inventory.slots.firstOrNull { Items.isThrowable(it.id) && !it.empty }?.id ?: 0
        }

        // ------------------------------------------------------------ desenho

        override fun onDraw(c: Canvas) {
            hits.clear()
            val h = height.toFloat()
            if (h <= 0f) return
            val left = h * 0.11f
            panel.set(left, 0f, left + h * 0.935f, h)
            // Fora do painel: em partida, tocar volta ao jogo.
            if (inMatch) hits.add(Hit(RectF(panel.right, 0f, width.toFloat(), h)) { close() })
            fill.color = PANEL
            c.drawRect(panel, fill)
            drawHandle(c, h, left)
            val eqL = left + h * 0.019f
            val eqR = left + h * 0.522f
            // Faixa do título EQUIPAMENTO e divisória entre o equipamento e os itens.
            fill.color = Color.argb(26, 255, 255, 255)
            c.drawRect(left, 0f, left + h * 0.531f, h * 0.052f, fill)
            fill.color = LINE
            c.drawRect(left, h * 0.052f, left + h * 0.531f, h * 0.052f + 1f, fill)
            c.drawRect(left + h * 0.531f, h * 0.012f, left + h * 0.531f + 1f, h * 0.985f, fill)
            label(c, "EQUIPAMENTO", (eqL + eqR) / 2f, h * 0.038f, h * 0.032f, TEXT, Paint.Align.CENTER)
            drawSlots(c, h, left)
            drawWeapons(c, h, left)
            drawInfo(c, h, eqL, eqR)
            drawItems(c, h, left)
            drawToast(c, h)
        }

        private fun drawHandle(c: Canvas, h: Float, left: Float) {
            val r = RectF(left - h * 0.045f, h * 0.44f, left - h * 0.008f, h * 0.56f)
            fill.color = Color.argb(190, 14, 14, 16)
            c.drawRoundRect(r, h * 0.008f, h * 0.008f, fill)
            fill.color = TEXT
            path.reset()
            path.moveTo(r.centerX() + h * 0.009f, r.centerY() - h * 0.016f)
            path.lineTo(r.centerX() - h * 0.009f, r.centerY())
            path.lineTo(r.centerX() + h * 0.009f, r.centerY() + h * 0.016f)
            path.close()
            c.drawPath(path, fill)
            hits.add(Hit(RectF(0f, h * 0.3f, left, h * 0.7f)) { close() })
        }

        // Coluna de equipamento: colete, capacete, mochila e os dois atalhos (arremesso e kit médico).
        private fun drawSlots(c: Canvas, h: Float, left: Float) {
            val x0 = left + h * 0.019f
            val x1 = x0 + h * 0.14f
            fun tile(i: Int) = RectF(x0, h * (0.063f + i * 0.1285f), x1, h * (0.063f + i * 0.1285f + 0.12f))

            // Colete: aceso quando há colete em uso na partida, com a barra de durabilidade.
            val armor = hud?.get(Hud.ARMOR) ?: 0f
            val armorMax = hud?.get(Hud.ARMOR_MAX) ?: 0f
            val vests = inventory.count(Items.ARMOR)
            var r = tile(0)
            tileBg(c, r, sel is Sel.Vest)
            outlineIcon(c, Icon.VEST, r.centerX(), r.centerY() - h * 0.004f, h * 0.04f, if (armor > 0.5f) VEST_LIT else null)
            if (armor > 0.5f && armorMax > 0f) bar(c, r.left + h * 0.012f, r.bottom - h * 0.016f, r.right - h * 0.012f, h * 0.007f, armor / armorMax, Color.rgb(120, 190, 255))
            if (vests > 0) label(c, "×$vests", r.right - h * 0.008f, r.top + h * 0.026f, h * 0.022f, TEXT, Paint.Align.RIGHT)
            hits.add(Hit(r) { select(Sel.Vest) })

            r = tile(1)
            tileBg(c, r, sel is Sel.Helmet)
            outlineIcon(c, Icon.HELMET, r.centerX(), r.centerY(), h * 0.038f, null)
            hits.add(Hit(r) { select(Sel.Helmet) })

            r = tile(2)
            tileBg(c, r, sel is Sel.Bag)
            outlineIcon(c, Icon.BAG, r.centerX(), r.centerY() - h * 0.006f, h * 0.04f, BAG_LIT)
            label(c, "${inventory.usedSlots()}/${Inventory.SIZE}", r.right - h * 0.008f, r.bottom - h * 0.01f, h * 0.02f, TEXT, Paint.Align.RIGHT)
            hits.add(Hit(r) { select(Sel.Bag) })

            // Atalhos (hexágonos com "+"): o arremessável do botão de arremesso e o kit médico do botão de cura.
            val tId = throwId()
            r = tile(3)
            hexSlot(c, r, h, tId, if (tId > 0) inventory.count(tId) else 0, sel is Sel.Item && (sel as Sel.Item).id == tId && tId > 0)
            hits.add(Hit(r) {
                if (tId > 0) select(Sel.Item(tId)) else {
                    tab = 1
                    gridScroll = 0f
                    showToast("Sem granadas na mochila")
                }
            })
            val kits = inventory.count(Items.MEDKIT)
            r = tile(4)
            hexSlot(c, r, h, if (kits > 0) Items.MEDKIT else 0, kits, sel is Sel.Item && (sel as Sel.Item).id == Items.MEDKIT)
            hits.add(Hit(r) {
                if (kits > 0) select(Sel.Item(Items.MEDKIT)) else {
                    tab = 0
                    gridScroll = 0f
                    showToast("Sem kit médico na mochila")
                }
            })
        }

        private fun tileBg(c: Canvas, r: RectF, selected: Boolean) {
            fill.color = TILE
            c.drawRect(r, fill)
            stroke.strokeWidth = if (selected) height * 0.004f else 1f
            stroke.color = if (selected) GOLD else LINE
            c.drawRect(r, stroke)
        }

        /** Ícone no estilo "contorno" do Free Fire (slot vazio) ou aceso na cor [lit]. */
        private fun outlineIcon(c: Canvas, icon: Icon, cx: Float, cy: Float, s: Float, lit: Int?) {
            if (lit != null) {
                Icons.draw(c, icon, cx, cy, s, lit, fill)
            } else {
                Icons.draw(c, icon, cx, cy, s * 1.1f, OUTLINE, fill)
                Icons.draw(c, icon, cx, cy, s * 0.94f, DARK, fill)
            }
        }

        private fun hexSlot(c: Canvas, r: RectF, h: Float, itemId: Int, count: Int, selected: Boolean) {
            val inset = h * 0.012f
            val l = r.left + inset
            val t = r.top + inset * 0.5f
            val rr = r.right - inset
            val b = r.bottom - inset * 0.5f
            val k = h * 0.026f
            path.reset()
            path.moveTo(l + k, t); path.lineTo(rr - k, t); path.lineTo(rr, t + k); path.lineTo(rr, b - k)
            path.lineTo(rr - k, b); path.lineTo(l + k, b); path.lineTo(l, b - k); path.lineTo(l, t + k); path.close()
            fill.color = Color.argb(215, 12, 14, 20)
            c.drawPath(path, fill)
            stroke.strokeWidth = if (selected) h * 0.004f else h * 0.003f
            stroke.color = if (selected) GOLD else OUTLINE
            c.drawPath(path, stroke)
            if (itemId > 0) {
                ItemIcons.draw(c, itemId, r.centerX(), r.centerY(), h * 0.075f)
                label(c, "$count", rr - h * 0.006f, b - h * 0.01f, h * 0.022f, TEXT, Paint.Align.RIGHT)
            } else {
                Icons.draw(c, Icon.PLUS, r.centerX(), r.centerY(), h * 0.02f, OUTLINE, fill)
            }
        }

        // Cards das armas: as 2 principais (fuzil e escopeta do template) e a faca.
        private fun drawWeapons(c: Canvas, h: Float, left: Float) {
            val x0 = left + h * 0.167f
            val x1 = left + h * 0.522f
            val primaries = inventory.primarySlots()
            weaponCard(c, h, RectF(x0, h * 0.063f, x1, h * 0.315f), primaries.getOrNull(0) ?: -1, Items.RIFLE, true)
            weaponCard(c, h, RectF(x0, h * 0.326f, x1, h * 0.576f), primaries.getOrNull(1) ?: -1, Items.SHOTGUN, true)

            // Faca (ataque corpo a corpo, sempre disponível).
            val r = RectF(x0, h * 0.589f, x1, h * 0.697f)
            fill.color = CARD
            c.drawRect(r, fill)
            outlineIcon(c, Icon.FIST, r.centerX(), r.centerY(), h * 0.034f, null)
            label(c, "∞", r.right - h * 0.01f, r.bottom - h * 0.012f, h * 0.03f, TEXT, Paint.Align.RIGHT)
            stroke.strokeWidth = if (sel is Sel.Knife) h * 0.003f else 1f
            stroke.color = if (sel is Sel.Knife) SELECT else LINE
            c.drawRect(r, stroke)
            hits.add(Hit(r) { select(Sel.Knife) })
        }

        private fun weaponCard(c: Canvas, h: Float, r: RectF, slot: Int, placeholder: Int, big: Boolean) {
            val id = inventory.slots.getOrNull(slot)?.id ?: 0
            val has = Items.isWeapon(id)
            val inHand = has && slot == inventory.weaponSlot
            val selected = has && sel == Sel.Weapon(id)
            if (inHand) {
                // Arma na mão: fundo dourado com listras diagonais, como o card selecionado do Free Fire.
                fill.shader = LinearGradient(r.left, 0f, r.right, 0f, intArrayOf(Color.argb(235, 22, 22, 22), Color.argb(235, 44, 34, 12), Color.argb(240, 176, 122, 8)), floatArrayOf(0f, 0.42f, 1f), Shader.TileMode.CLAMP)
                c.drawRect(r, fill)
                fill.shader = null
                c.save()
                c.clipRect(r)
                stroke.color = Color.argb(34, 0, 0, 0)
                stroke.strokeWidth = h * 0.004f
                var x = r.left + r.width() * 0.35f - r.height()
                while (x < r.right) {
                    c.drawLine(x, r.bottom, x + r.height(), r.top, stroke)
                    x += h * 0.014f
                }
                c.restore()
            } else {
                fill.color = CARD
                c.drawRect(r, fill)
            }
            if (has) {
                val def = Items.get(id)
                val band = if (big) h * 0.045f else h * 0.034f
                fill.color = Color.argb(150, 0, 0, 0)
                c.drawRect(r.left, r.top, r.right, r.top + band, fill)
                val size = if (big) h * 0.026f else h * 0.02f
                val ammo = ammoText(id)
                text.textSize = size
                text.typeface = Fonts.bold
                val ammoW = text.measureText(ammo)
                val name = fit(def?.name?.uppercase() ?: "", size, r.width() - ammoW - h * 0.04f)
                label(c, name, r.left + h * 0.012f, r.top + band * 0.72f, size, TEXT)
                label(c, ammo, r.right - h * 0.012f, r.top + band * 0.72f, size, TEXT, Paint.Align.RIGHT)
                val iconTop = r.top + band
                val iconBottom = if (big) r.bottom - h * 0.04f else r.bottom
                ItemIcons.draw(c, id, r.centerX(), (iconTop + iconBottom) / 2f, if (big) r.width() * 0.66f else r.width() * 0.5f, fitDrawing = true)
                if (big) {
                    // Traços da arma no lugar dos acessórios (zoom, perfuração, chumbos...).
                    var cx = r.left + h * 0.012f
                    for (trait in WeaponSpecs.get(id)?.traits.orEmpty().take(2)) {
                        text.textSize = h * 0.018f
                        val w = text.measureText(trait) + h * 0.02f
                        val chip = RectF(cx, r.bottom - h * 0.048f, cx + w, r.bottom - h * 0.01f)
                        fill.color = Color.argb(190, 22, 16, 34)
                        c.drawRect(chip, fill)
                        fill.color = PURPLE
                        c.drawRect(chip.left, chip.bottom - h * 0.004f, chip.right, chip.bottom, fill)
                        label(c, trait, chip.centerX(), chip.bottom - h * 0.013f, h * 0.018f, TEXT, Paint.Align.CENTER)
                        cx += w + h * 0.01f
                    }
                }
                hits.add(Hit(r) { select(Sel.Weapon(id)) })
            } else {
                // Espaço vazio: silhueta apagada, como no Free Fire.
                ItemIcons.draw(c, placeholder, r.centerX(), r.centerY(), if (big) r.width() * 0.66f else r.width() * 0.5f, 28, fitDrawing = true)
                hits.add(Hit(r) {
                    tab = 1
                    gridScroll = 0f
                    showToast(if (inventory.spareWeaponSlots().isEmpty()) "Nenhuma arma guardada" else "Escolha uma arma em DISPOSITIVO")
                })
            }
            stroke.strokeWidth = when {
                inHand -> h * 0.0045f
                selected -> h * 0.003f
                else -> 1f
            }
            stroke.color = when {
                inHand -> GOLD
                selected -> SELECT
                else -> LINE
            }
            c.drawRect(r, stroke)
            if (inHand && selected) {
                stroke.strokeWidth = h * 0.002f
                stroke.color = SELECT
                c.drawRect(r.left + h * 0.006f, r.top + h * 0.006f, r.right - h * 0.006f, r.bottom - h * 0.006f, stroke)
            }
        }

        /** "18/30": munição no pente (da cópia do HUD na partida) e o tamanho do pente com a melhoria. */
        private fun ammoText(id: Int): String {
            val base = Items.get(id)?.magazine ?: 0
            val h = hud ?: return "$base/$base"
            // Mesmo cálculo de Game::magazineFor: lround(base * max(1, magazine / 30)).
            val mag = (base * h[Hud.MAG_MUL].coerceAtLeast(1f)).roundToInt().coerceAtLeast(1)
            val clip = h[Hud.CLIPS + id - Items.RIFLE]
            val current = if (clip < 0f) mag else clip.toInt()
            return "$current/$mag"
        }

        // ------------------------------------------------------------ atributos (parte de baixo)

        private fun drawInfo(c: Canvas, h: Float, x0: Float, x1: Float) {
            val top = h * 0.718f
            val s = sel
            var title: String
            var tags: List<WeaponTag> = emptyList()
            var spec: WeaponSpec? = null
            var desc = ""
            when (s) {
                is Sel.Weapon -> {
                    spec = WeaponSpecs.get(s.id)
                    title = "${Items.get(s.id)?.name?.uppercase()} [${spec?.category}]"
                    tags = spec?.tags.orEmpty()
                }
                Sel.Knife -> {
                    spec = WeaponSpecs.get(WeaponSpecs.KNIFE)
                    title = "FACA [${spec?.category}]"
                    tags = spec?.tags.orEmpty()
                }
                is Sel.Item -> {
                    val def = Items.get(s.id)
                    title = "${def?.name?.uppercase()} [${kindName(def?.kind)}]"
                    tags = listOf(kindTag(def?.kind))
                    desc = "${def?.desc}\nNa mochila: ${inventory.count(s.id)}"
                    if (Items.isThrowable(s.id) && throwId() == s.id) desc += " • no botão de arremesso"
                }
                Sel.Vest -> {
                    title = "COLETE [EQUIPAMENTO]"
                    tags = listOf(WeaponTag("PROTEÇÃO", Color.rgb(79, 143, 230)))
                    val armor = hud?.get(Hud.ARMOR) ?: 0f
                    val armorMax = hud?.get(Hud.ARMOR_MAX) ?: 0f
                    desc = (if (armor > 0.5f) "Colete em uso: ${armor.roundToInt()}/${armorMax.roundToInt()} de proteção.\n" else "Nenhum colete em uso.\n") +
                        "Absorve 70% do dano até se desgastar. Coletes na mochila: ${inventory.count(Items.ARMOR)}."
                }
                Sel.Helmet -> {
                    title = "CAPACETE [EQUIPAMENTO]"
                    desc = "Nenhum capacete equipado."
                }
                Sel.Bag -> {
                    title = "MOCHILA [EQUIPAMENTO]"
                    desc = "Ocupando ${inventory.usedSlots()} de ${Inventory.SIZE} espaços.\nAs armas equipadas ficam nos cards; as outras ficam guardadas em DISPOSITIVO."
                }
            }

            // Botões de ação no canto direito do cabeçalho.
            val pills = pillsFor(s)
            var px = x1
            val pillH = h * 0.042f
            val pillSize = h * 0.021f
            for (p in pills.reversed()) {
                text.textSize = pillSize
                text.typeface = Fonts.bold
                val w = text.measureText(p.label) + h * 0.03f
                val r = RectF(px - w, top - h * 0.004f, px, top - h * 0.004f + pillH)
                Shapes.chamfer(path, r.left, r.top, r.right, r.bottom, h * 0.008f)
                fill.color = when {
                    !p.enabled -> Color.argb(150, 70, 70, 74)
                    p.danger -> Color.rgb(196, 64, 64)
                    else -> GOLD
                }
                c.drawPath(path, fill)
                label(c, p.label, r.centerX(), r.centerY() + pillSize * 0.36f, pillSize, if (!p.enabled) MUTED else if (p.danger) Color.WHITE else Color.rgb(28, 22, 4), Paint.Align.CENTER)
                if (p.enabled) hits.add(Hit(RectF(r.left, r.top - h * 0.015f, r.right, r.bottom + h * 0.015f), p.action))
                px -= w + h * 0.01f
            }

            // Cabeçalho: barra amarela + "NOME [CATEGORIA]".
            fill.color = GOLD
            c.drawRect(x0, top, x0 + h * 0.006f, top + h * 0.034f, fill)
            // Como no Free Fire o nome vem inteiro: a fonte encolhe um pouco antes de cortar com reticências.
            val titleRoom = px - x0 - h * 0.03f
            var titleSize = h * 0.03f
            text.typeface = Fonts.bold
            while (titleSize > h * 0.02f) {
                text.textSize = titleSize
                if (text.measureText(title) <= titleRoom) break
                titleSize *= 0.94f
            }
            title = fit(title, titleSize, titleRoom)
            label(c, title, x0 + h * 0.016f, top + h * 0.028f, titleSize, TEXT)

            val bodyTop = top + h * 0.05f
            val bodyBottom = h * 0.985f
            // Etiquetas coloridas à esquerda.
            var ty = bodyTop + h * 0.004f
            for (tag in tags.take(3)) {
                val r = RectF(x0, ty, x0 + h * 0.15f, ty + h * 0.036f)
                fill.color = tag.color
                c.drawRect(r, fill)
                label(c, fit(tag.label, h * 0.019f, r.width() - h * 0.008f), r.centerX(), r.centerY() + h * 0.007f, h * 0.019f, Color.WHITE, Paint.Align.CENTER)
                ty += h * 0.044f
            }
            val rx0 = x0 + h * 0.17f
            if (spec != null) {
                val rows = listOf(
                    Triple("Dano", spec.damage, spec.damage / WeaponSpecs.MAX_DAMAGE.toFloat()),
                    Triple("Taxa de Disparo", spec.fireRate, spec.fireRate / WeaponSpecs.MAX_FIRE_RATE.toFloat()),
                    Triple("Alcance", spec.range, spec.range / WeaponSpecs.MAX_RANGE.toFloat()),
                    Triple("Precisão", spec.accuracy, spec.accuracy / WeaponSpecs.MAX_ACCURACY.toFloat()),
                    Triple("Velocidade de movimento", spec.moveSpeed, spec.moveSpeed / WeaponSpecs.MAX_MOVE.toFloat()),
                )
                val rowH = (bodyBottom - bodyTop) / rows.size
                rows.forEachIndexed { i, (name, value, f) ->
                    val rowTop = bodyTop + i * rowH
                    val size = if (i == rows.lastIndex) h * 0.019f else h * 0.024f
                    label(c, name, rx0, rowTop + rowH * 0.58f, size, TEXT)
                    label(c, "$value", x1, rowTop + rowH * 0.58f, h * 0.024f, TEXT, Paint.Align.RIGHT)
                    bar(c, rx0, rowTop + rowH * 0.78f, x1, h * 0.0045f, f, Color.rgb(236, 236, 236))
                }
            } else if (desc.isNotEmpty()) {
                text.textSize = h * 0.024f
                text.typeface = Fonts.bold
                text.color = TEXT
                text.textAlign = Paint.Align.LEFT
                val width = (x1 - rx0).toInt().coerceAtLeast(1)
                val layout = StaticLayout.Builder.obtain(desc, 0, desc.length, text, width)
                    .setAlignment(Layout.Alignment.ALIGN_NORMAL)
                    .setLineSpacing(0f, 1.05f)
                    .build()
                c.save()
                c.clipRect(rx0, bodyTop, x1, bodyBottom)
                c.translate(rx0, bodyTop + h * 0.004f)
                layout.draw(c)
                c.restore()
            }
        }

        /** Ações do item selecionado (como os botões da mochila do Free Fire). */
        private fun pillsFor(s: Sel): List<Pill> = when (s) {
            is Sel.Weapon -> {
                val slot = inventory.firstSlotOf(s.id)
                val primaries = inventory.primarySlots()
                val equipped = slot in primaries
                if (equipped) {
                    val inHand = slot == inventory.weaponSlot
                    listOf(
                        Pill(if (inHand) "✓ NA MÃO" else "NA MÃO", !inHand) { inventory.holdWeapon(slot); done() },
                        Pill("GUARDAR", !inHand) {
                            if (!inventory.stowWeapon(slot)) showToast("Mochila cheia")
                            done()
                        },
                    )
                } else if (primaries.size < Inventory.MAX_PRIMARIES) {
                    listOf(Pill("EQUIPAR", slot >= 0) {
                        if (!inventory.equipWeapon(slot)) showToast("Sem espaço na barra rápida")
                        done()
                    })
                } else {
                    listOf(
                        Pill("TROCAR 1", slot >= 0) { inventory.equipWeapon(slot, primaries[0]); done() },
                        Pill("TROCAR 2", slot >= 0) { inventory.equipWeapon(slot, primaries[1]); done() },
                    )
                }
            }
            is Sel.Item -> {
                val def = Items.get(s.id)
                val list = ArrayList<Pill>()
                if (def?.kind == ItemKind.THROWABLE) {
                    val equipped = throwId() == s.id && inventory.throwSlot >= 0
                    list.add(Pill(if (equipped) "✓ EQUIPADA" else "EQUIPAR", !equipped) {
                        inventory.throwSlot = inventory.firstSlotOf(s.id)
                        done()
                    })
                } else if (inMatch) {
                    list.add(Pill("USAR", true) { useNow(inventory.firstSlotOf(s.id)) })
                }
                list.add(Pill("DESCARTAR", true, danger = true) { showDiscard(s.id) })
                list
            }
            Sel.Vest -> if (inMatch && inventory.count(Items.ARMOR) > 0) {
                listOf(Pill("VESTIR", true) { useNow(inventory.firstSlotOf(Items.ARMOR)) })
            } else emptyList()
            else -> emptyList()
        }

        /** Usa agora (só em partida): fecha a mochila e o motor usa o item ao retomar. */
        private fun useNow(slot: Int) {
            if (slot < 0) return
            inventory.normalize()
            onClose(slot)
        }

        private fun kindName(kind: ItemKind?): String = when (kind) {
            ItemKind.HEAL -> "CURA"
            ItemKind.BUFF -> "BUFF"
            ItemKind.THROWABLE -> "DISPOSITIVO"
            else -> "ITEM"
        }

        private fun kindTag(kind: ItemKind?): WeaponTag = when (kind) {
            ItemKind.HEAL -> WeaponTag("CURA", Color.rgb(93, 170, 70))
            ItemKind.BUFF -> WeaponTag("BUFF", Color.rgb(47, 179, 163))
            ItemKind.THROWABLE -> WeaponTag("ARREMESSO", Color.rgb(232, 135, 58))
            else -> WeaponTag("ITEM", Color.rgb(120, 120, 120))
        }

        // ------------------------------------------------------------ itens (lado direito)

        private fun entries(): List<Entry> = if (tab == 0) {
            Items.all.filter { (it.kind == ItemKind.HEAL || it.kind == ItemKind.BUFF) && inventory.count(it.id) > 0 }
                .map { Entry(it.id, inventory.count(it.id), false) }
        } else {
            Items.all.filter { it.kind == ItemKind.THROWABLE && inventory.count(it.id) > 0 }.map { Entry(it.id, inventory.count(it.id), false) } +
                inventory.spareWeaponSlots().map { Entry(inventory[it].id, 1, true) }
        }

        private fun drawItems(c: Canvas, h: Float, left: Float) {
            val sx0 = left + h * 0.539f
            val sx1 = left + h * 0.934f
            // Abas ITEM / DISPOSITIVO.
            listOf("ITEM", "DISPOSITIVO").forEachIndexed { k, name ->
                val tx = sx0 + k * (h * 0.136f)
                val r = RectF(tx, TABS_TOP * h, tx + h * 0.13f, (TABS_TOP + 0.05f) * h)
                Shapes.chamfer(path, r.left, r.top, r.right, r.bottom, h * 0.01f)
                fill.color = if (k == tab) TAB_ON else Color.argb(205, 24, 24, 26)
                c.drawPath(path, fill)
                if (k != tab) {
                    stroke.strokeWidth = 1.5f
                    stroke.color = Color.argb(150, 170, 170, 170)
                    c.drawPath(path, stroke)
                }
                label(c, name, r.centerX(), r.centerY() + h * 0.009f, h * 0.024f, if (k == tab) Color.rgb(28, 28, 28) else TEXT, Paint.Align.CENTER)
                hits.add(Hit(RectF(r.left, r.top - h * 0.01f, r.right, r.bottom + h * 0.01f)) {
                    if (tab != k) {
                        tab = k
                        gridScroll = 0f
                        app.audio.playUi("click", 0.3f)
                        invalidate()
                    }
                })
            }
            // Ocupação da mochila + ícone de camadas.
            label(c, "${inventory.usedSlots()}/${Inventory.SIZE}", sx1 - h * 0.052f, (TABS_TOP + 0.036f) * h, h * 0.026f, TEXT, Paint.Align.RIGHT)
            stroke.color = TEXT
            stroke.strokeWidth = h * 0.003f
            val ix = sx1 - h * 0.024f
            for (i in 0 until 3) {
                val iy = h * (TABS_TOP + 0.014f + i * 0.009f)
                path.reset()
                path.moveTo(ix - h * 0.017f, iy); path.lineTo(ix, iy - h * 0.008f); path.lineTo(ix + h * 0.017f, iy); path.lineTo(ix, iy + h * 0.008f); path.close()
                c.drawPath(path, stroke)
            }

            gridRect.set(sx0, (TABS_TOP + 0.065f) * h, sx1, h * 0.988f)
            val list = entries()
            val gap = h * 0.009f
            val cardW = (gridRect.width() - gap) / 2f
            val cardH = h * 0.154f
            val rows = ceil(list.size / 2f).toInt()
            gridMax = (rows * (cardH + gap) - gap - gridRect.height()).coerceAtLeast(0f)
            gridScroll = gridScroll.coerceIn(0f, gridMax)
            if (list.isEmpty()) {
                val msg = if (tab == 0) "Nenhum item. Mate zumbis e abra baús." else "Nenhuma granada ou arma guardada."
                label(c, msg, gridRect.centerX(), gridRect.top + h * 0.12f, h * 0.022f, MUTED, Paint.Align.CENTER)
                return
            }
            val tId = throwId()
            c.save()
            c.clipRect(gridRect)
            list.forEachIndexed { i, e ->
                val col = i % 2
                val row = i / 2
                val l = sx0 + col * (cardW + gap)
                val t = gridRect.top + row * (cardH + gap) - gridScroll
                val r = RectF(l, t, l + cardW, t + cardH)
                val strip = h * 0.038f
                fill.color = Color.argb(228, 32, 32, 34)
                c.drawRect(r, fill)
                fill.color = STRIP
                c.drawRect(r.left, r.bottom - strip, r.right, r.bottom, fill)
                val name = Items.get(e.id)?.name ?: ""
                label(c, fit(name, h * 0.02f, cardW - h * 0.012f), r.centerX(), r.bottom - strip * 0.32f, h * 0.02f, Color.rgb(214, 214, 214), Paint.Align.CENTER)
                val iconSpace = cardH - strip
                ItemIcons.draw(c, e.id, r.centerX(), r.top + iconSpace * 0.55f, minOf(cardW * 0.62f, iconSpace * 0.86f), fitDrawing = true)
                if (e.weapon) label(c, "ARMA", r.left + h * 0.01f, r.top + h * 0.03f, h * 0.018f, MUTED)
                else label(c, "${e.count}", r.left + h * 0.01f, r.top + h * 0.034f, h * 0.028f, TEXT)
                if (!e.weapon && e.id == tId) label(c, "EQUIPADA", r.right - h * 0.008f, r.top + h * 0.028f, h * 0.016f, GOLD, Paint.Align.RIGHT)
                val selected = if (e.weapon) sel == Sel.Weapon(e.id) else sel == Sel.Item(e.id)
                stroke.strokeWidth = if (selected) h * 0.004f else 1f
                stroke.color = if (selected) GOLD else Color.argb(40, 255, 255, 255)
                c.drawRect(r, stroke)
                val visible = RectF(r)
                if (visible.intersect(gridRect)) {
                    hits.add(Hit(visible) { select(if (e.weapon) Sel.Weapon(e.id) else Sel.Item(e.id)) })
                }
            }
            c.restore()
            if (gridMax > 0f) {
                // Barrinha de rolagem.
                val track = gridRect.height()
                val thumb = track * (track / (track + gridMax))
                val y = gridRect.top + (track - thumb) * (gridScroll / gridMax)
                fill.color = Color.argb(120, 255, 255, 255)
                c.drawRect(gridRect.right + h * 0.004f, y, gridRect.right + h * 0.008f, y + thumb, fill)
            }
        }

        private fun drawToast(c: Canvas, h: Float) {
            val msg = toast ?: return
            if (SystemClock.uptimeMillis() > toastUntil) {
                toast = null
                return
            }
            text.textSize = h * 0.028f
            text.typeface = Fonts.bold
            val w = text.measureText(msg) + h * 0.06f
            val r = RectF(panel.centerX() - w / 2f, h * 0.45f, panel.centerX() + w / 2f, h * 0.53f)
            fill.color = Color.argb(225, 10, 10, 12)
            c.drawRoundRect(r, h * 0.01f, h * 0.01f, fill)
            stroke.strokeWidth = 1.5f
            stroke.color = GOLD
            c.drawRoundRect(r, h * 0.01f, h * 0.01f, stroke)
            label(c, msg, r.centerX(), r.centerY() + h * 0.01f, h * 0.028f, TEXT, Paint.Align.CENTER)
        }

        // ------------------------------------------------------------ utilidades

        private fun label(c: Canvas, s: String, x: Float, y: Float, size: Float, color: Int, align: Paint.Align = Paint.Align.LEFT, font: Typeface = Fonts.bold) {
            text.textSize = size
            text.color = color
            text.textAlign = align
            text.typeface = font
            c.drawText(s, x, y, text)
        }

        private fun fit(s: String, size: Float, maxW: Float): String {
            text.textSize = size
            text.typeface = Fonts.bold
            return TextUtils.ellipsize(s, text, maxW.coerceAtLeast(1f), TextUtils.TruncateAt.END).toString()
        }

        private fun bar(c: Canvas, x0: Float, y: Float, x1: Float, thickness: Float, fraction: Float, color: Int) {
            fill.color = Color.argb(70, 255, 255, 255)
            c.drawRect(x0, y, x1, y + thickness, fill)
            fill.color = color
            c.drawRect(x0, y, x0 + (x1 - x0) * fraction.coerceIn(0f, 1f), y + thickness, fill)
        }

        // ------------------------------------------------------------ toque

        override fun onTouchEvent(e: MotionEvent): Boolean {
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    downX = e.x
                    downY = e.y
                    lastX = e.x
                    lastY = e.y
                    dragging = false
                    gridDrag = gridRect.contains(e.x, e.y)
                }
                MotionEvent.ACTION_MOVE -> {
                    if (!dragging && (kotlin.math.abs(e.x - downX) > slop || kotlin.math.abs(e.y - downY) > slop)) dragging = true
                    if (dragging) {
                        if (gridDrag && gridMax > 0f) {
                            gridScroll = (gridScroll - (e.y - lastY)).coerceIn(0f, gridMax)
                            invalidate()
                        }
                    }
                    lastX = e.x
                    lastY = e.y
                }
                MotionEvent.ACTION_UP -> if (!dragging) {
                    for (i in hits.indices.reversed()) {
                        if (hits[i].rect.contains(e.x, e.y)) {
                            hits[i].action()
                            break
                        }
                    }
                }
            }
            return true
        }
    }

    private companion object {
        val GOLD = Color.rgb(255, 193, 7)
        val PANEL = Color.argb(208, 14, 14, 16)
        val LINE = Color.argb(48, 255, 255, 255)
        val TILE = Color.argb(165, 30, 30, 34)
        val CARD = Color.argb(185, 28, 28, 30)
        val STRIP = Color.rgb(56, 58, 64)
        val OUTLINE = Color.rgb(92, 108, 134)
        val DARK = Color.rgb(12, 14, 19)
        val TEXT = Color.rgb(238, 238, 238)
        val MUTED = Color.rgb(150, 150, 150)
        val SELECT = Color.argb(220, 255, 255, 255)
        val TAB_ON = Color.rgb(224, 224, 224)
        val PURPLE = Color.rgb(176, 92, 255)
        val VEST_LIT = Color.rgb(157, 176, 216)
        val BAG_LIT = Color.rgb(196, 168, 112)
        /** Topo das abas ITEM/DISPOSITIVO (fração da altura): longe da faixa de gestos do topo da tela. */
        const val TABS_TOP = 0.066f
    }
}
