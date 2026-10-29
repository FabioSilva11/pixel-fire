package com.fabiosilva.ecosdeeldra.save

import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.Items
import com.fabiosilva.ecosdeeldra.data.save.DecodeOutcome
import com.fabiosilva.ecosdeeldra.data.save.SaveCodec
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class SaveCodecTest {
    private fun read(raw: String) = (SaveCodec.decode(raw) as DecodeOutcome.Ok).progress

    @Test fun retainedSettingsAndEquipmentRoundTrip() {
        val p = Progress().apply {
            settings.sensitivity = 1.4f; settings.musicVolume = 0.2f
            settings.leftHanded = true; settings.quality = 2; settings.hudLayout = "custom"
            inventory.add(Items.SMOKE, 2)
        }
        val back = read(SaveCodec.encode(p))
        assertEquals(p.inventory.encode(), back.inventory.encode())
        assertEquals(1.4f, back.settings.sensitivity, 0.001f)
        assertEquals(0.2f, back.settings.musicVolume, 0.001f)
        assertTrue(back.settings.leftHanded)
        assertEquals(2, back.settings.quality)
        assertEquals("custom", back.settings.hudLayout)
    }

    @Test fun migrationWritesOnlyRetainedSections() {
        val old = JSONObject(SaveFixtures.V4_JSON).put("obsoleteSection", JSONObject().put("value", 99))
        val migrated = JSONObject(SaveCodec.encode(read(old.toString()), "test"))
        assertEquals(setOf("version", "writtenBy", "inventory", "settings"), migrated.keys().asSequence().toSet())
        assertEquals("1234", migrated.getJSONObject("settings").getString("playerName"))
        assertEquals(SaveCodec.SCHEMA_VERSION, migrated.getInt("version"))
    }

    @Test fun corruptCriticalSectionsAreRejected() {
        for (key in listOf("inventory", "settings")) {
            val o = JSONObject(SaveCodec.encode(Progress())).put(key, 123)
            assertTrue(key, SaveCodec.decode(o.toString()) is DecodeOutcome.Invalid)
        }
        assertTrue(SaveCodec.decode("{bad") is DecodeOutcome.Invalid)
        assertTrue(SaveCodec.decode("{}") is DecodeOutcome.Invalid)
    }

    @Test fun settingsAreClampedAndMissingValuesUseDefaults() {
        val o = JSONObject().put("version", 5).put("settings", JSONObject().put("sensitivity", 99).put("quality", -1).put("hudOpacity", 0))
        val p = read(o.toString())
        assertEquals(2.5f, p.settings.sensitivity, 0f)
        assertEquals(0, p.settings.quality)
        assertEquals(0.3f, p.settings.hudOpacity, 0f)
        assertTrue(p.inventory.has(Items.RIFLE))
    }

}
