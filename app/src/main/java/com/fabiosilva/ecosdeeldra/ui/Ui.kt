package com.fabiosilva.ecosdeeldra.ui

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.PixelFormat
import android.graphics.RectF
import android.graphics.Typeface
import android.graphics.drawable.Drawable
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.animation.OvershootInterpolator
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/** Paleta e componentes visuais do jogo (estilo "blocos" inspirado em Minecraft). */
object Palette {
    val bg = Color.parseColor("#0A1120")
    val panel = Color.parseColor("#E6111A2E")
    val panelSoft = Color.parseColor("#CC1A2440")
    val line = Color.parseColor("#33A8FF3E")
    val lime = Color.parseColor("#A8FF3E")
    val limeDark = Color.parseColor("#5DA81E")
    // Cores usadas apenas pelas telas preservadas do lobby e do prêmio diário.
    val accent = Color.parseColor("#FFC400")
    val accentDark = Color.parseColor("#B38600")
    val cyan = Color.parseColor("#3CE6FF")
    val gold = Color.parseColor("#FFC83D")
    val red = Color.parseColor("#FF5A5A")
    val purple = Color.parseColor("#A877FF")
    val purpleDeep = Color.parseColor("#2A1466")
    val text = Color.parseColor("#F4F7FF")
    val muted = Color.parseColor("#9AA6C0")
    val dark = Color.parseColor("#0E1A08")
    val stone = Color.parseColor("#3A4258")

    // --- Identidade visual (docs/brand/IDENTIDADE.md): tokens ADITIVOS, nunca renomear ---
    /** Fundo mais fundo que [bg]: base de cartões e do selo. */
    val ink = Color.parseColor("#060B16")
    /** Perigo, Noite vermelha, vida crítica. */
    val danger = Color.parseColor("#FF3D3D")
    /** Linha de painel em destaque (versão forte de [line]). */
    val lineStrong = Color.parseColor("#66A8FF3E")
    /** Cor de cada contrato. */
    val contractSilent = Color.parseColor("#3CE6FF")
    val contractNest = Color.parseColor("#FF8A3D")
    val contractRedNight = Color.parseColor("#FF3D3D")
    val contractCrate = Color.parseColor("#FFC83D")
    val contractEndurance = Color.parseColor("#A8FF3E")
    /** Recursos: sucata (aço) e material infectado (verde-água). */
    val resScrap = Color.parseColor("#B8C4DA")
    val resInfected = Color.parseColor("#4FE3A0")
}

object Fonts {
    val title: Typeface = Typeface.create("sans-serif-black", Typeface.BOLD)
    val bold: Typeface = Typeface.create("sans-serif-condensed", Typeface.BOLD)
    val body: Typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
}

fun Context.dp(value: Float): Float = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, resources.displayMetrics)
fun Context.dpi(value: Int): Int = dp(value.toFloat()).toInt()
fun View.dpi(value: Int): Int = context.dpi(value)

/** Fundo de bloco com relevo: destaque em cima, sombra embaixo e contorno escuro. */
class BlockDrawable(
    private val ctx: Context,
    var fill: Int,
    private val radiusDp: Float = 6f,
    private val depthDp: Float = 4f,
    private val outline: Boolean = true,
) : Drawable() {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    private var pressed = false

    override fun draw(canvas: Canvas) {
        val b = bounds
        val r = ctx.dp(radiusDp)
        val depth = ctx.dp(depthDp)
        val inset = if (pressed) depth * 0.6f else 0f
        if (outline) {
            paint.color = Color.argb(200, 0, 0, 0)
            rect.set(b.left.toFloat(), b.top + inset, b.right.toFloat(), b.bottom.toFloat())
            canvas.drawRoundRect(rect, r, r, paint)
        }
        val o = if (outline) ctx.dp(2f) else 0f
        // sombra inferior
        paint.color = shade(fill, 0.55f)
        rect.set(b.left + o, b.top + o + inset, b.right - o, b.bottom - o)
        canvas.drawRoundRect(rect, r, r, paint)
        // face
        paint.color = if (pressed) shade(fill, 0.88f) else fill
        rect.set(b.left + o, b.top + o + inset, b.right - o, b.bottom - o - (depth - inset))
        canvas.drawRoundRect(rect, r, r, paint)
        // brilho superior
        paint.color = Color.argb(70, 255, 255, 255)
        rect.set(b.left + o + r * 0.4f, b.top + o + inset + ctx.dp(2f), b.right - o - r * 0.4f, b.top + o + inset + ctx.dp(5f))
        canvas.drawRoundRect(rect, r * 0.5f, r * 0.5f, paint)
    }

    override fun isStateful() = true
    override fun onStateChange(state: IntArray): Boolean {
        val now = state.contains(android.R.attr.state_pressed)
        if (now != pressed) {
            pressed = now
            invalidateSelf()
            return true
        }
        return false
    }

    override fun setAlpha(alpha: Int) = Unit
    override fun setColorFilter(colorFilter: ColorFilter?) = Unit
    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT

    companion object {
        fun shade(color: Int, f: Float): Int = Color.argb(
            Color.alpha(color),
            (Color.red(color) * f).toInt().coerceIn(0, 255),
            (Color.green(color) * f).toInt().coerceIn(0, 255),
            (Color.blue(color) * f).toInt().coerceIn(0, 255),
        )
    }
}

