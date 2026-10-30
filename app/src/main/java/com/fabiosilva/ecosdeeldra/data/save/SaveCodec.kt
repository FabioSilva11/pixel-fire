package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.Progress
import org.json.JSONObject

class SaveFormatException(message: String) : Exception(message)

sealed interface DecodeOutcome {
    data class Ok(val progress: Progress, val schemaFound: Int, val warnings: List<String>) : DecodeOutcome {
        val isNewerThanApp: Boolean get() = schemaFound > SaveCodec.SCHEMA_VERSION
    }
    data class Invalid(val reason: String) : DecodeOutcome
}

/** Lista explícita de campos: conteúdo desconhecido de formatos anteriores não volta a ser gravado. */
object SaveCodec {
    const val SCHEMA_VERSION = 6

    private fun Float.safe(default: Float) = if (isFinite()) this else default

    fun encode(p: Progress, writtenBy: String = ""): String = JSONObject().apply {
        put("version", SCHEMA_VERSION)
        if (writtenBy.isNotEmpty()) put("writtenBy", writtenBy)
        put("inventory", p.inventory.encode())
        put("settings", JSONObject().apply {
            val s = p.settings
            put("sensitivity", s.sensitivity.safe(1f).toDouble())
            put("musicVolume", s.musicVolume.safe(0.6f).toDouble())
            put("sfxVolume", s.sfxVolume.safe(1f).toDouble())
            put("aimAssist", s.aimAssist)
            put("invertY", s.invertY)
            put("vibration", s.vibration)
            put("quality", s.quality)
            put("showFps", s.showFps)
            put("leftHanded", s.leftHanded)
            put("playerName", s.playerName)
            put("hudLayout", s.hudLayout)
            put("hudOpacity", s.hudOpacity.safe(0.85f).toDouble())
            put("previewWeapon", s.previewWeapon.coerceIn(1,6))
        })
    }.toString()

    private fun JSONObject.section(key: String): JSONObject? {
        if (isNull(key)) return null
        return get(key) as? JSONObject ?: throw SaveFormatException("'$key' deveria ser objeto")
    }

    private fun JSONObject.number(key: String, default: Float, min: Float, max: Float): Float =
        optDouble(key, default.toDouble()).toFloat().safe(default).coerceIn(min, max)

    fun decode(raw: String): DecodeOutcome = try {
        val o = JSONObject(raw.removePrefix("\uFEFF").trim())
        val found = SaveMigrations.detectSchema(o)
        if (found < 0) DecodeOutcome.Invalid("formato não reconhecido") else {
            val p = Progress()
            if (!o.isNull("inventory")) {
                val inventory = o.get("inventory") as? String ?: throw SaveFormatException("'inventory' deveria ser texto")
                p.inventory.copyFrom(Inventory.decode(inventory))
            }
            o.section("settings")?.let {
                val s = p.settings
                s.sensitivity = it.number("sensitivity", 1f, 0.3f, 2.5f)
                s.musicVolume = it.number("musicVolume", 0.6f, 0f, 1f)
                s.sfxVolume = it.number("sfxVolume", 1f, 0f, 1f)
                s.aimAssist = it.optBoolean("aimAssist", true)
                s.invertY = it.optBoolean("invertY", false)
                s.vibration = it.optBoolean("vibration", true)
                s.quality = it.optInt("quality", 1).coerceIn(0, 2)
                s.showFps = it.optBoolean("showFps", false)
                s.leftHanded = it.optBoolean("leftHanded", false)
                s.playerName = it.optString("playerName", "")
                s.hudLayout = it.optString("hudLayout", "")
                s.hudOpacity = it.number("hudOpacity", 0.85f, 0.3f, 1f)
                s.previewWeapon = it.optInt("previewWeapon",3).coerceIn(1,6)
            }
            DecodeOutcome.Ok(p, found, emptyList())
        }
    } catch (e: Exception) {
        DecodeOutcome.Invalid(e.message.orEmpty().take(100))
    }

    fun summarize(p: Progress) = SaveSummary(p.inventory.slots.count { !it.empty })
}
