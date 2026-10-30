package com.fabiosilva.ecosdeeldra.ui

import org.junit.Assert.*
import org.junit.Test

class HudGestureTest {
    @Test fun longDragMovesTheDynamicStickBaseAndClampsMovement() {
        val s = HudGestures.stick(100f, 200f, 300f, 200f, 50f)
        assertEquals(220f, s.baseX, .001f)
        assertEquals(200f, s.baseY, .001f)
        assertEquals(270f, s.knobX, .001f)
        assertEquals(1f, s.moveX, .001f)
        assertEquals(0f, s.moveY, .001f)
    }

    @Test fun stickDeadzoneStopsDriftAndScreenUpMovesForward() {
        val idle = HudGestures.stick(100f, 200f, 104f, 200f, 50f)
        assertEquals(0f, idle.moveX, 0f)
        val forward = HudGestures.stick(100f, 200f, 100f, 150f, 50f)
        assertEquals(1f, forward.moveY, .001f)
    }

    @Test fun dynamicStickFollowsItsConfiguredSideAndDoesNotClaimAnotherFinger() {
        assertTrue(HudGestures.joystickRegion(800f, 300f, 900f, 410f, 800f, false))
        assertFalse(HudGestures.joystickRegion(100f, 300f, 900f, 410f, 800f, false))
        assertTrue(HudGestures.joystickRegion(100f, 150f, 900f, 410f, 100f, false))
        assertFalse(HudGestures.joystickRegion(100f, 150f, 900f, 410f, 100f, true))
    }

    @Test fun grenadeReleaseHonorsCancellationAndTheOtherFireFingerStaysHeld() {
        assertFalse(HudGestures.throwCancelled(false, false))
        assertTrue(HudGestures.throwCancelled(true, false))
        assertTrue(HudGestures.throwCancelled(false, true))
        assertTrue(HudGestures.fireHeld(listOf(HudControl.FIRE_LEFT)))
        assertFalse(HudGestures.fireHeld(listOf(HudControl.JOYSTICK)))
    }

    @Test fun battleItemIdsKeepShotgunReticleAndMedicalSuppliesSeparateFromArmor() {
        assertEquals(com.fabiosilva.ecosdeeldra.data.Items.SHOTGUN, BattleItems.iconId(4))
        assertEquals(com.fabiosilva.ecosdeeldra.data.Items.MEDKIT, BattleItems.iconId(23))
        assertEquals(com.fabiosilva.ecosdeeldra.data.Items.ARMOR, BattleItems.iconId(20))
        assertFalse(BattleItems.isGun(6))
        assertTrue(BattleItems.isGun(3))
    }
}
