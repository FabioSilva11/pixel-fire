package com.fabiosilva.ecosdeeldra.ui

import android.animation.ValueAnimator
import android.content.Context
import android.annotation.SuppressLint
import android.app.Activity
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.animation.LinearInterpolator
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import com.fabiosilva.ecosdeeldra.MainActivity
import com.fabiosilva.ecosdeeldra.engine.Hud
import com.fabiosilva.ecosdeeldra.engine.NativeBridge
import kotlin.math.cos
import kotlin.math.hypot

private fun dimmed(activity: Activity, alpha: Int = 190): FrameLayout = FrameLayout(activity).apply {
    setBackgroundColor(Color.argb(alpha, 4, 7, 14))
    isClickable = true
    isFocusable = true
}

/** Caixa de diálogo do jogo, sempre acima de tudo. */
object Popup {
    fun show(
        activity: Activity,
        title: String,
        message: String,
        ok: String,
        cancel: String? = null,
        onResult: ((Boolean) -> Unit)? = null,
    ) {
        val content = activity.findViewById<ViewGroup>(android.R.id.content)
        val layer = dimmed(activity)
        val panel = Ui.panel(activity)
        panel.gravity = Gravity.CENTER_HORIZONTAL
        panel.addView(Ui.title(activity, title, 20f, Palette.lime).apply { gravity = Gravity.CENTER })
        panel.addView(Ui.text(activity, message, 14f, Palette.text).apply {
            gravity = Gravity.CENTER
            setLineSpacing(0f, 1.2f)
            setPadding(0, activity.dpi(10), 0, activity.dpi(14))
        })
        val row = Ui.row(activity, Gravity.CENTER)
        fun close(result: Boolean) {
            content.removeView(layer)
            onResult?.invoke(result)
        }
        if (cancel != null) {
            row.addView(Ui.button(activity, cancel, Palette.stone, Color.WHITE, 13f) { close(false) }, Ui.lp(0, activity.dpi(48), 1f).apply { marginEnd = activity.dpi(10) })
        }
        row.addView(Ui.button(activity, ok, Palette.lime, Palette.dark, 14f) { close(true) }, Ui.lp(0, activity.dpi(48), 1f))
        panel.addView(row, Ui.lp(Ui.MATCH, Ui.WRAP))
        layer.addView(panel, Ui.frame(activity.dpi(440), Ui.WRAP, Gravity.CENTER))
        content.addView(layer, FrameLayout.LayoutParams(-1, -1))
        Ui.popIn(panel)
    }
}

@SuppressLint("ViewConstructor")
class LoadingOverlay(app: MainActivity) : FrameLayout(app) {
    init {
        setBackgroundColor(Color.BLACK)
        isClickable = true

        val column = LinearLayout(app).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL }
        column.addView(SpinnerView(app), Ui.lp(app.dpi(56), app.dpi(56)))
        column.addView(
            TextView(app).apply {
                text = "CARREGANDO"
                textSize = 14f
                setTextColor(Color.WHITE)
                typeface = Fonts.title
                letterSpacing = 0.25f
                gravity = Gravity.CENTER
                includeFontPadding = false
            },
            Ui.lp(Ui.WRAP, Ui.WRAP).apply { topMargin = app.dpi(22) },
        )
        addView(column, Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.CENTER))

        addView(
            TextView(app).apply {
                text = "INICIANDO..."
                textSize = 11f
                setTextColor(Color.argb(170, 255, 255, 255))
                typeface = Fonts.bold
                letterSpacing = 0.1f
                gravity = Gravity.CENTER
                includeFontPadding = false
            },
            Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL).apply { bottomMargin = app.dpi(42) },
        )
        addView(
            BarView(app),
            Ui.frame(Ui.MATCH, app.dpi(3), Gravity.BOTTOM).apply { setMargins(app.dpi(32), 0, app.dpi(32), app.dpi(22)) },
        )
    }
}

/** Indicador circular branco: arco que gira e pulsa de tamanho. Uma animação só, sem alocação por quadro. */
private class SpinnerView(context: Context) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; color = Color.WHITE }
    private val track = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.argb(40, 255, 255, 255) }
    private val rect = RectF()
    private var phase = 0f
    private val animator = ValueAnimator.ofFloat(0f, 1f).apply {
        duration = 1100
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener { phase = it.animatedValue as Float; invalidate() }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (!animator.isRunning) animator.start()
    }

    override fun onDetachedFromWindow() {
        animator.cancel()
        super.onDetachedFromWindow()
    }

    override fun onDraw(canvas: Canvas) {
        val stroke = resources.displayMetrics.density * 3f
        paint.strokeWidth = stroke
        track.strokeWidth = stroke
        rect.set(stroke / 2f, stroke / 2f, width - stroke / 2f, height - stroke / 2f)
        canvas.drawArc(rect, 0f, 360f, false, track)
        // Dois pulsos de tamanho por volta: o arco abre e fecha enquanto gira.
        val sweep = 50f + 230f * (0.5f - 0.5f * cos(phase * 4f * Math.PI.toFloat()))
        canvas.drawArc(rect, phase * 360f - 90f, sweep, false, paint)
    }
}

