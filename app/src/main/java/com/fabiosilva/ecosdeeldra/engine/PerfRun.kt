package com.fabiosilva.ecosdeeldra.engine

/** Parâmetros de diagnóstico de desempenho, lidos de extras de Intent (somente build debug). */
data class PerfRun(
    val mode: Int,            // 1 autopiloto sem inimigos, 2 autopiloto com inimigos, 3 bancada headless
    val stage: Int = 2,
    val seed: Int = 1_234_567,        // < 2^24: cabe exato no FloatArray da configuração
    val frames: Int = 7200,
    val quality: Int = -1,            // -1 mantém o ajuste salvo
    val zombies: Int = -1,
    val meters: Int = 900,
    val policy: Int = 2,
    val flags: Int = 1,
) {
    fun applyTo(config: FloatArray) {
        config[Cfg.SEED] = seed.toFloat()
        config[Cfg.PERF] = if (mode == MODE_BENCH) 0f else mode.toFloat()
        config[Cfg.PERF_FRAMES] = frames.toFloat()
        if (quality >= 0) config[Cfg.QUALITY] = quality.toFloat()
        if (zombies >= 0) config[Cfg.ZOMBIES] = zombies.toFloat()
    }

    companion object {
        const val MODE_AUTO_CLEAN = 1
        const val MODE_AUTO_ENEMIES = 2
        const val MODE_BENCH = 3

        /** Lê extras `zz_perf`, `zz_stage`, ...; null se `zz_perf` ausente/zero ou inválido. */
        fun parse(getInt: (String, Int) -> Int): PerfRun? {
            val mode = getInt("zz_perf", 0)
            if (mode !in 1..3) return null
            return PerfRun(
                mode = mode,
                stage = getInt("zz_stage", 2).coerceIn(1, 60),
                seed = getInt("zz_seed", 1_234_567).coerceIn(1, 16_000_000),
                frames = getInt("zz_frames", 7200).coerceIn(600, 36_000),
                quality = getInt("zz_quality", -1).coerceIn(-1, 2),
                zombies = getInt("zz_zombies", -1).coerceIn(-1, 60),
                meters = getInt("zz_meters", 900).coerceIn(100, 5000),
                policy = getInt("zz_policy", 2).coerceIn(0, 2),
                flags = getInt("zz_flags", 1),
            )
        }
    }
}
