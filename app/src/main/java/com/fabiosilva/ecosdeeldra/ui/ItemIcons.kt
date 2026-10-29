package com.fabiosilva.ecosdeeldra.ui

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Matrix
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.util.Xml
import android.view.View
import androidx.core.graphics.PathParser
import com.fabiosilva.ecosdeeldra.data.Items
import org.xmlpull.v1.XmlPullParser

/** Ícones dos itens desenhados em Canvas (sem imagens): usados no HUD, na mochila e no arsenal. */
object ItemIcons {
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)
    private val path = Path()
    private val rect = RectF()
    private data class SvgIcon(val path: Path, val left: Float, val top: Float, val width: Float, val height: Float) {
        /** Contorno real do desenho (o viewBox tem folga; a mochila usa isto para a arma encher o card). */
        val bounds = RectF().also { path.computeBounds(it, true) }
    }
    private val sourceIcons = HashMap<Int, SvgIcon>()
    private var sourceIconsLoaded = false

    /** Carrega os SVG originais do repositório de referência uma vez por processo. */
    fun initialize(context: Context) {
        if (sourceIconsLoaded) return
        sourceIconsLoaded = true
        listOf(
            Items.RIFLE to "icons/ak47.svg",
        ).forEach { (id, asset) ->
            runCatching { parseSvg(context, asset) }.getOrNull()?.let { sourceIcons[id] = it }
        }
    }

    private fun parseSvg(context: Context, file: String): SvgIcon? {
        val parser = Xml.newPullParser()
        var viewBox: FloatArray? = null
        var pathData: String? = null
        context.assets.open(file).use { input ->
            parser.setInput(input, "UTF-8")
            var event = parser.eventType
            while (event != XmlPullParser.END_DOCUMENT) {
                if (event == XmlPullParser.START_TAG) {
                    when (parser.name) {
                        "svg" -> {
                            val raw = parser.getAttributeValue(null, "viewBox")
                            val values = raw?.trim()?.split(Regex("[ ,]+"))?.mapNotNull { it.toFloatOrNull() }
                            if (values?.size == 4) viewBox = values.toFloatArray()
                        }
                        "path" -> if (pathData == null) pathData = parser.getAttributeValue(null, "d")
                    }
                }
                event = parser.next()
            }
        }
        val box = viewBox ?: return null
        val data = pathData ?: return null
        return SvgIcon(PathParser.createPathFromPathData(data), box[0], box[1], box[2], box[3])
    }

    /**
     * Desenha o item [id] centrado em (cx, cy) num quadrado de lado [size]. Com [fitDrawing], os ícones SVG
     * (fuzil, pistola, sniper) são ajustados pelo contorno do desenho em vez do viewBox (ficam maiores e centrados).
     */
    fun draw(canvas: Canvas, id: Int, cx: Float, cy: Float, size: Float, alpha: Int = 255, fitDrawing: Boolean = false) {
        sourceIcons[id]?.let { icon ->
            val b = icon.bounds
            val fit = fitDrawing && b.width() > 0f && b.height() > 0f
            val scale = if (fit) minOf(size / b.width(), size / b.height()) * 0.92f else minOf(size / icon.width, size / icon.height) * 0.86f
            val ox = if (fit) b.centerX() else icon.left + icon.width * 0.5f
            val oy = if (fit) b.centerY() else icon.top + icon.height * 0.5f
            val matrix = Matrix().apply {
                setScale(scale, scale)
                postTranslate(cx - ox * scale, cy - oy * scale)
            }
            val scaledPath = Path()
            icon.path.transform(matrix, scaledPath)
            paint.style = Paint.Style.FILL
            paint.color = Color.rgb(226, 232, 240)
            paint.alpha = alpha
            canvas.drawPath(scaledPath, paint)
            paint.alpha = 255
            return
        }
        val s = size / 32f
        val p = paint
        p.style = Paint.Style.FILL
        p.alpha = alpha
        fun box(l: Float, t: Float, r: Float, b: Float, color: Int, radius: Float = 1.5f) {
            p.color = color
            p.alpha = alpha
            rect.set(cx + l * s, cy + t * s, cx + r * s, cy + b * s)
            canvas.drawRoundRect(rect, radius * s, radius * s, p)
        }
        val metal = Color.rgb(70, 76, 88)
        val dark = Color.rgb(34, 36, 42)
        val wood = Color.rgb(150, 98, 52)
        val accent = Items.get(id)?.color ?: Color.WHITE
        when (id) {
            Items.RIFLE -> {
                box(-14f, -3f, 10f, 3f, metal)
                box(8f, -2f, 15f, 0f, dark)
                box(-15f, -2f, -9f, 4f, wood)
                box(-3f, 2f, 1f, 10f, dark)
                box(3f, 2f, 6f, 8f, dark)
                box(-4f, -6f, 3f, -3f, dark)
                box(4f, -4f, 7f, -2f, accent)
            }
            Items.SHOTGUN -> {
                box(-15f, -4f, 15f, -1f, dark)
                box(-15f, -1f, 15f, 1f, dark)
                box(2f, 1f, 9f, 5f, wood)
                box(-16f, -3f, -6f, 5f, wood)
                box(-6f, -3f, 1f, 3f, metal)
                box(13f, -6f, 15f, -4f, accent)
            }
            Items.MEDKIT -> {
                box(-12f, -9f, 12f, 11f, Color.rgb(240, 240, 240), 3f)
                box(-4f, -13f, 4f, -9f, Color.rgb(200, 200, 200))
                box(-2.5f, -6f, 2.5f, 8f, Color.rgb(220, 40, 40))
                box(-7f, -1.5f, 7f, 3.5f, Color.rgb(220, 40, 40))
            }
            Items.BANDAGE -> {
                p.color = Color.rgb(238, 220, 190)
                p.alpha = alpha
                canvas.drawCircle(cx, cy, 11f * s, p)
                p.color = Color.rgb(196, 176, 146)
                p.alpha = alpha
                canvas.drawCircle(cx, cy, 5f * s, p)
                box(4f, 6f, 15f, 11f, Color.rgb(238, 220, 190))
            }
            Items.ANTIDOTE -> {
                box(-7f, -4f, 7f, 13f, Color.rgb(110, 230, 90), 4f)
                box(-3f, -10f, 3f, -4f, Color.rgb(200, 230, 200))
                box(-4f, -13f, 4f, -10f, Color.rgb(120, 80, 50))
                box(-5f, 2f, 5f, 7f, Color.rgb(240, 255, 240))
            }
            Items.ADRENALINE -> {
                canvas.save()
                canvas.rotate(-40f, cx, cy)
                box(-12f, -3.5f, 6f, 3.5f, Color.rgb(70, 220, 255), 2f)
                box(6f, -1f, 15f, 1f, Color.rgb(200, 200, 210))
                box(-15f, -5f, -12f, 5f, Color.rgb(220, 220, 230))
                box(-8f, -3.5f, -5f, 3.5f, Color.rgb(255, 255, 255))
                canvas.restore()
            }
            Items.FIRE_AMMO -> {
                for (k in -1..1) {
                    val x = k * 8f
                    box(x - 3f, -4f, x + 3f, 12f, Color.rgb(196, 150, 60), 1f)
                    path.reset()
                    path.moveTo(cx + (x - 3f) * s, cy - 4f * s)
                    path.lineTo(cx + x * s, cy - 13f * s)
                    path.lineTo(cx + (x + 3f) * s, cy - 4f * s)
                    path.close()
                    p.color = Color.rgb(255, 120, 40)
                    p.alpha = alpha
                    canvas.drawPath(path, p)
                }
            }
            Items.ARMOR -> {
                path.reset()
                path.moveTo(cx - 12f * s, cy - 8f * s)
                path.lineTo(cx - 5f * s, cy - 13f * s)
                path.lineTo(cx - 3f * s, cy - 8f * s)
                path.lineTo(cx + 3f * s, cy - 8f * s)
                path.lineTo(cx + 5f * s, cy - 13f * s)
                path.lineTo(cx + 12f * s, cy - 8f * s)
                path.lineTo(cx + 11f * s, cy + 12f * s)
                path.lineTo(cx - 11f * s, cy + 12f * s)
                path.close()
                p.color = Color.rgb(90, 110, 150)
                p.alpha = alpha
                canvas.drawPath(path, p)
                box(-8f, -1f, 8f, 3f, Color.rgb(170, 186, 220))
                box(-8f, 6f, 8f, 9f, Color.rgb(170, 186, 220))
            }
            Items.GRENADE -> {
                p.color = Color.rgb(120, 140, 70)
                p.alpha = alpha
                canvas.drawCircle(cx, cy + 3f * s, 10f * s, p)
                box(-4f, -11f, 4f, -6f, Color.rgb(150, 150, 150))
                p.style = Paint.Style.STROKE
                p.strokeWidth = 2f * s
                p.color = Color.rgb(200, 200, 200)
                p.alpha = alpha
                canvas.drawCircle(cx + 7f * s, cy - 10f * s, 3.5f * s, p)
                p.style = Paint.Style.FILL
                box(-8f, 1f, 8f, 3f, Color.rgb(90, 106, 52))
            }
            Items.MOLOTOV -> {
                box(-7f, -2f, 7f, 14f, Color.rgb(90, 130, 70), 4f)
                box(-3f, -9f, 3f, -2f, Color.rgb(110, 150, 90))
                box(-2f, -12f, 2f, -9f, Color.rgb(230, 220, 200))
                p.color = Color.rgb(255, 150, 40)
                p.alpha = alpha
                canvas.drawCircle(cx, cy - 15f * s, 3.5f * s, p)
                p.color = Color.rgb(255, 230, 120)
                p.alpha = alpha
                canvas.drawCircle(cx, cy - 14f * s, 1.8f * s, p)
            }
            Items.SMOKE -> {
                box(-7f, -8f, 7f, 10f, Color.rgb(150, 160, 166), 3f)
                box(-5f, -11f, 5f, -8f, Color.rgb(96, 106, 112))
                box(-7f, -2f, 7f, 1f, Color.rgb(210, 218, 222))
                p.style = Paint.Style.STROKE
                p.strokeWidth = 2f * s
                p.color = Color.rgb(220, 228, 232)
                p.alpha = alpha
                canvas.drawCircle(cx + 7f * s, cy - 10f * s, 3.5f * s, p)
                p.style = Paint.Style.FILL
                p.color = Color.rgb(186, 196, 202)
                p.alpha = alpha
                canvas.drawCircle(cx - 10f * s, cy - 11f * s, 3.0f * s, p)
                canvas.drawCircle(cx + 11f * s, cy - 16f * s, 4.0f * s, p)
            }
            else -> Unit
        }
        p.alpha = 255
    }
}

/** View simples com o ícone de um item (para listas do arsenal). */
class ItemIconView(context: Context, var itemId: Int) : View(context) {
    init { ItemIcons.initialize(context) }

    override fun onDraw(canvas: Canvas) {
        ItemIcons.draw(canvas, itemId, width / 2f, height / 2f, minOf(width, height) * 0.9f)
    }
}
