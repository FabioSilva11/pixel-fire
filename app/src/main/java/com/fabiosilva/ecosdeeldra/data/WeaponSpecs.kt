package com.fabiosilva.ecosdeeldra.data

/** Etiqueta colorida do painel de atributos (ex.: DANO em vermelho, LONGO ALCANCE em azul). */
data class WeaponTag(val label: String, val color: Int)

/**
 * ReticleType do template Multiplayer FPS (Unity): o desenho da mira de cada arma. A ordem espelha
 * ReticleType em app/src/main/cpp/game/items.h.
 */
enum class Reticle { CROSS, TCROSS, OPEN_CIRCULAR, CIRCULAR_CROSS }

/**
 * Atributos exibidos na mochila (estilo Free Fire). Os números vêm de kWeapons em
 * app/src/main/cpp/game/game_items.cpp: dano por acerto, disparos por minuto (60 / intervalo), alcance em metros,
 * precisão (as armas do template não têm dispersão: 100) e velocidade de movimento (o template não muda: 100).
 */
data class WeaponSpec(
    val category: String,
    val damage: Int,
    val fireRate: Int,
    val range: Int,
    val accuracy: Int,
    val moveSpeed: Int,
    val tags: List<WeaponTag>,
    /** Traços que aparecem como "acessórios" no card da arma. */
    val traits: List<String> = emptyList(),
    val reticle: Reticle = Reticle.CROSS,
)

object WeaponSpecs {
    const val KNIFE = -1

    // Cores das etiquetas.
    private const val RED = 0xFFD9545E.toInt()
    private const val ORANGE = 0xFFE8873A.toInt()
    private const val YELLOW = 0xFFD9A62B.toInt()
    private const val PURPLE = 0xFF8E63D9.toInt()

    private val DANO = WeaponTag("DANO", RED)
    private val CADENCIA = WeaponTag("CADÊNCIA", YELLOW)
    private val PRECISAO = WeaponTag("PRECISÃO", PURPLE)
    private val PROJETIL = WeaponTag("PROJÉTIL", ORANGE)
    private val CORPO = WeaponTag("CORPO A CORPO", ORANGE)

    private val specs = mapOf(
        // Fuzil de assalto (AssaultRifle.asset): hitscan, mira em cruz.
        Items.RIFLE to WeaponSpec("FUZIL", 18, 600, 100, 100, 100, listOf(CADENCIA, PRECISAO), listOf("HITSCAN"), Reticle.CROSS),
        // Escopeta (Shotgun.asset): bala que voa a 30 m/s, mira de círculo aberto.
        Items.SHOTGUN to WeaponSpec("ESCOPETA", 36, 60, 90, 100, 100, listOf(DANO, PROJETIL), listOf("PROJÉTIL 30 M/S"), Reticle.OPEN_CIRCULAR),
        // Faca: ataque corpo a corpo (botão ∞), 55 de dano a ~2,4 m, uma vez a cada 0,55 s.
        KNIFE to WeaponSpec("CORPO A CORPO", 55, 109, 2, 100, 100, listOf(CORPO)),
    )

    /** Máximos usados para encher as barras. */
    const val MAX_DAMAGE = 60
    const val MAX_FIRE_RATE = 600
    const val MAX_RANGE = 100
    const val MAX_ACCURACY = 100
    const val MAX_MOVE = 120

    fun get(id: Int): WeaponSpec? = specs[id]

    /** Mira da arma [id] (o fuzil é o padrão, como weaponDef() no C++). */
    fun reticle(id: Int): Reticle = specs[id]?.takeIf { id != KNIFE }?.reticle ?: Reticle.CROSS
}
