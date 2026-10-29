package com.fabiosilva.ecosdeeldra.ui

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HudLayoutTest {
    private class Screen(val w: Float, val h: Float, val name: String)

    private fun dp(widthPx: Int, heightPx: Int, density: Float) =
        Screen(widthPx / density, heightPx / density, "${widthPx}x$heightPx@$density")

    /** Telas landscape de teste em dp (a última linha são 2712x1220 px em duas densidades comuns). */
    private val screens = listOf(
        Screen(640f, 360f, "640x360"),
        Screen(800f, 360f, "800x360"),
        Screen(892f, 412f, "892x412"),
        dp(2712, 1220, 3f),
        dp(2712, 1220, 2.625f),
    )

    // Controles dos polegares (espelham no modo canhoto) e os que ficam sempre no mesmo lugar.
    private val combat = listOf(
        HudControl.FIRE, HudControl.FIRE_LEFT, HudControl.AIM, HudControl.JUMP, HudControl.RELOAD,
        HudControl.CROUCH, HudControl.SPRINT, HudControl.MELEE, HudControl.GRENADE, HudControl.JOYSTICK, HudControl.HOTBAR,
        HudControl.BAG, HudControl.MEDKIT,
    )
    private val fixed = listOf(HudControl.MAP, HudControl.PAUSE, HudControl.WEAPON, HudControl.EMOTE)

    private fun diameter(l: HudLayout, c: HudControl) = 2f * c.baseDp * l[c].scale

    private fun center(l: HudLayout, s: Screen, c: HudControl) = l[c].x * s.w to l[c].y * s.h

    @Test
    fun defaultSizesMatchReferenceScale() {
        // Diâmetros (dp) medidos na captura de referência (screenshots/xray-hud/xray-report.md).
        val l = HudLayout.defaults(2712f / 3f, 1220f / 3f, false)
        val expected = mapOf(
            HudControl.MAP to 113.2f, HudControl.JOYSTICK to 90f, HudControl.FIRE to 75.6f, HudControl.AIM to 73f,
            HudControl.JUMP to 92.2f, HudControl.CROUCH to 92.2f, HudControl.SPRINT to 90f, HudControl.MELEE to 38.8f,
            HudControl.GRENADE to 53.6f, HudControl.RELOAD to 36.6f, HudControl.BAG to 49.4f, HudControl.MEDKIT to 48.2f,
            HudControl.EMOTE to 36.6f,
        )
        for ((c, d) in expected) assertEquals(c.name, d, diameter(l, c), 0.05f)
        // os seis espaços da barra de itens: 30 dp, o mínimo legível
        assertEquals(30f, diameter(l, HudControl.HOTBAR), 0.01f)
    }

    @Test
    fun defaultLayoutFitsNarrowLandscape() {
        for (s in screens) for (left in listOf(false, true)) {
            val l = HudLayout.defaults(s.w, s.h, left)
            for (c in HudControl.entries) {
                val (x, y) = center(l, s, c)
                val (hx, hy) = HudLayout.halfExtent(c, c.baseDp * l[c].scale)
                val tag = "${s.name} left=$left ${c.name}"
                if (c == HudControl.HOTBAR) assertTrue("$tag slot legível", diameter(l, c) >= 30f - 0.01f)
                assertTrue("$tag esquerda", x - hx >= -0.01f)
                assertTrue("$tag direita", x + hx <= s.w + 0.01f)
                assertTrue("$tag topo", y - hy >= -0.01f)
                assertTrue("$tag margem inferior", y + hy <= s.h - HudLayout.BOTTOM_MARGIN_DP + 0.01f)
            }
        }
    }

    @Test
    fun defaultLayoutSeparatesVisibleControls() {
        // Os desenhos não se sobrepõem; as folgas de toque podem se cruzar (vale o centro mais próximo).
        for (s in screens) for (left in listOf(false, true)) {
            val l = HudLayout.defaults(s.w, s.h, left)
            val shown = HudControl.entries.filter { l[it].visible }
            val areas = shown.map { c ->
                val (x, y) = center(l, s, c)
                c to HudTouch.visual(c, x, y, c.baseDp * l[c].scale)
            }
            for (i in areas.indices) for (j in i + 1 until areas.size) {
                assertFalse("${s.name} left=$left: ${areas[i].first} x ${areas[j].first}", areas[i].second.overlaps(areas[j].second))
            }
        }
    }

    @Test
    fun defaultLayoutMirrorsCombatControlsForLeftHanded() {
        for (s in screens) {
            val right = HudLayout.defaults(s.w, s.h, false)
            val left = HudLayout.defaults(s.w, s.h, true)
            for (c in combat) {
                assertEquals("${s.name} ${c.name} x", 1f - right[c].x, left[c].x, 0.0001f)
                assertEquals("${s.name} ${c.name} y", right[c].y, left[c].y, 0.0001f)
                assertEquals("${s.name} ${c.name} escala", right[c].scale, left[c].scale, 0.0001f)
            }
            for (c in fixed) {
                assertEquals("${s.name} ${c.name} x", right[c].x, left[c].x, 0.0001f)
                assertEquals("${s.name} ${c.name} y", right[c].y, left[c].y, 0.0001f)
            }
        }
    }

    @Test
    fun decodePreservesCustomControlPositions() {
        val fallback = HudLayout.defaults(892f, 412f, false)
        val saved = "${HudLayout.VERSION};fire=0.3000,0.4000,1.250,1;map=0.5000,0.2500,0.800,0;hotbar=0.4000,0.9000,1.000,1"
        val l = HudLayout.decode(saved, fallback)
        assertEquals(0.3f, l[HudControl.FIRE].x, 0.0001f)
        assertEquals(0.4f, l[HudControl.FIRE].y, 0.0001f)
        assertEquals(1.25f, l[HudControl.FIRE].scale, 0.0001f)
        assertEquals(0.5f, l[HudControl.MAP].x, 0.0001f)
        assertEquals(0.8f, l[HudControl.MAP].scale, 0.0001f)
        assertFalse(l[HudControl.MAP].visible)
        // controles que não estavam no layout salvo (versão antiga) vêm do padrão
        assertEquals(fallback[HudControl.JUMP].x, l[HudControl.JUMP].x, 0.0001f)
        assertEquals(fallback[HudControl.BAG].y, l[HudControl.BAG].y, 0.0001f)
        // texto errado não derruba: ignora a entrada inválida e preserva o resto
        val odd = HudLayout.decode("${HudLayout.VERSION};xyz=1,2,3,1;fire=abc,0.4,1,1;aim=0.1,0.2,9.0,1", fallback)
        assertEquals(fallback[HudControl.FIRE].x, odd[HudControl.FIRE].x, 0.0001f)
        assertEquals(HudLayout.MAX_SCALE, odd[HudControl.AIM].scale, 0.0001f)
        // salvar e ler de novo mantém tudo
        val again = HudLayout.decode(l.encode(), fallback)
        for (c in HudControl.entries) {
            assertEquals(c.name, l[c].x, again[c].x, 0.0001f)
            assertEquals(c.name, l[c].visible, again[c].visible)
        }
    }

    @Test
    fun layoutSavedBeforeTheReferenceLayoutIsReplacedOnce() {
        val fallback = HudLayout.defaults(904f, 406.67f, false)
        // salvo sem versão (antes do raio X do HUD): volta tudo ao padrão novo
        val old = HudLayout.decode("fire=0.3000,0.4000,1.250,1;sprint=0.8812,0.3908,0.700,1", fallback)
        for (c in HudControl.entries) {
            assertEquals(c.name, fallback[c].x, old[c].x, 0.0001f)
            assertEquals(c.name, fallback[c].scale, old[c].scale, 0.0001f)
        }
        // o que o jogador salvar depois disso já sai com a versão e é respeitado
        assertTrue(old.encode().startsWith("${HudLayout.VERSION};"))
    }
}
