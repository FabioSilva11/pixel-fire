package com.fabiosilva.ecosdeeldra.objective

import com.fabiosilva.ecosdeeldra.engine.Hud
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ObjectiveTest {
    private fun obj(done: Int = 0, crystals: Int = 0, portal: Boolean = false, victory: Boolean = false, total: Int = 3) =
        Objective(true, done, total, crystals, portal, victory)

    @Test
    fun sentenceConstantsMatchTheOfficialText() {
        assertEquals("destrua ninhos → recolha cristais → extraia pelo portal", ObjectiveText.SENTENCE)
        assertEquals(ObjectiveText.SENTENCE.uppercase(), ObjectiveText.SENTENCE_UPPER)
        assertEquals(ObjectiveText.SENTENCE.replaceFirstChar { it.uppercase() }, ObjectiveText.SENTENCE_CAPITALIZED)
        assertTrue(ObjectiveText.LOADING_TIP.contains(ObjectiveText.SENTENCE))
    }

    @Test
    fun phaseFollowsNestsThenCrystalsThenPortal() {
        assertEquals(ObjectivePhase.NESTS, obj(0, 0).phase)
        assertEquals(ObjectivePhase.NESTS, obj(2, 1).phase)
        assertEquals(ObjectivePhase.CRYSTALS, obj(3, 1).phase)
        assertEquals(ObjectivePhase.PORTAL, obj(3, 3, portal = true).phase)
        assertEquals(ObjectivePhase.DONE, obj(3, 3, portal = true, victory = true).phase)
    }

    @Test
    fun pendingCrystalsIsDestroyedMinusCollected() {
        assertEquals(1, obj(2, 1).pendingCrystals)
        assertEquals(0, obj(1, 1).pendingCrystals)
        assertEquals(0, obj(0, 2).pendingCrystals)
    }

    @Test
    fun nextActionPrefersPortalThenCrystalThenNest() {
        assertEquals(NextAction.DESTROY_NEST, obj(0, 0).next)
        assertEquals(NextAction.PICK_CRYSTAL, obj(1, 0).next)
        assertEquals(NextAction.DESTROY_NEST, obj(1, 1).next)
        assertEquals(NextAction.ENTER_PORTAL, obj(3, 3, portal = true).next)
        assertEquals(NextAction.ENTER_PORTAL, obj(3, 2, portal = true).next)
        assertEquals(NextAction.NONE, obj(3, 3, portal = true, victory = true).next)
        assertEquals(NextAction.NONE, Objective.INACTIVE.next)
    }

    @Test
    fun pillsMarkDoneCurrentAndAttention() {
        val start = obj(0, 0).pills
        assertEquals(listOf("NINHOS", "CRISTAIS", "PORTAL"), start.map { it.label })
        assertEquals(listOf(PillState.CURRENT, PillState.TODO, PillState.TODO), start.map { it.state })
        assertEquals("0/3", start[0].value)

        val mid = obj(1, 0).pills
        assertTrue(mid[1].attention)
        assertFalse(start[1].attention)

        val crystals = obj(3, 1).pills
        assertEquals(listOf(PillState.DONE, PillState.CURRENT, PillState.TODO), crystals.map { it.state })

        val portal = obj(3, 3, portal = true).pills
        assertEquals(listOf(PillState.DONE, PillState.DONE, PillState.CURRENT), portal.map { it.state })
        assertEquals("ABERTO", portal[2].value)

        val win = obj(3, 3, portal = true, victory = true).pills
        assertEquals(PillState.DONE, win[2].state)
    }

    @Test
    fun fromHudReadsCampaignIndices() {
        val h = FloatArray(Hud.SIZE)
        h[Hud.ISLAND] = 1f; h[Hud.NEST_TOTAL] = 3f; h[Hud.NESTS_DONE] = 2f; h[Hud.CRYSTALS] = 1f; h[Hud.PORTAL_OPEN] = 0f
        val o = Objective.from(h)
        assertTrue(o.active)
        assertEquals(2, o.nestsDestroyed)
        assertEquals(3, o.nestTotal)
        assertEquals(1, o.crystals)
        assertFalse(o.portalOpen)
        assertFalse(o.victory)
        h[Hud.PORTAL_OPEN] = 1f; h[Hud.STATE] = 2f
        assertTrue(Objective.from(h).portalOpen)
        assertTrue(Objective.from(h).victory)
    }

    @Test
    fun survivalIsInactive() {
        val h = FloatArray(Hud.SIZE)
        h[Hud.ISLAND] = 1f; h[Hud.NEST_TOTAL] = 3f
        h[Hud.SURV] = 1f
        assertEquals(Objective.INACTIVE, Objective.from(h))
        h[Hud.SURV] = 0f; h[Hud.NEST_TOTAL] = 0f
        assertEquals(Objective.INACTIVE, Objective.from(h))
    }
}
