package com.fabiosilva.ecosdeeldra.data

import android.graphics.Bitmap

/**
 * Aparência de um personagem estilo Minecraft, codificada em 32 bits para o motor C++
 * (mesmo layout de `Look` em app/src/main/cpp/render/blocky.h).
 */
data class Look(
    val skin: Int = 1,
    val hairStyle: Int = 1,
    val hairColor: Int = 1,
    val eyes: Int = 0,
    val face: Int = 0,
    val top: Int = 0,
    val topColor: Int = 6,
    val pantsColor: Int = 0,
    val shoes: Int = 0,
    val hat: Int = 0,
    val shorts: Boolean = false,
) {
    fun encode(): Int =
        (skin and 7) or ((hairStyle and 7) shl 3) or ((hairColor and 7) shl 6) or ((eyes and 3) shl 9) or
            ((face and 3) shl 11) or ((top and 7) shl 13) or ((topColor and 15) shl 16) or ((pantsColor and 7) shl 20) or
            ((shoes and 3) shl 23) or ((hat and 7) shl 25) or (if (shorts) 1 shl 28 else 0)

    companion object {
        fun decode(code: Int): Look = Look(
            skin = code and 7,
            hairStyle = (code shr 3) and 7,
            hairColor = (code shr 6) and 7,
            eyes = (code shr 9) and 3,
            face = (code shr 11) and 3,
            top = (code shr 13) and 7,
            topColor = (code shr 16) and 15,
            pantsColor = (code shr 20) and 7,
            shoes = (code shr 23) and 3,
            hat = (code shr 25) and 7,
            shorts = ((code shr 28) and 1) != 0,
        )

    }
}

/** Paleta da miniatura do personagem salvo. */
private object AvatarPalette {
    private fun rgb(r: Int, g: Int, b: Int) = (0xFF shl 24) or (r shl 16) or (g shl 8) or b

    val skinColors = intArrayOf(rgb(255, 224, 196), rgb(241, 194, 160), rgb(224, 172, 128), rgb(198, 140, 98), rgb(161, 108, 72), rgb(120, 78, 52), rgb(84, 54, 36), rgb(120, 170, 96))
    val hairColors = intArrayOf(rgb(34, 28, 26), rgb(92, 58, 34), rgb(222, 186, 96), rgb(178, 74, 38), rgb(180, 180, 184), rgb(58, 104, 214), rgb(232, 104, 170), rgb(80, 180, 80))
    val eyeColors = intArrayOf(rgb(92, 58, 34), rgb(64, 118, 224), rgb(70, 160, 80), rgb(120, 124, 132))
    val topColors = intArrayOf(
        rgb(198, 52, 46), rgb(232, 124, 40), rgb(236, 196, 56), rgb(84, 170, 62), rgb(46, 98, 52), rgb(48, 176, 196),
        rgb(56, 108, 206), rgb(32, 46, 98), rgb(124, 64, 176), rgb(226, 110, 170), rgb(232, 232, 232), rgb(120, 124, 130),
        rgb(36, 38, 44), rgb(110, 72, 44), rgb(214, 192, 150), rgb(232, 186, 48),
    )

}

/** Rosto 8x8 em pixel art (ícones do mapa e da mochila), igual à frente da cabeça no jogo. */
object LookPainter {
    private fun shade(c: Int, f: Float): Int {
        val r = (((c shr 16) and 0xFF) * f).toInt().coerceIn(0, 255)
        val g = (((c shr 8) and 0xFF) * f).toInt().coerceIn(0, 255)
        val b = ((c and 0xFF) * f).toInt().coerceIn(0, 255)
        return (0xFF shl 24) or (r shl 16) or (g shl 8) or b
    }

    fun face(look: Look): Bitmap {
        val px = IntArray(64)
        fun set(x: Int, y: Int, c: Int) {
            if (x in 0..7 && y in 0..7) px[y * 8 + x] = c
        }
        val skin = AvatarPalette.skinColors[look.skin and 7]
        val hair = AvatarPalette.hairColors[look.hairColor and 7]
        for (i in 0 until 64) px[i] = shade(skin, 0.94f + 0.08f * (((i * 7919) % 13) / 13f))
        val frontRows = when (look.hairStyle) { 1, 2, 4, 7 -> 1; 5, 6 -> 2; else -> 0 }
        for (y in 0 until frontRows) for (x in 0..7) set(x, y, hair)
        if (look.hairStyle == 6) { for (x in 0..5) set(x, 1, hair); set(0, 2, hair); set(1, 2, hair) }
        if (look.hairStyle == 3) { set(3, 0, hair); set(4, 0, hair) }
        val brow = if (look.hairStyle == 0 || look.hairStyle == 3) shade(skin, 0.7f) else shade(hair, 0.85f)
        for (x in intArrayOf(1, 2, 5, 6)) set(x, 3, brow)
        val iris = AvatarPalette.eyeColors[look.eyes and 3]
        set(1, 4, 0xFFF4F4F4.toInt()); set(2, 4, iris); set(5, 4, iris); set(6, 4, 0xFFF4F4F4.toInt())
        set(3, 5, shade(skin, 0.86f)); set(4, 5, shade(skin, 0.86f))
        set(3, 6, shade(skin, 0.58f)); set(4, 6, shade(skin, 0.58f))
        when (look.face) {
            1 -> for (y in 5..7) for (x in 0..7) if (!(y == 6 && (x == 3 || x == 4))) set(x, y, shade(hair, 0.9f))
            2 -> for (x in 2..5) set(x, 5, shade(hair, 0.85f))
            3 -> { set(6, 3, 0xFFAA4646.toInt()); set(7, 5, 0xFFAA4646.toInt()); set(6, 6, 0xFFAA4646.toInt()); set(5, 7, 0xFFAA4646.toInt()) }
        }
        val hatColor = AvatarPalette.topColors[look.topColor and 15]
        when (look.hat) {
            1 -> for (x in 0..7) { set(x, 0, hatColor); set(x, 1, shade(hatColor, 0.75f)) }
            2 -> for (y in 0..2) for (x in 0..7) set(x, y, if (y == 2) 0xFF465030.toInt() else 0xFF5C6840.toInt())
            3 -> for (y in 0..2) for (x in 0..7) set(x, y, if (y == 2) 0xFFECECEC.toInt() else hatColor)
            4 -> for (x in 0..7) set(x, 2, 0xFFC82828.toInt())
            5 -> { for (x in 0..7) set(x, 4, 0xFF1E1E22.toInt()); set(1, 4, 0xFF46C8DC.toInt()); set(2, 4, 0xFF46C8DC.toInt()); set(5, 4, 0xFF46C8DC.toInt()); set(6, 4, 0xFF46C8DC.toInt()) }
            6 -> { set(0, 3, hatColor); set(0, 4, hatColor); set(7, 3, hatColor); set(7, 4, hatColor) }
            7 -> for (x in 0..7) set(x, 0, if (x % 3 == 0) 0xFFDC2846.toInt() else 0xFFECBE34.toInt())
        }
        return Bitmap.createBitmap(px, 8, 8, Bitmap.Config.ARGB_8888)
    }

}
