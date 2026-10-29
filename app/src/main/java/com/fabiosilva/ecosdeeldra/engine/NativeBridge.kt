package com.fabiosilva.ecosdeeldra.engine

import android.content.res.AssetManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.util.Log

/** Funções do motor C++ (libzombiezone.so). Métodos de jogo devem rodar na thread GL. */
object NativeBridge {
    private var assets: AssetManager? = null

    init {
        System.loadLibrary("zombiezone")
    }

    fun init(assetManager: AssetManager) {
        assets = assetManager
        nativeInit(assetManager)
    }

    private external fun nativeInit(assetManager: AssetManager)
    external fun surfaceCreated()
    external fun surfaceChanged(width: Int, height: Int)
    external fun drawFrame(nanos: Long)

    external fun enterLobby(look: Int, screen: Int)
    /** [look] = aparência do personagem (Look codificado). */
    external fun startMission(config: FloatArray, look: Int)
    /** [looks] = aparência de cada jogador por id. */
    external fun setPaused(paused: Boolean)
    external fun revive()
    external fun setSettings(sensitivity: Float, aimAssist: Boolean, invertY: Boolean)
    external fun setGraphics(quality: Int)

    // Entrada (seguras em qualquer thread)
    external fun setMove(x: Float, y: Float)
    external fun addLook(dx: Float, dy: Float)
    external fun setFire(held: Boolean)
    external fun setAim(on: Boolean)
    external fun setAimStage(stage: Int)
    external fun pressJump()
    external fun pressReload()
    external fun requestSupply()
    external fun toggleCrouch()
    external fun toggleSprint()
    external fun pressMelee()
    external fun pressGrenade()
    external fun swapShoulder()
    external fun tapSlot(slot: Int)
    external fun useSlot(slot: Int)
    /** Item descartado na mochila durante a partida: cai no chão à frente do jogador (thread GL). */
    external fun dropItem(id: Int, count: Int)
    external fun setThrowAim(held: Boolean, cancel: Boolean)

    /** Inventário (thread GL): ver [INV_INT_COUNT]. */
    external fun setInventory(values: IntArray)

    // Saídas
    external fun copyHud(out: FloatArray)
    external fun drainEvents(out: IntArray): Int
    external fun copyResult(out: FloatArray)
    external fun minimapSize(): Int
    external fun copyMinimap(out: IntArray)
    external fun bigMapSize(): Int
    external fun copyBigMap(out: IntArray)

    /** Diagnóstico (builds debug): bancada headless de streaming, loga na tag ZZPerf. Rode em engine { }. */
    external fun perfBench(seed: Int, stage: Int, radius: Int, meters: Int, policy: Int, flags: Int): Int


    /** Diagnóstico (builds debug): autoteste da reserva de munição dos contratos. 0 = ok; senão linha*1000+falhas. */
    external fun contractSelfTest(): Int

    /**
     * Chamado pelo C++ para decodificar PNGs dos assets com BitmapFactory.
     * Retorna [largura, altura, pixels ARGB...].
     */
    @JvmStatic
    fun decodeAssetImage(path: String, maxSize: Int): IntArray? = runCatching {
        val manager = assets ?: return null
        val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
        manager.open(path).use { BitmapFactory.decodeStream(it, null, bounds) }
        var sample = 1
        while (maxOf(bounds.outWidth, bounds.outHeight) / sample > maxSize) sample *= 2
        val options = BitmapFactory.Options().apply {
            inSampleSize = sample
            inPreferredConfig = Bitmap.Config.ARGB_8888
            inScaled = false
        }
        val bitmap = manager.open(path).use { BitmapFactory.decodeStream(it, null, options) } ?: return null
        val w = bitmap.width
        val h = bitmap.height
        val out = IntArray(2 + w * h)
        out[0] = w
        out[1] = h
        bitmap.getPixels(out, 2, w, 0, 0, w, h)
        bitmap.recycle()
        out
    }.onFailure { Log.e("NativeBridge", "Falha ao decodificar $path", it) }.getOrNull()
}
