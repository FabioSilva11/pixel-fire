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

