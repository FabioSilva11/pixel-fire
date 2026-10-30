package com.fabiosilva.ecosdeeldra

import com.fabiosilva.ecosdeeldra.engine.Hud
import org.junit.Assert.*
import org.junit.Test
import java.io.File

class NativeProtocolMirrorTest {
    @Test fun presentationIndicesMatchHeaderAndFitBuffers(){
        val file=listOf(File("src/main/cpp/game/protocol.h"),File("app/src/main/cpp/game/protocol.h")).first{it.exists()}
        val text=file.readText()
        Hud::class.java.fields.filter{it.type==Int::class.javaPrimitiveType}.forEach{field->
            val value=Regex("\\bH_${field.name}\\s*=\\s*(\\d+)").find(text)
            assertNotNull(field.name,value);assertEquals(field.name,field.getInt(null),value!!.groupValues[1].toInt())
        }
        assertTrue(Hud.INV+48<=Hud.SIZE);assertTrue(Hud.CLIPS+8<=Hud.INV)
    }
}
