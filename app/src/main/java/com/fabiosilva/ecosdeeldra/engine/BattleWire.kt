package com.fabiosilva.ecosdeeldra.engine

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.math.floor

/** Dynamic state only. Inventory and health always come from the dedicated server. */
object BattleWire {
    const val VERSION=1
    const val MAX_SEQUENCE=16_777_215
    private fun integer(v: Float, lo: Int, hi: Int) = v.isFinite() && v>=lo && v<=hi && floor(v)==v
    private fun item(id: Int) = id in 1..6 || id in 10..14 || id in 20..28
    fun input(v: FloatArray): ByteArray? {
        if(v.size!=12 || v.any { !it.isFinite() } || !integer(v[0],1,MAX_SEQUENCE) ||
            v[1] !in -1f..1f || v[2] !in -1f..1f || v[4] !in -1.55f..1.55f ||
            !integer(v[5],0,32767) || !integer(v[6],-1,23) || !integer(v[7],-1,49) ||
            !integer(v[8],0,28) || !integer(v[9],0,60)) return null
        return ByteBuffer.allocate(52).order(ByteOrder.LITTLE_ENDIAN).apply {
            put(1);put(1);putShort(0);v.forEach { putFloat(it) }
        }.array()
    }
    fun snapshot(bytes: ByteArray): FloatArray? {
        if(bytes.size !in 292..96004 || bytes.size%4!=0 || bytes[0]!=2.toByte() || bytes[1]!=1.toByte() || bytes[2]!=0.toByte() || bytes[3]!=0.toByte())return null
        val b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);b.position(4)
        return validate(FloatArray((bytes.size-4)/4) { b.float })
    }
    fun effects(bytes: ByteArray): FloatArray? {
        if(bytes.size !in 12..2412||bytes.size%4!=0||bytes[0]!=5.toByte()||bytes[1]!=1.toByte()||bytes[2]!=0.toByte()||bytes[3]!=0.toByte())return null
        val b=ByteBuffer.wrap(bytes).order(ByteOrder.LITTLE_ENDIAN);b.position(4)
        val v=FloatArray((bytes.size-4)/4){b.float}
        if(v.any{!it.isFinite()}||v[0]<0||!integer(v[1],0,100)||v.size!=2+v[1].toInt()*6)return null
        repeat(v[1].toInt()){val p=2+it*6;if(!integer(v[p],0,49)||!integer(v[p+1],25,26)||v[p+2] !in 0f..1440f||v[p+3] !in -10f..300f||v[p+4] !in 0f..1440f||v[p+5] !in 0f..3.1f)return null}
        return v
    }
    fun validate(v: FloatArray): FloatArray? {
        if(v.size !in 72..24000 || v.any { !it.isFinite() } || v[0]!=1f)return null
        val at=intArrayOf(16,17,18,19,20);val max=intArrayOf(50,1800,128,12,100)
        if(at.indices.any { !integer(v[at[it]],0,max[it]) })return null
        val counts=at.map { v[it].toInt() }
        if(v.size!=24+counts[0]*32+counts[1]*7+counts[2]*10+counts[3]*7+counts[4]*5+48)return null
        if(!integer(v[1],0,3)||!integer(v[3],-1,49)||v[2]<0||v[6] !in 0f..1440f||v[9] !in 0f..1440f||!integer(v[23],1,4)||v[23]==3f)return null
        val seen=HashSet<Int>();var p=24
        repeat(counts[0]) {
            if(!integer(v[p],0,49)||!seen.add(v[p].toInt())||!integer(v[p+1],0,49)||!integer(v[p+2],0,65535)||!integer(v[p+3],0,63)||
                v[p+4] !in 0f..1440f||v[p+5] !in -10f..300f||v[p+6] !in 0f..1440f||v[p+8] !in -1.55f..1.55f||v[p+9] !in 0f..100f||
                !integer(v[p+11],0,3)||!integer(v[p+12],0,6)||v[p+13] !in 0f..60f||!integer(v[p+18],0,MAX_SEQUENCE)||!integer(v[p+26],-1,23)||!integer(v[p+30],0,65535))return null
            p+=32
        }
        repeat(counts[1]) { if(!integer(v[p],1,MAX_SEQUENCE)||!integer(v[p+1],1,28)||!item(v[p+1].toInt())||!integer(v[p+2],1,10000))return null;p+=7 }
        repeat(counts[2]) { if(!integer(v[p],1,MAX_SEQUENCE)||!integer(v[p+1],0,49)||v[p+6] !in 0f..250f||v[p+7] !in 0f..45f)return null;p+=10 }
        repeat(counts[3]) { if(!integer(v[p],1,MAX_SEQUENCE)||!integer(v[p+4],0,1))return null;p+=7 }
        p+=counts[4]*5
        repeat(24) {
            val id=v[p].toInt();val limit=if(id in 1..6||id in 20..22)1 else if(id in 10..14)60 else 5
            if(!integer(v[p],0,28)||!integer(v[p+1],0,limit)||(id==0&&v[p+1]!=0f)||(id!=0&&(!item(id)||v[p+1]<1)))return null
            p+=2
        }
        return v
    }
}
