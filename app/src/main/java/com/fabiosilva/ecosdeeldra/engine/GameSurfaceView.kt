package com.fabiosilva.ecosdeeldra.engine

import android.annotation.SuppressLint
import android.content.Context
import android.opengl.GLSurfaceView
import android.os.Build
import android.view.Choreographer
import android.view.Surface
import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

/** EGL_OPENGL_ES3_BIT_KHR (não existe constante em EGL10). */
private const val EGL_OPENGL_ES3_BIT = 0x40

/**
 * Superfície OpenGL ES 3 que hospeda o motor C++.
 * Renderiza sob demanda a no máximo ~60 FPS (o aparelho pode ter tela de 120 Hz).
 */
@SuppressLint("ViewConstructor")
class GameSurfaceView(
    context: Context,
    private val onEvents: (IntArray, Int) -> Unit,
    private val onInput: (FloatArray) -> Unit = {},
    private val onFrame: () -> Unit = {},
) : GLSurfaceView(context), Choreographer.FrameCallback {

    private val events = IntArray(4096)
    private var lastRequest = 0L
    private var running = false
    private var renderScale = 0.8f
    private var viewW = 0
    private var viewH = 0

    init {
        setEGLContextClientVersion(3)
        // Antisserrilhado 4x (as bordas dos blocos do herói e das armas ficam lisas, como na página de teste);
        // aparelhos sem MSAA caem na configuração simples de antes.
        setEGLConfigChooser { egl, display ->
            fun choose(samples: Int): EGLConfig? {
                val attribs = mutableListOf(
                    EGL10.EGL_RED_SIZE, 8, EGL10.EGL_GREEN_SIZE, 8, EGL10.EGL_BLUE_SIZE, 8, EGL10.EGL_ALPHA_SIZE, 8,
                    EGL10.EGL_DEPTH_SIZE, 24, EGL10.EGL_STENCIL_SIZE, 0, EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                )
                if (samples > 0) attribs += listOf(EGL10.EGL_SAMPLE_BUFFERS, 1, EGL10.EGL_SAMPLES, samples)
                attribs += EGL10.EGL_NONE
                val configs = arrayOfNulls<EGLConfig>(1)
                val count = IntArray(1)
                return if (egl.eglChooseConfig(display, attribs.toIntArray(), configs, 1, count) && count[0] > 0) configs[0] else null
            }
            choose(4) ?: choose(0) ?: throw IllegalStateException("Nenhuma configuração EGL compatível com OpenGL ES 3")
        }
        preserveEGLContextOnPause = true
        setRenderer(object : Renderer {
            override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
                NativeBridge.surfaceCreated()
            }

            override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
                NativeBridge.surfaceChanged(width, height)
            }

            override fun onDrawFrame(gl: GL10?) {
                onFrame()
                NativeBridge.drawFrame(System.nanoTime())
                repeat(4) {
                    val command=NativeBridge.pollBattleInput()
                    if(command.isNotEmpty())onInput(command)
                }
                val n = NativeBridge.drainEvents(events)
                if (n > 0) onEvents(events, n)
            }
        })
        renderMode = RENDERMODE_WHEN_DIRTY
    }

    /** 0 = baixa (60% da resolução), 1 = média (80%), 2 = alta (100%). */
    fun setQuality(level: Int) {
        renderScale = when (level) { 0 -> 0.6f; 2 -> 1.0f; else -> 0.8f }
        applyFixedSize()
    }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) {
        super.onSizeChanged(w, h, oldw, oldh)
        viewW = w
        viewH = h
        applyFixedSize()
    }

    private fun applyFixedSize() {
        if (viewW <= 0 || viewH <= 0) return
        holder.setFixedSize((viewW * renderScale).toInt().coerceAtLeast(320), (viewH * renderScale).toInt().coerceAtLeast(180))
    }

    fun start() {
        if (running) return
        running = true
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            runCatching { holder.surface.setFrameRate(60f, Surface.FRAME_RATE_COMPATIBILITY_DEFAULT) }
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    fun stop() {
        running = false
        Choreographer.getInstance().removeFrameCallback(this)
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!running) return
        if (frameTimeNanos - lastRequest >= 15_500_000L) {
            lastRequest = frameTimeNanos
            requestRender()
        }
        Choreographer.getInstance().postFrameCallback(this)
    }

    /** Executa um comando do motor na thread GL. */
    fun engine(block: () -> Unit) = queueEvent(block)
}
