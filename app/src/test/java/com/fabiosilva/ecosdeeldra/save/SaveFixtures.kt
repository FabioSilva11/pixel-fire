package com.fabiosilva.ecosdeeldra.save

import com.fabiosilva.ecosdeeldra.data.Progress
import com.fabiosilva.ecosdeeldra.data.Inventory
import com.fabiosilva.ecosdeeldra.data.save.SaveCodec
import org.json.JSONObject

object SaveFixtures {
    val V4_JSON = JSONObject().put("version", 4)
        .put("inventory", Inventory.starter().encode())
        .put("settings", JSONObject().put("playerName", "1234")).toString()
    fun json(value: Int): String = SaveCodec.encode(Progress().apply { settings.playerName = value.toString() }, "teste")
    fun v4With(edit: (JSONObject) -> Unit): String = JSONObject(V4_JSON).also(edit).toString()
}