/** Painel translúcido com borda fina. */
class PanelDrawable(private val ctx: Context, private val fill: Int = Palette.panel, private val border: Int = Palette.line, private val radiusDp: Float = 10f) : Drawable() {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    override fun draw(canvas: Canvas) {
        val r = ctx.dp(radiusDp)
        rect.set(bounds)
        paint.style = Paint.Style.FILL
        paint.color = fill
        canvas.drawRoundRect(rect, r, r, paint)
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = ctx.dp(1.2f)
        paint.color = border
        rect.inset(paint.strokeWidth / 2, paint.strokeWidth / 2)
        canvas.drawRoundRect(rect, r, r, paint)
    }
    override fun setAlpha(alpha: Int) = Unit
    override fun setColorFilter(colorFilter: ColorFilter?) = Unit
    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT
}

/** Cartão com cantos arredondados, borda e realce no estado pressionado. */
class CardDrawable(
    private val ctx: Context,
    var fill: Int,
    var border: Int = Color.TRANSPARENT,
    private val radiusDp: Float = 10f,
    private val borderDp: Float = 1.3f,
) : Drawable() {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    private var pressed = false

    override fun draw(canvas: Canvas) {
        val b = bounds
        val r = ctx.dp(radiusDp)
        paint.style = Paint.Style.FILL
        paint.color = if (pressed) BlockDrawable.shade(fill, 0.8f) else fill
        rect.set(b.left.toFloat(), b.top.toFloat(), b.right.toFloat(), b.bottom.toFloat())
        canvas.drawRoundRect(rect, r, r, paint)
        if (border != Color.TRANSPARENT) {
            paint.style = Paint.Style.STROKE
            paint.strokeWidth = ctx.dp(borderDp)
            paint.color = if (pressed) BlockDrawable.shade(border, 0.8f) else border
            rect.inset(paint.strokeWidth / 2f, paint.strokeWidth / 2f)
            canvas.drawRoundRect(rect, r, r, paint)
        }
    }

    override fun isStateful() = true
    override fun onStateChange(state: IntArray): Boolean {
        val now = state.contains(android.R.attr.state_pressed)
        if (now != pressed) {
            pressed = now
            invalidateSelf()
            return true
        }
        return false
    }

    override fun setAlpha(alpha: Int) = Unit
    override fun setColorFilter(colorFilter: ColorFilter?) = Unit
    @Deprecated("Deprecated in Java")
    override fun getOpacity() = PixelFormat.TRANSLUCENT
}

/** Barra de progresso simples desenhada em Canvas. */
class ProgressBarView(context: Context, var color: Int = Palette.lime) : View(context) {
    var fraction = 0f
        set(value) {
            field = value.coerceIn(0f, 1f)
            invalidate()
        }
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val rect = RectF()
    override fun onDraw(canvas: Canvas) {
        val r = height / 2f
        rect.set(0f, 0f, width.toFloat(), height.toFloat())
        paint.color = Color.argb(140, 0, 0, 0)
        canvas.drawRoundRect(rect, r, r, paint)
        if (fraction > 0f) {
            rect.set(0f, 0f, width * fraction, height.toFloat())
            paint.color = color
            canvas.drawRoundRect(rect, r, r, paint)
        }
    }
}

object Ui {
    var clickSound: (() -> Unit)? = null

    fun text(ctx: Context, value: CharSequence, sizeSp: Float = 14f, color: Int = Palette.text, font: Typeface = Fonts.body, bold: Boolean = false): TextView =
        TextView(ctx).apply {
            text = value
            textSize = sizeSp
            setTextColor(color)
            typeface = if (bold) Fonts.bold else font
            includeFontPadding = false
            setShadowLayer(ctx.dp(1.5f), 0f, ctx.dp(1f), Color.argb(160, 0, 0, 0))
        }

