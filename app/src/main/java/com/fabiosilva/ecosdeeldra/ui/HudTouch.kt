package com.fabiosilva.ecosdeeldra.ui

import kotlin.math.hypot

/**
 * Áreas de toque dos controles do HUD, em geometria pura (sem Android) para que o HudView e os
 * testes de layout usem exatamente as mesmas formas.
 */
sealed class HudTouchArea {
    abstract fun contains(x: Float, y: Float): Boolean

    class Circle(val cx: Float, val cy: Float, val r: Float) : HudTouchArea() {
        override fun contains(x: Float, y: Float) = hypot(x - cx, y - cy) <= r
    }

    class Box(val left: Float, val top: Float, val right: Float, val bottom: Float) : HudTouchArea() {
        override fun contains(x: Float, y: Float) = x > left && x < right && y > top && y < bottom
    }

    fun overlaps(other: HudTouchArea): Boolean = when {
        this is Circle && other is Circle -> hypot(cx - other.cx, cy - other.cy) < r + other.r
        this is Box && other is Box -> left < other.right && other.left < right && top < other.bottom && other.top < bottom
        this is Circle && other is Box -> other.overlapsCircle(this)
        this is Box && other is Circle -> overlapsCircle(other)
        else -> false
    }

    private fun Box.overlapsCircle(c: Circle): Boolean {
        val nx = c.cx.coerceIn(left, right)
        val ny = c.cy.coerceIn(top, bottom)
        return hypot(c.cx - nx, c.cy - ny) < c.r
    }
}

object HudTouch {
    /** Folga de toque em volta dos botões redondos (dp): raio * 1,2 + 4. */
    const val BUTTON_PAD_DP = 4f
    const val BUTTON_GROW = 1.2f

    /**
     * Área de toque do controle [c] centrado em ([x], [y]) com raio visual [r] (px) e densidade [density].
     * O joystick devolve o círculo do anel de repouso (a área real dele é dinâmica, onde o polegar tocar).
     * Botões vizinhos podem ter folgas que se cruzam (como na referência): o HudView escolhe o centro mais próximo.
     */
    fun area(c: HudControl, x: Float, y: Float, r: Float, density: Float): HudTouchArea = when (c) {
        HudControl.JOYSTICK -> HudTouchArea.Circle(x, y, r * HudLayout.JOYSTICK_IDLE)
        HudControl.MAP -> HudTouchArea.Box(x - r, y - r, x + r, y + r)
        HudControl.HOTBAR -> HudTouchArea.Box(x - r * 6f, y - r * 1.2f, x + r * 6f, y + r * 1.2f)
        else -> HudTouchArea.Circle(x, y, r * BUTTON_GROW + BUTTON_PAD_DP * density)
    }

    /** Forma desenhada do controle (sem folga de toque): usada para garantir que nada se sobrepõe na tela. */
    fun visual(c: HudControl, x: Float, y: Float, r: Float): HudTouchArea {
        val (hx, hy) = HudLayout.halfExtent(c, r)
        return when (c) {
            HudControl.MAP, HudControl.HOTBAR, HudControl.WEAPON -> HudTouchArea.Box(x - hx, y - hy, x + hx, y + hy)
            else -> HudTouchArea.Circle(x, y, hx)
        }
    }
}