/** Barra fina e discreta com um trecho claro deslizando (progresso sem valor conhecido). */
private class BarView(context: Context) : View(context) {
    private val track = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(34, 255, 255, 255) }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.argb(210, 255, 255, 255) }
    private val rect = RectF()
    private var phase = 0f
    private val animator = ValueAnimator.ofFloat(0f, 1f).apply {
        duration = 1600
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener { phase = it.animatedValue as Float; invalidate() }
    }

    override fun onAttachedToWindow() {
        super.onAttachedToWindow()
        if (!animator.isRunning) animator.start()
    }

    override fun onDetachedFromWindow() {
        animator.cancel()
        super.onDetachedFromWindow()
    }

    override fun onDraw(canvas: Canvas) {
        val r = height / 2f
        rect.set(0f, 0f, width.toFloat(), height.toFloat())
        canvas.drawRoundRect(rect, r, r, track)
        val segment = width * 0.28f
        val x = -segment + (width + segment) * phase
        rect.set(x, 0f, x + segment, height.toFloat())
        canvas.save()
        canvas.clipRect(0f, 0f, width.toFloat(), height.toFloat())
        canvas.drawRoundRect(rect, r, r, fill)
        canvas.restore()
    }
}

@SuppressLint("ViewConstructor")
class PauseOverlay(app: MainActivity, subtitle: String) : FrameLayout(app) {
    init {
        setBackgroundColor(Color.argb(170, 4, 7, 14))
        isClickable = true
        val panel = Ui.panel(app)
        panel.gravity = Gravity.CENTER_HORIZONTAL
        panel.addView(Ui.title(app, "PAUSADO", 26f, Palette.lime).apply { gravity = Gravity.CENTER }, Ui.lp(Ui.MATCH, Ui.WRAP))
        panel.addView(Ui.text(app, subtitle, 13f, Palette.muted).apply { gravity = Gravity.CENTER; setPadding(0, app.dpi(4), 0, app.dpi(12)) })
        val buttons = listOf(
            Triple("CONTINUAR ▶", Palette.lime) { app.resumeMission() },
            Triple("AJUSTES", Palette.stone) { app.showPauseSettings() },
            Triple("VOLTAR AO LOBBY", Palette.red) { app.abandonMission(retry = false) },
        )
        for ((label, color, action) in buttons) {
            val dark = color == Palette.lime || color == Palette.cyan || color == Palette.gold
            panel.addView(Ui.button(app, label, color, if (dark) Palette.dark else Color.WHITE, 14f, action), Ui.lp(Ui.MATCH, app.dpi(42)).apply { bottomMargin = app.dpi(6) })
        }
        addView(panel, Ui.frame(app.dpi(320), Ui.WRAP, Gravity.CENTER))
        Ui.popIn(panel)
    }
}

@SuppressLint("ViewConstructor")
class DefeatOverlay(app: MainActivity) : FrameLayout(app) {
    init {
        setBackgroundColor(Color.argb(170, 40, 4, 8))
        isClickable = true
        val panel = Ui.panel(app).apply { gravity = Gravity.CENTER_HORIZONTAL }
        panel.addView(Ui.title(app, "VOCÊ CAIU!", 26f, Palette.red))
        panel.addView(Ui.button(app, "TENTAR DE NOVO", Palette.lime, Palette.dark) { app.abandonMission(true) }, Ui.lp(Ui.MATCH, app.dpi(44)).apply { topMargin = app.dpi(14) })
        panel.addView(Ui.button(app, "LOBBY", Palette.stone, Color.WHITE) { app.abandonMission(false) }, Ui.lp(Ui.MATCH, app.dpi(42)).apply { topMargin = app.dpi(8) })
        addView(panel, Ui.frame(app.dpi(320), Ui.WRAP, Gravity.CENTER))
    }
}