    fun title(ctx: Context, value: CharSequence, sizeSp: Float = 22f, color: Int = Palette.text): TextView =
        text(ctx, value, sizeSp, color, Fonts.title).apply { letterSpacing = 0.03f }

    fun button(
        ctx: Context,
        label: CharSequence,
        fill: Int = Palette.lime,
        textColor: Int = Palette.dark,
        sizeSp: Float = 15f,
        onClick: () -> Unit,
    ): TextView = TextView(ctx).apply {
        text = label
        textSize = sizeSp
        typeface = Fonts.title
        letterSpacing = 0.04f
        gravity = Gravity.CENTER
        setTextColor(textColor)
        background = BlockDrawable(ctx, fill)
        setPadding(ctx.dpi(16), ctx.dpi(8), ctx.dpi(16), ctx.dpi(12))
        isClickable = true
        isFocusable = true
        setOnClickListener {
            clickSound?.invoke()
            onClick()
        }
    }

    fun setEnabledLook(view: View, enabled: Boolean) {
        view.isEnabled = enabled
        view.alpha = if (enabled) 1f else 0.45f
    }

    fun panel(ctx: Context, fill: Int = Palette.panel, border: Int = Palette.line): LinearLayout = LinearLayout(ctx).apply {
        orientation = LinearLayout.VERTICAL
        background = PanelDrawable(ctx, fill, border)
        setPadding(ctx.dpi(14), ctx.dpi(12), ctx.dpi(14), ctx.dpi(12))
    }

    fun row(ctx: Context, gravity: Int = Gravity.CENTER_VERTICAL): LinearLayout = LinearLayout(ctx).apply {
        orientation = LinearLayout.HORIZONTAL
        this.gravity = gravity
    }

    fun column(ctx: Context): LinearLayout = LinearLayout(ctx).apply { orientation = LinearLayout.VERTICAL }

    fun chip(ctx: Context, icon: String, value: String, iconColor: Int): TextView = text(ctx, "$icon $value", 14f, Palette.text, Fonts.title).apply {
        val span = android.text.SpannableString(text)
        span.setSpan(android.text.style.ForegroundColorSpan(iconColor), 0, icon.length, 0)
        text = span
        background = PanelDrawable(ctx, Color.parseColor("#CC0B1426"), Color.argb(80, Color.red(iconColor), Color.green(iconColor), Color.blue(iconColor)), 14f)
        setPadding(ctx.dpi(12), ctx.dpi(6), ctx.dpi(12), ctx.dpi(6))
    }


    fun iconButton(ctx: Context, icon: Icon, fill: Int = Palette.stone, iconColor: Int = Color.WHITE, onClick: () -> Unit): FrameLayout = FrameLayout(ctx).apply {
        background = BlockDrawable(ctx, fill, 6f, 3f)
        isClickable = true
        isFocusable = true
        addView(IconView(ctx, icon, iconColor), frame(ctx.dpi(22), ctx.dpi(22), Gravity.CENTER))
        setOnClickListener { clickSound?.invoke(); onClick() }
    }

    fun lp(w: Int, h: Int, weight: Float = 0f): LinearLayout.LayoutParams = LinearLayout.LayoutParams(w, h, weight)
    fun frame(w: Int, h: Int, gravity: Int = Gravity.NO_GRAVITY): FrameLayout.LayoutParams = FrameLayout.LayoutParams(w, h, gravity)
    const val MATCH = ViewGroup.LayoutParams.MATCH_PARENT
    const val WRAP = ViewGroup.LayoutParams.WRAP_CONTENT

    fun popIn(view: View) {
        view.scaleX = 0.85f
        view.scaleY = 0.85f
        view.alpha = 0f
        view.animate().scaleX(1f).scaleY(1f).alpha(1f).setDuration(260).setInterpolator(OvershootInterpolator(1.6f)).start()
    }

    fun pulse(view: View) {
        ValueAnimator.ofFloat(1f, 1.06f, 1f).apply {
            duration = 1400
            repeatCount = ValueAnimator.INFINITE
            addUpdateListener { val v = it.animatedValue as Float; view.scaleX = v; view.scaleY = v }
            start()
        }
    }

    /** Ignora toques que vazariam para a camada de baixo. */
    fun blockTouches(view: View) {
        view.setOnTouchListener { _, _: MotionEvent -> true }
    }
}

