package com.fabiosilva.ecosdeeldra.save

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

/** Sentinela: se falhar com "not mocked", falta testImplementation("org.json:json:...") no classpath de testes. */
class JsonRuntimeSanityTest {
    @Test
    fun orgJsonIsTheRealImplementationNotTheAndroidStub() {
        val o = JSONObject("""{"a":1,"b":[true,null,"x"]}""")
        assertEquals(1, o.getInt("a"))
        assertEquals(3, o.getJSONArray("b").length())
        assertTrue(o.getJSONArray("b").isNull(1))
    }
}