@SuppressLint("ViewConstructor")
class MatchResultOverlay(app: MainActivity, victory: Boolean, kills: Int, seconds: Int) : FrameLayout(app) {
    init {
        setBackgroundColor(Color.argb(190, 4, 7, 14))
        isClickable = true
        val panel = Ui.panel(app).apply { gravity = Gravity.CENTER_HORIZONTAL }
        panel.addView(Ui.title(app, if (victory) "EXTRAÇÃO CONCLUÍDA" else "PARTIDA ENCERRADA", 24f, Palette.lime))
        panel.addView(Ui.text(app, "$kills abates • %d:%02d".format(seconds / 60, seconds % 60), 15f, Palette.text))
        panel.addView(Ui.button(app, "JOGAR NOVAMENTE", Palette.lime, Palette.dark) { app.abandonMission(true) }, Ui.lp(Ui.MATCH, app.dpi(44)).apply { topMargin = app.dpi(14) })
        panel.addView(Ui.button(app, "LOBBY", Palette.stone, Color.WHITE) { app.abandonMission(false) }, Ui.lp(Ui.MATCH, app.dpi(42)).apply { topMargin = app.dpi(8) })
        addView(panel, Ui.frame(app.dpi(360), Ui.WRAP, Gravity.CENTER))
    }
}

/** Mapa da ilha inteira. Pausa o jogo; toque para fechar. */
@SuppressLint("ViewConstructor")
class MapOverlay(app: MainActivity, private val map: Bitmap?, private val worldSize: Float, onClose: () -> Unit) : FrameLayout(app) {
    private val hud = FloatArray(Hud.SIZE)
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { typeface = Fonts.title; textAlign = Paint.Align.CENTER; color = Color.WHITE }
    private val rect = RectF()
    private val path = Path()

