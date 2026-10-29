package com.fabiosilva.ecosdeeldra.data

data class Difficulty(
    val zombieCount: Int,
    val zombieHealth: Float,
    val zombieDamage: Float,
    val zombieSpeed: Float,
    val playerHealth: Float,
    val magazine: Int,
    val power: Float,
    val reloadSpeed: Float,
)

/** Tipos de zumbi (mesma ordem de ZombieKind no C++). */
data class ZombieInfo(val kind: Int, val name: String, val ability: String, val color: Int)

object ZombieKinds {
    val all = listOf(
        ZombieInfo(0, "Errante", "Anda em bando e ataca de perto.", 0xFF8FD46B.toInt()),
        ZombieInfo(1, "Corredor", "Muito rápido e dá botes de longe.", 0xFF9DB4FF.toInt()),
        ZombieInfo(2, "Brutamontes", "Gigante: soca o chão e arremessa você.", 0xFF6E9C5E.toInt()),
        ZombieInfo(3, "Arremessador", "Atira pedras e mantém distância.", 0xFFB8B4A6.toInt()),
        ZombieInfo(4, "Incendiário", "Lança coquetéis molotov que deixam fogo no chão.", 0xFFFF9A4D.toInt()),
        ZombieInfo(5, "Retalhador", "Avança com uma faca e causa sangramento.", 0xFFE07A7A.toInt()),
        ZombieInfo(6, "Inchado", "Corre até você e explode — mate de longe!", 0xFFB6FF5C.toInt()),
        ZombieInfo(7, "Gritador", "Grita, chama a horda e a deixa furiosa.", 0xFFD6B8FF.toInt()),
        ZombieInfo(8, "Cuspidor", "Cospe ácido que deixa poças corrosivas.", 0xFFE4FF6B.toInt()),
        ZombieInfo(9, "Atirador de facas", "Lança facas rápidas em linha reta.", 0xFF9CD8FF.toInt()),
    )

    fun get(kind: Int): ZombieInfo = all[kind.coerceIn(0, all.size - 1)]
}

/** Os mesmos valores básicos da partida dos ninhos, sem progressão entre fases. */
object MatchRules {
    const val NEST_COUNT = 3
    fun difficulty() = Difficulty(8, 60f, 8f, 1f, 100f, 30, 1f, 1f)
}
