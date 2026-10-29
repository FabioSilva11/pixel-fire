package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.engine.Cfg
import com.fabiosilva.ecosdeeldra.engine.Hud
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.io.File

/** Garante que os índices novos de desempenho do NativeProtocol.kt espelham o protocol.h. */
class NativeProtocolMirrorTest {
    private fun header(): String {
        val candidates = listOf("src/main/cpp/game/protocol.h", "app/src/main/cpp/game/protocol.h")
        val file = candidates.map(::File).firstOrNull { it.exists() }
        assertNotNull("protocol.h não encontrado a partir de ${File(".").absolutePath}", file)
        return file!!.readText()
    }

    private fun headerValue(text: String, name: String): Int {
        val m = Regex("""\b$name\s*=\s*(\d+)""").find(text)
        assertNotNull("$name sem valor explícito no protocol.h", m)
        return m!!.groupValues[1].toInt()
    }

    @Test
    fun perfBlockMatchesHeader() {
        val h = header()
        val pairs = mapOf(
            "H_PERF_FPS" to Hud.PERF_FPS,
            "H_PERF_AVG_MS" to Hud.PERF_AVG_MS,
            "H_PERF_P95_MS" to Hud.PERF_P95_MS,
            "H_PERF_MAX_MS" to Hud.PERF_MAX_MS,
            "H_PERF_WORK_MS" to Hud.PERF_WORK_MS,
            "H_PERF_STREAM_MS" to Hud.PERF_STREAM_MS,
            "H_PERF_PENDING" to Hud.PERF_PENDING,
            "H_PERF_GEN_PS" to Hud.PERF_GEN_PS,
            "H_PERF_UP_PS" to Hud.PERF_UP_PS,
            "H_PERF_FLAGS" to Hud.PERF_FLAGS,
            "H_SIZE" to Hud.SIZE,
            "C_PERF" to Cfg.PERF,
            "C_PERF_FRAMES" to Cfg.PERF_FRAMES,
            "C_COUNT" to Cfg.COUNT,
        )
        for ((name, kotlinValue) in pairs) assertEquals(name, headerValue(h, name), kotlinValue)
    }

    @Test
    fun backpackBlockMatchesHeader() {
        val h = header()
        assertEquals("H_CLIPS", headerValue(h, "H_CLIPS"), Hud.CLIPS)
        assertEquals("H_MAG_MUL", headerValue(h, "H_MAG_MUL"), Hud.MAG_MUL)
        // 8 armas (ids 1..8) cabem antes do bloco da sobrevivência.
        assertTrue(Hud.CLIPS + 8 <= Hud.SURV)
    }

    @Test
    fun perfBlockFitsInsideHudArray() {
        assertTrue(Hud.PERF_FLAGS < Hud.SIZE)
        val perf = Hud::class.java.fields.filter { it.name.startsWith("PERF_") }
        assertEquals(10, perf.size)
        val perfValues = perf.map { it.getInt(null) }.toSet()
        assertEquals(10, perfValues.size)
        // Nenhum outro índice do HUD cai dentro de 650..659.
        val others = Hud::class.java.fields.filter { it.type == Int::class.javaPrimitiveType && !it.name.startsWith("PERF_") && it.name != "SIZE" && !it.name.endsWith("_STRIDE") && !it.name.endsWith("_COUNT") }
        for (f in others) {
            val v = f.getInt(null)
            assertTrue("${f.name}=$v colide com o bloco de desempenho", v !in 650..659)
        }
    }
}