    init {
        setBackgroundColor(Color.argb(225, 4, 7, 14))
        setWillNotDraw(false)
        setOnClickListener { onClose() }
        NativeBridge.copyHud(hud)
        val island = hud[Hud.ISLAND] > 0.5f
        addView(Ui.text(app, if (island) "MAPA DA ILHA • toque para voltar" else "MAPA • toque para voltar", 12f, Palette.lime, Fonts.title), Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.TOP or Gravity.CENTER_HORIZONTAL).apply { topMargin = app.dpi(10) })
        if (island) {
            val legend = "ninho (roxo)   ✕ destruído   cristal (ciano)   portal   ■ suprimentos   ▲ você"
            addView(Ui.text(app, legend, 11f, Palette.muted), Ui.frame(Ui.WRAP, Ui.WRAP, Gravity.BOTTOM or Gravity.CENTER_HORIZONTAL).apply { bottomMargin = app.dpi(8) })
        }
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val bmp = map ?: return
        val size = minOf(width * 0.9f, height - context.dp(70f))
        val left = (width - size) / 2
        val top = (height - size) / 2 + context.dp(6f)
        rect.set(left, top, left + size, top + size)
        paint.isFilterBitmap = true
        canvas.drawBitmap(bmp, null, rect, paint)
        val k = size / worldSize
        val px = hud[Hud.PLAYER_X]
        val pz = hud[Hud.PLAYER_Z]
        val island = hud[Hud.ISLAND] > 0.5f
        if (island) {
            val n = hud[Hud.OBJ_COUNT].toInt()
            for (i in 0 until n) {
                val o = Hud.OBJ + i * Hud.OBJ_STRIDE
                val x = left + hud[o] * k
                val y = top + hud[o + 1] * k
                val state = hud[o + 2].toInt()
                val s = context.dp(7f)
                when (state) {
                    1 -> {
                        path.reset()
                        path.moveTo(x, y - s); path.lineTo(x + s, y); path.lineTo(x, y + s); path.lineTo(x - s, y); path.close()
                        paint.color = Color.rgb(220, 70, 220)
                        canvas.drawPath(path, paint)
                        text.textSize = context.dp(10f)
                        val d = hypot(hud[o] - px, hud[o + 1] - pz)
                        canvas.drawText(if (d >= 1000f) "%.1f km".format(d / 1000f) else "${d.toInt()} m", x, y - s - context.dp(3f), text)
                    }
                    2 -> {
                        paint.color = Color.argb(200, 160, 160, 160)
                        paint.strokeWidth = context.dp(2.5f)
                        canvas.drawLine(x - s * 0.7f, y - s * 0.7f, x + s * 0.7f, y + s * 0.7f, paint)
                        canvas.drawLine(x + s * 0.7f, y - s * 0.7f, x - s * 0.7f, y + s * 0.7f, paint)
                    }
                    else -> {
                        paint.color = if (state == 4) Palette.purple else Color.argb(200, 140, 110, 200)
                        canvas.drawCircle(x, y, s, paint)
                        paint.color = Color.WHITE
                        canvas.drawCircle(x, y, s * 0.4f, paint)
                        text.textSize = context.dp(10f)
                        canvas.drawText(if (state == 4) "PORTAL ABERTO" else "PORTAL", x, y - s - context.dp(3f), text)
                    }
                }
            }
            // Cristais caídos e ainda no chão (losango ciano com a distância).
            val cn = hud[Hud.CRYSTAL_COUNT].toInt().coerceIn(0, Hud.MAX_CRYSTALS)
            for (c in 0 until cn) {
                val o = Hud.CRYSTAL_ITEMS + c * Hud.CRYSTAL_STRIDE
                val x = left + hud[o] * k
                val y = top + hud[o + 1] * k
                val s = context.dp(6f)
                path.reset()
                path.moveTo(x, y - s); path.lineTo(x + s, y); path.lineTo(x, y + s); path.lineTo(x - s, y); path.close()
                paint.color = Color.WHITE
                canvas.drawCircle(x, y, s + context.dp(1.5f), paint)
                paint.color = Palette.cyan
                canvas.drawPath(path, paint)
                text.textSize = context.dp(10f)
                val d = hypot(hud[o] - px, hud[o + 1] - pz)
                canvas.drawText("CRISTAL " + (if (d >= 1000f) "%.1f km".format(d / 1000f) else "${d.toInt()} m"), x, y - s - context.dp(3f), text)
            }
            // Caixa perdida: carga revelada (quadrado dourado com a distância).
            if (hud[Hud.CT_ID] > 0.5f && hud[Hud.CT_CARGO_STATE].toInt() == 2) {
                val x = left + hud[Hud.CT_CARGO_X] * k
                val y = top + hud[Hud.CT_CARGO_Z] * k
                val s = context.dp(6f)
                paint.color = Color.WHITE
                canvas.drawRect(x - s - context.dp(1.5f), y - s - context.dp(1.5f), x + s + context.dp(1.5f), y + s + context.dp(1.5f), paint)
                paint.color = Palette.contractCrate
                canvas.drawRect(x - s, y - s, x + s, y + s, paint)
                text.textSize = context.dp(10f)
                val d = hypot(hud[Hud.CT_CARGO_X] - px, hud[Hud.CT_CARGO_Z] - pz)
                canvas.drawText("CARGA " + (if (d >= 1000f) "%.1f km".format(d / 1000f) else "${d.toInt()} m"), x, y - s - context.dp(3f), text)
            }
            if (hud[Hud.SUPPLY_X] >= 0f) {
                paint.color = Color.rgb(120, 255, 90)
                val sx = left + hud[Hud.SUPPLY_X] * k
                val sy = top + hud[Hud.SUPPLY_Z] * k
                canvas.drawRect(sx - context.dp(4f), sy - context.dp(4f), sx + context.dp(4f), sy + context.dp(4f), paint)
            }
            // Escala: 1 km.
            val bar = 1000f * k
            paint.color = Color.WHITE
            canvas.drawRect(left + context.dp(10f), top + size - context.dp(14f), left + context.dp(10f) + bar, top + size - context.dp(11f), paint)
            text.textSize = context.dp(10f)
            text.textAlign = Paint.Align.LEFT
            canvas.drawText("1 km", left + context.dp(10f), top + size - context.dp(18f), text)
            text.textAlign = Paint.Align.CENTER
        } else {
            val alive = hud[Hud.ALIVE].toInt()
            val n = hud[Hud.ENEMY_COUNT].toInt()
            for (i in 0 until n) {
                val ex = hud[Hud.ENEMIES + i * 3]
                val ez = hud[Hud.ENEMIES + i * 3 + 1]
                if (alive > 3 && hypot(ex - px, ez - pz) > 40f && hud[Hud.ENEMIES + i * 3 + 2] < 0.5f) continue
                paint.color = Palette.red
                canvas.drawCircle(left + ex * k, top + ez * k, context.dp(3f), paint)
            }
        }
        val x = left + px * k
        val y = top + pz * k
        val yaw = hud[Hud.CAM_YAW]
        val s = context.dp(9f)
        canvas.save()
        canvas.rotate(Math.toDegrees(yaw.toDouble()).toFloat(), x, y)
        path.reset()
        path.moveTo(x, y - s); path.lineTo(x + s * 0.7f, y + s * 0.7f); path.lineTo(x, y + s * 0.3f); path.lineTo(x - s * 0.7f, y + s * 0.7f); path.close()
        paint.color = Color.WHITE
        canvas.drawPath(path, paint)
        canvas.restore()
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = context.dp(2f)
        paint.color = Palette.line
        canvas.drawRect(rect, paint)
        paint.style = Paint.Style.FILL
    }
}

@Suppress("unused")
private fun LinearLayout.spacer(size: Int) = addView(View(context), LinearLayout.LayoutParams(size, size))
