package com.fabiosilva.ecosdeeldra.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.view.View
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/** Geometrias utilizadas pelos painéis de equipamento. */
object Shapes {
    fun chamfer(path: Path, l: Float, t: Float, r: Float, b: Float, c: Float) {
        path.reset()
        path.moveTo(l + c, t); path.lineTo(r, t); path.lineTo(r, b - c)
        path.lineTo(r - c, b); path.lineTo(l, b); path.lineTo(l, t + c); path.close()
    }

    fun slanted(path: Path, l: Float, t: Float, r: Float, b: Float, skew: Float) {
        path.reset()
        path.moveTo(l + skew, t); path.lineTo(r, t); path.lineTo(r - skew, b); path.lineTo(l, b); path.close()
    }

    fun chevron(path: Path, l: Float, t: Float, r: Float, b: Float, tip: Float) {
        path.reset()
        path.moveTo(l, t); path.lineTo(r - tip, t); path.lineTo(r, (t + b) / 2f)
        path.lineTo(r - tip, b); path.lineTo(l, b); path.lineTo(l + tip, (t + b) / 2f); path.close()
    }
}

/** Ícones da interface, todos desenhados em vetor (sem imagens). */
enum class Icon {
    GEAR, BAG, CLOSE, HELMET, VEST, FIST, PLUS,
}

object Icons {
    private val path = Path()
    private val rect = RectF()

    /**
     * Desenha [icon] centrado em ([cx], [cy]). [s] é a metade do lado da caixa do ícone.
     * O [paint] é reaproveitado: estilo e cor são redefinidos aqui e o estilo volta a FILL no fim.
     */
    fun draw(canvas: Canvas, icon: Icon, cx: Float, cy: Float, s: Float, color: Int, paint: Paint) {
        fun x(u: Float) = cx + u * s
        fun y(v: Float) = cy + v * s
        fun fillMode() { paint.style = Paint.Style.FILL; paint.color = color }
        fun strokeMode(width: Float) {
            paint.style = Paint.Style.STROKE
            paint.strokeWidth = s * width
            paint.strokeCap = Paint.Cap.ROUND
            paint.strokeJoin = Paint.Join.ROUND
            paint.color = color
        }
        fun poly(vararg pts: Float) {
            path.reset()
            path.moveTo(x(pts[0]), y(pts[1]))
            var i = 2
            while (i < pts.size) { path.lineTo(x(pts[i]), y(pts[i + 1])); i += 2 }
            path.close()
        }

        when (icon) {
            Icon.CLOSE -> {
                strokeMode(0.3f)
                canvas.drawLine(x(-0.7f), y(-0.7f), x(0.7f), y(0.7f), paint)
                canvas.drawLine(x(0.7f), y(-0.7f), x(-0.7f), y(0.7f), paint)
            }
            Icon.PLUS -> {
                strokeMode(0.3f)
                canvas.drawLine(x(-0.7f), y(0f), x(0.7f), y(0f), paint)
                canvas.drawLine(x(0f), y(-0.7f), x(0f), y(0.7f), paint)
            }
            Icon.GEAR -> {
                fillMode()
                for (i in 0 until 8) {
                    canvas.save()
                    canvas.rotate(45f * i, cx, cy)
                    rect.set(x(-0.15f), y(-1f), x(0.15f), y(-0.55f))
                    canvas.drawRect(rect, paint)
                    canvas.restore()
                }
                strokeMode(0.3f)
                canvas.drawCircle(cx, cy, s * 0.52f, paint)
            }
            Icon.BAG -> {
                fillMode()
                rect.set(x(-0.68f), y(-0.5f), x(0.68f), y(0.95f))
                canvas.drawRoundRect(rect, s * 0.28f, s * 0.28f, paint)
                strokeMode(0.16f)
                rect.set(x(-0.32f), y(-1f), x(0.32f), y(-0.25f))
                canvas.drawArc(rect, 180f, 180f, false, paint)
                paint.style = Paint.Style.FILL
                paint.color = Color.argb(80, 0, 0, 0)
                rect.set(x(-0.42f), y(0.28f), x(0.42f), y(0.78f))
                canvas.drawRoundRect(rect, s * 0.12f, s * 0.12f, paint)
                canvas.drawRect(x(-0.68f), y(-0.12f), x(0.68f), y(-0.04f), paint)
            }
            Icon.FIST -> {
                fillMode()
                rect.set(x(-0.7f), y(-0.25f), x(0.75f), y(0.8f)); canvas.drawRoundRect(rect, s * 0.25f, s * 0.25f, paint)
                for (i in 0 until 4) {
                    rect.set(x(-0.7f + i * 0.37f), y(-0.75f), x(-0.38f + i * 0.37f), y(-0.1f))
                    canvas.drawRoundRect(rect, s * 0.12f, s * 0.12f, paint)
                }
                rect.set(x(-1f), y(-0.05f), x(-0.55f), y(0.45f)); canvas.drawRoundRect(rect, s * 0.14f, s * 0.14f, paint)
            }
            Icon.HELMET -> {
                fillMode()
                rect.set(x(-0.9f), y(-0.9f), x(0.9f), y(0.9f))
                canvas.drawArc(rect, 180f, 180f, true, paint)
                canvas.drawRect(x(-1f), y(0f), x(1f), y(0.2f), paint)
                canvas.drawRect(x(-0.82f), y(0.2f), x(-0.6f), y(0.75f), paint)
                canvas.drawRect(x(0.6f), y(0.2f), x(0.82f), y(0.75f), paint)
                paint.color = Color.argb(90, 0, 0, 0)
                canvas.drawRect(x(-0.7f), y(-0.35f), x(0.7f), y(-0.2f), paint)
            }
            Icon.VEST -> {
                fillMode()
                path.reset()
                path.moveTo(x(-0.8f), y(-0.85f)); path.lineTo(x(-0.35f), y(-0.85f))
                path.quadTo(x(0f), y(-0.3f), x(0.35f), y(-0.85f)); path.lineTo(x(0.8f), y(-0.85f))
                path.lineTo(x(0.92f), y(0.2f)); path.lineTo(x(0.62f), y(0.92f)); path.lineTo(x(-0.62f), y(0.92f)); path.lineTo(x(-0.92f), y(0.2f))
                path.close()
                canvas.drawPath(path, paint)
                paint.color = Color.argb(90, 0, 0, 0)
                canvas.drawRect(x(-0.04f), y(-0.2f), x(0.04f), y(0.9f), paint)
            }
        }
        paint.style = Paint.Style.FILL
    }
}

/** View simples que mostra um [Icon]. */
class IconView(context: Context, var icon: Icon, var color: Int = Color.WHITE) : View(context) {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)

    fun set(icon: Icon, color: Int) {
        this.icon = icon
        this.color = color
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        val s = min(width, height) / 2f * 0.92f
        Icons.draw(canvas, icon, width / 2f, height / 2f, s, color, paint)
    }
}
