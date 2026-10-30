package com.fabiosilva.ecosdeeldra.engine

import org.junit.Assert.*
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder

class BattleWireTest {
    @Test fun projectilePacketsAreBoundedSeparatelyFromInventory() {
        val out=ByteBuffer.allocate(36).order(ByteOrder.LITTLE_ENDIAN)
        out.put(5).put(1).putShort(0)
        floatArrayOf(1f,1f,0f,25f,700f,30f,700f,2.5f).forEach{out.putFloat(it)}
        assertNotNull(BattleWire.effects(out.array()))
        out.putFloat(16,99f);assertNull(BattleWire.effects(out.array()))
    }
    private fun snapshot(): FloatArray = FloatArray(104).apply {
        this[0]=1f;this[1]=2f;this[3]=-1f;this[6]=700f;this[9]=400f
        this[16]=1f;this[21]=1f;this[23]=1f
        this[24]=0f;this[25]=0f;this[27]=5f;this[28]=700f;this[29]=40f;this[30]=700f
        this[33]=100f;this[35]=3f;this[50]=-1f
    }
    @Test fun commandIsBoundedAndLittleEndian() {
        val bytes=BattleWire.input(floatArrayOf(7f,.4f,-.7f,1f,.2f,1f,-1f,-1f,0f,0f,0f,0f))!!
        assertEquals(52,bytes.size);assertEquals(1,bytes[0].toInt())
        assertEquals(7f,ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN).getFloat(4),0f)
        assertNull(BattleWire.input(FloatArray(12).apply { this[0]=Float.NaN }))
        assertNull(BattleWire.input(FloatArray(12).apply { this[0]=1f;this[1]=2f }))
    }
    @Test fun malformedSnapshotsNeverReachRenderer() {
        val state=snapshot();assertNotNull(BattleWire.validate(state))
        assertNull(BattleWire.validate(state.copyOf(103)))
        assertNull(BattleWire.validate(state.copyOf().apply { this[16]=50f }))
        assertNull(BattleWire.validate(state.copyOf().apply { this[28]=Float.POSITIVE_INFINITY }))
        assertNull(BattleWire.validate(state.copyOf().apply { this[35]=4f }))
        assertNull(BattleWire.validate(state.copyOf().apply { this[56]=3f;this[57]=60f }))
    }
    @Test fun snapshotPacketRequiresVersionAndReservedBytes() {
        val state=snapshot();val out=ByteBuffer.allocate(4+state.size*4).order(ByteOrder.LITTLE_ENDIAN)
        out.put(2).put(1).putShort(0);state.forEach { out.putFloat(it) }
        assertNotNull(BattleWire.snapshot(out.array()))
        out.array()[2]=1;assertNull(BattleWire.snapshot(out.array()))
    }
}
