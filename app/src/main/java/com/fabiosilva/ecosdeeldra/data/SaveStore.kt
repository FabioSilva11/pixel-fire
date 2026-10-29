package com.fabiosilva.ecosdeeldra.data

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.fabiosilva.ecosdeeldra.BuildConfig
import com.fabiosilva.ecosdeeldra.data.save.BackupInfo
import com.fabiosilva.ecosdeeldra.data.save.LoadResult
import com.fabiosilva.ecosdeeldra.data.save.PersistResult
import com.fabiosilva.ecosdeeldra.data.save.SaveCodec
import com.fabiosilva.ecosdeeldra.data.save.SaveFiles
import com.fabiosilva.ecosdeeldra.data.save.SaveRepository
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

/**
 * Fachada Android do save: arquivos em filesDir/save (ver data/save/SaveRepository).
 * O JSON é montado na thread chamadora (foto consistente do Progress) e gravado numa thread única,
 * juntando pedidos seguidos. onPause usa [saveNow] (síncrono, com limite de 2 s).
 */
class SaveStore(context: Context) {
    private val app = context.applicationContext
    private val repo = SaveRepository(
        files = SaveFiles(File(app.filesDir, "save")),
        legacy = { app.getSharedPreferences(LEGACY_PREFS, Context.MODE_PRIVATE).getString(LEGACY_KEY, null) },
        writtenBy = BuildConfig.VERSION_NAME,
        log = { Log.w(TAG, it) },
    )
    private val io = Executors.newSingleThreadExecutor { r -> Thread(r, "zz-save").apply { isDaemon = true } }
    private val pending = AtomicReference<String?>(null)
    private val main = Handler(Looper.getMainLooper())
    @Volatile private var failureReported = false

    /** Chamado na thread principal, uma vez por sequência de falhas de gravação. */
    var onSaveFailed: ((Throwable) -> Unit)? = null

    val recoveryPending: Boolean get() = repo.recoveryPending

    fun load(): LoadResult = repo.load()

    fun save(p: Progress) {
        if (repo.recoveryPending) return
        if (!snapshot(p)) return
        io.execute { flushPending() }
    }

    /** Grava já (onPause). Espera até 2 s; se estourar, a gravação segue na thread de E/S. */
    fun saveNow(p: Progress) {
        if (repo.recoveryPending) return
        if (!snapshot(p)) return
        try { io.submit { flushPending() }.get(2, TimeUnit.SECONDS) } catch (e: Exception) { Log.w(TAG, "saveNow demorou: ${e.message}") }
    }

    private fun snapshot(p: Progress): Boolean = try {
        pending.set(SaveCodec.encode(p, BuildConfig.VERSION_NAME))
        true
    } catch (e: Exception) {
        reportFailure(e)
        false
    }

    private fun flushPending() {
        val text = pending.getAndSet(null) ?: return
        when (val r = repo.persist(text)) {
            PersistResult.Written -> failureReported = false
            PersistResult.Blocked -> Unit
            is PersistResult.Failed -> reportFailure(r.error)
        }
    }

    private fun reportFailure(e: Throwable) {
        Log.w(TAG, "falha ao gravar o save", e)
        if (failureReported) return
        failureReported = true
        main.post { onSaveFailed?.invoke(e) }
    }

    // ---- recuperação (RecoveryScreen / Ajustes) ----
    fun startNewGame(): Progress = repo.startNewGame()
    fun restorable(): List<BackupInfo> = repo.listRestorable()
    fun restore(b: BackupInfo): Progress = repo.restore(b)

    /** Copia os arquivos do save para uma pasta legível por adb/gerenciador de arquivos. @return a pasta ou null. */
    fun exportForSupport(): File? {
        val base = app.getExternalFilesDir("save-recovery") ?: return null
        val target = File(base, "save-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()))
        return if (repo.exportTo(target) > 0) target else null
    }

    private companion object {
        const val TAG = "SaveStore"
        const val LEGACY_PREFS = "zombie_zone_native"
        const val LEGACY_KEY = "progress"
    }
}
