package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.data.Look
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LookTest {
    @Test
    fun encodeDecodeRoundTripsEveryPart() {
        val look = Look(skin = 6, hairStyle = 7, hairColor = 5, eyes = 3, face = 2, top = 6, topColor = 15, pantsColor = 7, shoes = 3, hat = 7, shorts = true)
        assertEquals(look, Look.decode(look.encode()))
        // O bit de zumbi (30) é só do motor: nunca aparece numa aparência de jogador.
        assertEquals(0, Look(hat = 7, shorts = true).encode() and (1 shl 30))
    }

}
