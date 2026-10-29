package com.fabiosilva.ecosdeeldra.objective

import com.fabiosilva.ecosdeeldra.engine.Hud
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** O bloco de cristais caídos do HUD (objetivo) espelha o protocol.h e cabe no array. */
class CrystalBlockMirrorTest {
    private fun header(): String {
        val file = listOf("src/main/cpp/game/protocol.h", "app/src/main/cpp/game/protocol.h").map(::File).firstOrNull { it.exists() }
        assertNotNull("protocol.h não encontrado", file)
        return file!!.readText()
    }

    private fun value(text: String, name: String): Int {
        val m = Regex("""\b$name\s*=\s*(\d+)""").find(text)
        assertNotNull("$name sem valor explícito", m)
        return m!!.groupValues[1].toInt()
    }

    @Test
    fun crystalBlockMatchesHeader() {
        val h = header()
        assertEquals(value(h, "H_CRYSTAL_COUNT"), Hud.CRYSTAL_COUNT)
        assertEquals(value(h, "H_CRYSTAL_ITEMS"), Hud.CRYSTAL_ITEMS)
        assertEquals(value(h, "H_SIZE"), Hud.SIZE)
        assertEquals(value(h, "kHudCrystalStride"), Hud.CRYSTAL_STRIDE)
        assertEquals(value(h, "kMaxHudCrystals"), Hud.MAX_CRYSTALS)
    }

    @Test
    fun crystalBlockFitsAndDoesNotCollideWithPerf() {
        assertTrue(Hud.CRYSTAL_COUNT > Hud.PERF_FLAGS)
        assertTrue(Hud.CRYSTAL_ITEMS + Hud.MAX_CRYSTALS * Hud.CRYSTAL_STRIDE <= Hud.SIZE)
    }
}
