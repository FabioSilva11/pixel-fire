package com.fabiosilva.ecosdeeldra.data.save

import com.fabiosilva.ecosdeeldra.data.Progress
import java.io.File
import java.io.IOException

internal sealed interface Eval {
    data object Missing : Eval
    data class Unreadable(val error: Throwable) : Eval
    data class Invalid(val reason: String) : Eval
    data class Valid(val ok: DecodeOutcome.Ok, val text: String) : Eval
}

class SaveRepository(
    private val files: SaveFiles,
    /** Leitor do SharedPreferences antigo (injetado pela fachada Android; nos testes é uma lambda). */
    private val legacy: () -> String? = { null },
    private val writtenBy: String = "",
    private val clock: () -> Long = { System.currentTimeMillis() },
    private val log: (String) -> Unit = {},
) {
    @Volatile private var blocked = false
    private var primaryKnownValid = false
    private var sessionChecked = false
    private var lastRotationAt = 0L

    /** true enquanto a recuperação explícita não foi resolvida: toda gravação é recusada. */
    val recoveryPending: Boolean get() = blocked

    // ------------------------------------------------------------------ carga

    @Synchronized
    fun load(): LoadResult {
        blocked = false
        primaryKnownValid = false
        sessionChecked = false

        val primary = eval(files.primary)
        if (primary is Eval.Valid) return accept(primary, files.primary, Origin.PRIMARY, primary)

        val tmp = eval(files.tmp)
        val bak1 = eval(files.bak1)
        val bak2 = eval(files.bak2)
        val chain = listOf(
            Triple(Origin.INTERRUPTED_WRITE, files.tmp, tmp),
            Triple(Origin.BACKUP_1, files.bak1, bak1),
            Triple(Origin.BACKUP_2, files.bak2, bak2),
        )
        for ((origin, file, ev) in chain) {
            if (ev is Eval.Valid) return accept(ev, file, origin, primary)
        }
        if (primary is Eval.Missing && tmp is Eval.Missing && bak1 is Eval.Missing && bak2 is Eval.Missing) return loadLegacyOrFresh()

        blocked = true
        return LoadResult.NeedsRecovery(
            report(
                listOf(
                    Triple("Save principal", files.primary, primary),
                    Triple("Gravação interrompida", files.tmp, tmp),
                    Triple("Cópia de segurança 1", files.bak1, bak1),
                    Triple("Cópia de segurança 2", files.bak2, bak2),
                ),
            ),
        )
    }

    private fun accept(v: Eval.Valid, file: File, origin: Origin, primary: Eval): LoadResult.Loaded {
        val notices = mutableListOf<SaveNotice>()
        val found = v.ok.schemaFound
        if (found > SaveCodec.SCHEMA_VERSION) {
            files.keepOnce("save.newer-v$found.json", v.text)
            notices += SaveNotice.NewerSchema(found)
        } else if (found < SaveCodec.SCHEMA_VERSION) {
            files.keepOnce("save.pre-v$found.json", v.text)            // snapshot antes de migrar; nunca sobrescrito
        }
        if (v.ok.warnings.isNotEmpty()) {
            files.quarantine(v.text, "partial")
            notices += SaveNotice.SectionsReset(v.ok.warnings)
        }
        if (origin == Origin.PRIMARY) {
            primaryKnownValid = true
        } else if (!(origin == Origin.INTERRUPTED_WRITE && primary is Eval.Missing)) {
            notices += SaveNotice.RestoredFromBackup(origin, file.lastModified(), SaveCodec.summarize(v.ok.progress))
        }
        return LoadResult.Loaded(v.ok.progress, origin, notices)
    }

    private fun loadLegacyOrFresh(): LoadResult {
        val raw = try { legacy() } catch (e: Exception) { null }
        if (raw.isNullOrBlank()) return LoadResult.Fresh
        files.keepOnce("save.legacy-prefs.json", raw)
        return when (val d = SaveCodec.decode(raw)) {
            is DecodeOutcome.Ok -> {
                val progress = d.progress
                val r = persist(SaveCodec.encode(progress, writtenBy))           // cria o primário já no formato novo
                if (r is PersistResult.Failed) log("migração do prefs não gravou: ${r.error.message}")
                LoadResult.Loaded(progress, Origin.LEGACY_PREFS, emptyList())
            }
            is DecodeOutcome.Invalid -> {
                blocked = true
                LoadResult.NeedsRecovery(
                    RecoveryReport(
                        listOf(RecoveryCandidate("Save antigo (preferências)", "save.legacy-prefs.json", CandidateStatus.INVALID, d.reason, raw.length.toLong(), 0L)),
                        readErrors = false,
                        restorable = emptyList(),
                    ),
                )
            }
        }
    }

    private fun eval(f: File): Eval = when (val r = files.read(f)) {
        FileRead.Missing -> Eval.Missing
        is FileRead.Failed -> Eval.Unreadable(r.error)
        is FileRead.TooBig -> Eval.Invalid("arquivo grande demais (${r.bytes} bytes)")
        is FileRead.Text -> when (val d = SaveCodec.decode(r.text)) {
            is DecodeOutcome.Ok -> Eval.Valid(d, r.text)
            is DecodeOutcome.Invalid -> Eval.Invalid(d.reason)
        }
    }

    private fun report(parts: List<Triple<String, File, Eval>>): RecoveryReport {
        val candidates = parts.mapNotNull { (label, f, ev) ->
            when (ev) {
                Eval.Missing, is Eval.Valid -> null
                is Eval.Unreadable -> RecoveryCandidate(label, f.name, CandidateStatus.UNREADABLE, ev.error.message ?: ev.error.javaClass.simpleName, f.length(), f.lastModified())
                is Eval.Invalid -> RecoveryCandidate(label, f.name, CandidateStatus.INVALID, ev.reason, f.length(), f.lastModified())
            }
        }
        return RecoveryReport(candidates, candidates.any { it.status == CandidateStatus.UNREADABLE }, listRestorable())
    }

    // ------------------------------------------------------------------ gravação

    @Synchronized
    fun persist(text: String, forceRotate: Boolean = false): PersistResult {
        if (blocked) return PersistResult.Blocked
        return try {
            val check = SaveCodec.decode(text)
            if (check is DecodeOutcome.Invalid) throw SaveFormatException("o app gerou um save ilegível: ${check.reason}")
            files.writeTmp(text)
            if (primaryKnownValid) {
                try { maybeRotate(forceRotate) } catch (e: Exception) { log("backup rotativo falhou: ${e.message}") }
            } else {
                preserveCurrentPrimary()
            }
            files.promoteTmp()
            primaryKnownValid = true
            PersistResult.Written
        } catch (e: Exception) {
            PersistResult.Failed(e)
        }
    }

    /** Nunca sobrescreve um arquivo que não sabemos ser válido sem antes guardá-lo. */
    private fun preserveCurrentPrimary() {
        if (!files.primary.exists()) return
        when (val r = files.read(files.primary)) {
            is FileRead.Text -> files.quarantine(r.text, "corrupt")
            FileRead.Missing -> Unit
            is FileRead.TooBig, is FileRead.Failed -> files.setAside(files.primary, "unreadable", clock())
        }
    }

    private fun maybeRotate(force: Boolean) {
        val now = clock()
        val due = force || !sessionChecked || now - lastRotationAt >= BACKUP_MIN_INTERVAL_MS
        if (!due || !files.primary.exists()) return
        sessionChecked = true
        lastRotationAt = now
        val current = files.primary.readBytes()
        val b1 = if (files.bak1.exists()) files.bak1.readBytes() else null
        if (b1 != null && b1.contentEquals(current)) return
        rotate()
    }

    private fun rotate() {
        when (val r = files.read(files.bak1)) {
            is FileRead.Text -> {
                if (SaveCodec.decode(r.text) is DecodeOutcome.Ok) files.moveOver(files.bak1, files.bak2)   // bak2 := bak1 (válido)
                else files.quarantine(r.text, "corrupt")                                                    // bak1 ruim: bak2 fica intacto
            }
            FileRead.Missing -> Unit
            is FileRead.TooBig, is FileRead.Failed -> return                                                // não destrói o que não consegue ler
        }
        files.copyAtomic(files.primary, files.bak1)
    }

    // ------------------------------------------------------------------ recuperação

    /** Itens válidos restauráveis: bak1, bak2, snapshots pre-v*, save antigo do SharedPreferences. */
    @Synchronized
    fun listRestorable(): List<BackupInfo> {
        val slots = mutableListOf("Cópia de segurança 1" to files.bak1, "Cópia de segurança 2" to files.bak2)
        files.dir.listFiles { f -> f.name.startsWith("save.pre-v") && f.name.endsWith(".json") }
            ?.sortedByDescending { it.name }
            ?.forEach { slots += "Antes da atualização do formato (${it.name.removePrefix("save.pre-").removeSuffix(".json")})" to it }
        slots += "Save antigo (antes da migração)" to File(files.dir, "save.legacy-prefs.json")
        return slots.mapNotNull { (label, f) ->
            val ev = eval(f)
            if (ev is Eval.Valid) BackupInfo(label, f, f.lastModified(), SaveCodec.summarize(ev.ok.progress)) else null
        }
    }

    /** Escolha explícita do usuário: começar do zero. Nada é apagado (o ruim vai para quarentena/renomeado). */
    @Synchronized
    fun startNewGame(): Progress {
        val fresh = Progress()
        blocked = false
        primaryKnownValid = false
        files.tmp.delete()
        val r = persist(SaveCodec.encode(fresh, writtenBy))
        if (r is PersistResult.Failed) { blocked = true; throw r.error }
        return fresh
    }

    /** Restaura um item de [listRestorable]. Se o primário atual é válido, ele vira bak1 (reversível). */
    @Synchronized
    fun restore(info: BackupInfo): Progress {
        val text = (files.read(info.file) as? FileRead.Text)?.text ?: throw IOException("não foi possível ler ${info.file.name}")
        val ok = SaveCodec.decode(text) as? DecodeOutcome.Ok ?: throw SaveFormatException("a cópia escolhida não é válida")
        val wasBlocked = blocked
        blocked = false
        val r = persist(SaveCodec.encode(ok.progress, writtenBy), forceRotate = true)
        if (r is PersistResult.Failed) { blocked = wasBlocked; throw r.error }
        return ok.progress
    }

    /** Copia todos os arquivos do save para [target] (suporte). @return quantidade copiada. */
    @Synchronized
    fun exportTo(target: File): Int = files.copyAllTo(target)

    companion object {
        /** Intervalo mínimo entre rotações de backup dentro de uma sessão (a 1ª gravação da sessão sempre roda). */
        const val BACKUP_MIN_INTERVAL_MS = 15 * 60_000L
    }
}
