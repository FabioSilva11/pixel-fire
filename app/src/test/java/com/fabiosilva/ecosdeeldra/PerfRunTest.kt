package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.engine.Cfg
import com.fabiosilva.ecosdeeldra.engine.PerfRun
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class PerfRunTest {
    private fun extras(vararg pairs: Pair<String, Int>): (String, Int) -> Int {
        val map = pairs.toMap()
        return { key, def -> map[key] ?: def }
    }

    @Test
    fun parseReturnsNullWhenDisabled() {
        assertNull(PerfRun.parse(extras()))
        assertNull(PerfRun.parse(extras("zz_perf" to 0)))
        assertNull(PerfRun.parse(extras("zz_perf" to 4)))
    }

    @Test
    fun parseClampsAndDefaults() {
        val run = PerfRun.parse(extras("zz_perf" to 1, "zz_seed" to 20_000_000, "zz_stage" to 0, "zz_frames" to 1))
        assertNotNull(run)
        run!!
        assertEquals(16_000_000, run.seed)
        assertEquals(1, run.stage)
        assertEquals(600, run.frames)
        assertEquals(-1, run.quality)
        assertEquals(900, run.meters)
    }

    @Test
    fun applyToWritesSeedAndMode() {
        val config = FloatArray(Cfg.COUNT)
        config[Cfg.QUALITY] = 1f
        PerfRun.parse(extras("zz_perf" to 1, "zz_seed" to 777))!!.applyTo(config)
        assertEquals(1f, config[Cfg.PERF], 0f)
        assertEquals(777f, config[Cfg.SEED], 0f)
        assertEquals(1f, config[Cfg.QUALITY], 0f)  // quality < 0: mantém o ajuste salvo

        PerfRun.parse(extras("zz_perf" to 2, "zz_quality" to 2, "zz_zombies" to 30))!!.applyTo(config)
        assertEquals(2f, config[Cfg.PERF], 0f)
        assertEquals(2f, config[Cfg.QUALITY], 0f)
        assertEquals(30f, config[Cfg.ZOMBIES], 0f)
    }

    @Test
    fun benchModeDoesNotEnableAutopilot() {
        val config = FloatArray(Cfg.COUNT)
        PerfRun.parse(extras("zz_perf" to PerfRun.MODE_BENCH))!!.applyTo(config)
        assertEquals(0f, config[Cfg.PERF], 0f)
    }
}
