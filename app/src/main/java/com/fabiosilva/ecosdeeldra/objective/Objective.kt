package com.fabiosilva.ecosdeeldra.objective

import com.fabiosilva.ecosdeeldra.engine.Hud
import kotlin.math.max

enum class ObjectivePhase { NESTS, CRYSTALS, PORTAL, DONE }
enum class NextAction { DESTROY_NEST, PICK_CRYSTAL, ENTER_PORTAL, NONE }
enum class PillState { TODO, CURRENT, DONE }
data class Pill(val label: String, val value: String, val state: PillState, val attention: Boolean = false)

/** Estado do objetivo da campanha. `active = false` em sobrevivência e fora da ilha. */
data class Objective(
    val active: Boolean,
    val nestsDestroyed: Int,
    val nestTotal: Int,
    val crystals: Int,
    val portalOpen: Boolean,
    val victory: Boolean,
) {
    /** Um cristal por ninho. */
    val crystalTotal: Int get() = nestTotal

    /** Cristais caídos e ainda no chão. */
    val pendingCrystals: Int get() = max(0, nestsDestroyed - crystals)

    val phase: ObjectivePhase
        get() = when {
            victory -> ObjectivePhase.DONE
            portalOpen -> ObjectivePhase.PORTAL
            nestTotal > 0 && nestsDestroyed >= nestTotal -> ObjectivePhase.CRYSTALS
            else -> ObjectivePhase.NESTS
        }

    /** O que fazer agora (prioridade: portal > cristal no chão > próximo ninho). */
    val next: NextAction
        get() = when {
            !active || victory -> NextAction.NONE
            portalOpen -> NextAction.ENTER_PORTAL
            pendingCrystals > 0 -> NextAction.PICK_CRYSTAL
            nestsDestroyed < nestTotal -> NextAction.DESTROY_NEST
            else -> NextAction.NONE
        }

    val pills: List<Pill>
        get() = listOf(
            Pill(
                ObjectiveText.PILL_NESTS, "$nestsDestroyed/$nestTotal",
                when {
                    nestsDestroyed >= nestTotal && nestTotal > 0 -> PillState.DONE
                    phase == ObjectivePhase.NESTS -> PillState.CURRENT
                    else -> PillState.TODO
                },
            ),
            Pill(
                ObjectiveText.PILL_CRYSTALS, "$crystals/$crystalTotal",
                when {
                    crystals >= crystalTotal && crystalTotal > 0 -> PillState.DONE
                    phase == ObjectivePhase.CRYSTALS -> PillState.CURRENT
                    else -> PillState.TODO
                },
                attention = pendingCrystals > 0,
            ),
            Pill(
                ObjectiveText.PILL_PORTAL, if (portalOpen) "ABERTO" else "",
                when {
                    victory -> PillState.DONE
                    portalOpen -> PillState.CURRENT
                    else -> PillState.TODO
                },
            ),
        )

    companion object {
        val INACTIVE = Objective(false, 0, 0, 0, portalOpen = false, victory = false)

        /** Lê o array do HUD (o mesmo `FloatArray(Hud.SIZE)` que o HudView usa). */
        fun from(hud: FloatArray): Objective {
            val campaign = hud[Hud.ISLAND] > 0.5f && hud[Hud.SURV] < 0.5f
            val total = hud[Hud.NEST_TOTAL].toInt()
            if (!campaign || total <= 0) return INACTIVE
            return Objective(
                active = true,
                nestsDestroyed = hud[Hud.NESTS_DONE].toInt(),
                nestTotal = total,
                crystals = hud[Hud.CRYSTALS].toInt(),
                portalOpen = hud[Hud.PORTAL_OPEN] > 0.5f,
                victory = hud[Hud.STATE].toInt() == 2,
            )
        }
    }
}
