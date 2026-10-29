package com.fabiosilva.ecosdeeldra.data.save

import org.json.JSONObject

internal object SaveMigrations {

    fun detectSchema(o: JSONObject): Int {
        if (o.has("version")) {
            val v = o.opt("version")
            return if (v is Number && v.toInt() >= 0) v.toInt() else -1
        }
        return if (listOf("inventory", "settings").any { o.has(it) }) 0 else -1
    }
}
